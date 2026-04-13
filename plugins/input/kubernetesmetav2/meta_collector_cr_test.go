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
