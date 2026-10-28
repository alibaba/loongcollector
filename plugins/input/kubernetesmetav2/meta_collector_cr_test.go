package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func testWorkflowUnstructured(t *testing.T) *unstructured.Unstructured {
	t.Helper()
	u := &unstructured.Unstructured{}
	u.SetAPIVersion("argoproj.io/v1alpha1")
	u.SetKind("Workflow")
	u.SetNamespace("default")
	u.SetName("wf-entity")
	u.SetLabels(map[string]string{"keep": "yes", "drop": "no"})
	u.SetAnnotations(map[string]string{"anno": "v"})
	require.NoError(t, unstructured.SetNestedField(u.Object, "Running", "status", "phase"))
	return u
}

func TestValidateCustomResourceEntityTypeUniqueness(t *testing.T) {
	seen := make(map[string]struct{})
	cfg := k8smeta.CustomResourceCollectorConfig{EntityType: "customresource/argoproj.io/workflow"}
	require.NoError(t, validateCustomResourceEntityTypeUniqueness(cfg, seen))
	err := validateCustomResourceEntityTypeUniqueness(cfg, seen)
	require.Error(t, err)
	assert.Contains(t, err.Error(), "duplicated CustomResources EntityType")
}

func TestPrepareNormalizedCustomResourceConfigsReturnErrorOnDuplicateEntityType(t *testing.T) {
	cfg1 := k8smeta.CustomResourceCollectorConfig{
		EntityType:    "customresource/argoproj.io/workflow",
		APIGroup:      "argoproj.io",
		APIVersion:    "v1alpha1",
		Resource:      "workflows",
		Kind:          "Workflow",
		PodLink:       &k8smeta.PodToCustomResourceLinkConfig{},
		CollectEntity: true,
	}
	cfg2 := k8smeta.CustomResourceCollectorConfig{
		EntityType: "customresource/argoproj.io/workflow",
		APIGroup:   "argoproj.io",
		APIVersion: "v1alpha1",
		Resource:   "workflows",
		Kind:       "Workflow",
	}

	normalized, err := prepareNormalizedCustomResourceConfigs([]k8smeta.CustomResourceCollectorConfig{cfg1, cfg2})
	require.Error(t, err)
	assert.Nil(t, normalized)
	assert.Contains(t, err.Error(), "duplicated CustomResources EntityType")
}

func TestPrepareNormalizedCustomResourceConfigsSkipsInvalidEntries(t *testing.T) {
	invalid := k8smeta.CustomResourceCollectorConfig{
		EntityType: "customresource/argoproj.io/workflow",
	}
	valid := k8smeta.CustomResourceCollectorConfig{
		EntityType: "customresource/argoproj.io/rollout",
		APIGroup:   "argoproj.io",
		APIVersion: "v1alpha1",
		Resource:   "rollouts",
		Kind:       "Rollout",
		PodLink:    &k8smeta.PodToCustomResourceLinkConfig{},
	}

	normalized, err := prepareNormalizedCustomResourceConfigs([]k8smeta.CustomResourceCollectorConfig{invalid, valid})
	require.NoError(t, err)
	require.Len(t, normalized, 1)
	assert.Equal(t, "customresource/argoproj.io/rollout", normalized[0].EntityType)
	// Normalize should fill PodLink defaults from CR config.
	assert.Equal(t, "Rollout", normalized[0].PodLink.OwnerKind)
	assert.Equal(t, "argoproj.io", normalized[0].PodLink.OwnerAPIGroupContains)
}

func TestProcessPodCustomResourceLink(t *testing.T) {
	entityType := "argo.workflow"
	linkRT := k8smeta.POD + k8smeta.LINK_SPLIT_CHARACTER + entityType

	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{Name: "pod1", Namespace: "default"},
	}
	cr := &unstructured.Unstructured{}
	cr.SetAPIVersion("argoproj.io/v1alpha1")
	cr.SetKind("Workflow")
	cr.SetNamespace("default")
	cr.SetName("wf1")

	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			entityType: {
				EntityType:         entityType,
				Entity2PodRelation: "contains",
			},
		},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: linkRT,
		Raw: &k8smeta.PodCustomResource{
			Pod: pod,
			CR:  cr,
		},
		FirstObservedTime: 1,
		LastObservedTime:  2,
	}
	events := m.processPodCustomResourceLink(data, "update")
	require.Len(t, events, 1)
	log, ok := events[0].(*models.Log)
	require.True(t, ok)
	rel := log.Contents.Get(entityLinkRelationTypeFieldName)
	assert.Equal(t, "contains", rel)
}

func TestProcessNamespaceCustomResourceLink(t *testing.T) {
	entityType := "argo.workflow"
	linkRT := entityType + k8smeta.LINK_SPLIT_CHARACTER + k8smeta.NAMESPACE

	ns := &corev1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: "production"}}
	cr := &unstructured.Unstructured{}
	cr.SetAPIVersion("argoproj.io/v1alpha1")
	cr.SetKind("Workflow")
	cr.SetNamespace("production")
	cr.SetName("wf-ns")

	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			entityType: {
				EntityType:               entityType,
				Namespace2EntityRelation: "contains",
			},
		},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: linkRT,
		Raw: &k8smeta.NamespaceCustomResource{
			Namespace: ns,
			CR:        cr,
		},
		FirstObservedTime: 10,
		LastObservedTime:  20,
	}
	events := m.processNamespaceCustomResourceLink(data, "update")
	require.Len(t, events, 1)
	log, ok := events[0].(*models.Log)
	require.True(t, ok)
	assert.Equal(t, "contains", log.Contents.Get(entityLinkRelationTypeFieldName))
}

func TestProcessNamespaceCustomResourceLinkSkipsWhenRelationUnset(t *testing.T) {
	entityType := "argo.workflow"
	linkRT := entityType + k8smeta.LINK_SPLIT_CHARACTER + k8smeta.NAMESPACE
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			entityType: {EntityType: entityType},
		},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: linkRT,
		Raw: &k8smeta.NamespaceCustomResource{
			Namespace: &corev1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: "x"}},
			CR:        &unstructured.Unstructured{},
		},
	}
	assert.Nil(t, m.processNamespaceCustomResourceLink(data, "update"))
}

func TestProcessNamespaceCustomResourceLinkRejectsNonNamespaceLinkResourceType(t *testing.T) {
	m := &metaCollector{
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			"argo.workflow": {EntityType: "argo.workflow", Namespace2EntityRelation: "contains"},
		},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: "argo.workflow",
		Raw: &k8smeta.NamespaceCustomResource{
			Namespace: &corev1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: "default"}},
			CR:        &unstructured.Unstructured{},
		},
	}
	assert.Nil(t, m.processNamespaceCustomResourceLink(data, "update"))
}

func TestProcessPodCustomResourceLinkRejectsNonPodCRResourceType(t *testing.T) {
	m := &metaCollector{
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			"argo.workflow": {EntityType: "argo.workflow", Entity2PodRelation: "contains"},
		},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: "argo.workflow",
		Raw: &k8smeta.PodCustomResource{
			Pod: &corev1.Pod{},
			CR:  &unstructured.Unstructured{},
		},
	}
	assert.Nil(t, m.processPodCustomResourceLink(data, "update"))
}

func TestProcessPodCustomResourceLinkWrongRawType(t *testing.T) {
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{},
		crConfigs:      map[string]k8smeta.CustomResourceCollectorConfig{"argo.workflow": {}},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: k8smeta.POD + k8smeta.LINK_SPLIT_CHARACTER + "argo.workflow",
		Raw:          &corev1.Pod{},
	}
	assert.Nil(t, m.processPodCustomResourceLink(data, "update"))
}

func stringField(t *testing.T, log *models.Log, key string) string {
	t.Helper()
	v := log.Contents.Get(key)
	require.NotNil(t, v)
	s, ok := v.(string)
	require.True(t, ok)
	return s
}

func TestProcessCustomResourceEntityUnknownResourceType(t *testing.T) {
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{Interval: 10},
		crConfigs:      map[string]k8smeta.CustomResourceCollectorConfig{},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType: "unknown.type",
		Raw:          testWorkflowUnstructured(t),
	}
	assert.Nil(t, m.processCustomResourceEntity(data, "update"))
}

func TestProcessCustomResourceEntityWrongRawType(t *testing.T) {
	entityType := "argo.workflow"
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{Interval: 10},
		crConfigs: map[string]k8smeta.CustomResourceCollectorConfig{
			entityType: {EntityType: entityType, Kind: "Workflow"},
		},
	}
	data := &k8smeta.ObjectWrapper{ResourceType: entityType, Raw: &corev1.Pod{}}
	assert.Nil(t, m.processCustomResourceEntity(data, "update"))
}

func TestProcessCustomResourceEntityCoreFields(t *testing.T) {
	entityType := "argo.workflow"
	cfg := k8smeta.CustomResourceCollectorConfig{EntityType: entityType, Kind: "Workflow"}
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{Interval: 10},
		crConfigs:      map[string]k8smeta.CustomResourceCollectorConfig{entityType: cfg},
	}
	data := &k8smeta.ObjectWrapper{
		ResourceType:      entityType,
		Raw:               testWorkflowUnstructured(t),
		FirstObservedTime: 100,
		LastObservedTime:  200,
	}
	events := m.processCustomResourceEntity(data, "update")
	require.Len(t, events, 1)
	log := events[0].(*models.Log)
	assert.Equal(t, "Workflow", stringField(t, log, entityKindFieldName))
	assert.Equal(t, "argoproj.io/v1alpha1", stringField(t, log, "api_version"))
	assert.Equal(t, "default", stringField(t, log, "namespace"))
	assert.False(t, log.Contents.Contains("labels"))
	assert.False(t, log.Contents.Contains("annotations"))
	assert.False(t, log.Contents.Contains("status"))
}

func TestProcessCustomResourceEntityEnableLabels(t *testing.T) {
	entityType := "argo.workflow"
	cfg := k8smeta.CustomResourceCollectorConfig{EntityType: entityType, Kind: "Workflow", EnableLabels: true}
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{Interval: 10},
		crConfigs:      map[string]k8smeta.CustomResourceCollectorConfig{entityType: cfg},
	}
	data := &k8smeta.ObjectWrapper{ResourceType: entityType, Raw: testWorkflowUnstructured(t)}
	events := m.processCustomResourceEntity(data, "update")
	require.Len(t, events, 1)
	log := events[0].(*models.Log)
	labels := stringField(t, log, "labels")
	assert.Contains(t, labels, "keep")
	assert.Contains(t, labels, "drop")
}

func TestProcessCustomResourceEntityEnableAnnotations(t *testing.T) {
	entityType := "argo.workflow"
	cfg := k8smeta.CustomResourceCollectorConfig{EntityType: entityType, Kind: "Workflow", EnableAnnotations: true}
	m := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{Interval: 10},
		crConfigs:      map[string]k8smeta.CustomResourceCollectorConfig{entityType: cfg},
	}
	data := &k8smeta.ObjectWrapper{ResourceType: entityType, Raw: testWorkflowUnstructured(t)}
	events := m.processCustomResourceEntity(data, "update")
	require.Len(t, events, 1)
	log := events[0].(*models.Log)
	annos := stringField(t, log, "annotations")
	assert.Contains(t, annos, "anno")
}
