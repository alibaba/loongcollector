package kubernetesmetav2

import (
	"strings"
	"time"

	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func filterStringMapByAllowList(m map[string]string, allow []string) map[string]string {
	if len(allow) == 0 || len(m) == 0 {
		return nil
	}
	out := make(map[string]string)
	for _, k := range allow {
		if k == "" {
			continue
		}
		if v, ok := m[k]; ok {
			out[k] = v
		}
	}
	if len(out) == 0 {
		return nil
	}
	return out
}

func pickUnstructuredFieldCopy(obj map[string]interface{}, paths []string) map[string]interface{} {
	if len(paths) == 0 || obj == nil {
		return nil
	}
	out := make(map[string]interface{})
	for _, p := range paths {
		if p == "" {
			continue
		}
		parts := strings.Split(p, ".")
		if v, found, err := unstructured.NestedFieldCopy(obj, parts...); found && err == nil {
			out[p] = v
		}
	}
	if len(out) == 0 {
		return nil
	}
	return out
}

func (m *metaCollector) processCustomResourceEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	obj, ok := data.Raw.(*unstructured.Unstructured)
	if !ok {
		return nil
	}
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	log.Timestamp = uint64(time.Now().Unix())
	kindKey := k8smeta.CUSTOM_RESOURCE_ARGO_WORKFLOW
	m.processEntityCommonPart(log.Contents, kindKey, obj.GetNamespace(), obj.GetName(), method, data.FirstObservedTime, data.LastObservedTime, obj.GetCreationTimestamp())
	log.Contents.Add(entityKindFieldName, k8smeta.ArgoWorkflowKind)
	log.Contents.Add("api_version", obj.GetAPIVersion())
	log.Contents.Add("namespace", obj.GetNamespace())

	// Labels/annotations: same switches as built-in entities (EnableLabels / EnableAnnotations).
	// If CustomResourceWorkflow*AllowList is non-empty, only those keys are emitted (subset mode; does not require Enable*).
	if len(m.serviceK8sMeta.CustomResourceWorkflowLabelAllowList) > 0 {
		if labels := filterStringMapByAllowList(obj.GetLabels(), m.serviceK8sMeta.CustomResourceWorkflowLabelAllowList); labels != nil {
			log.Contents.Add("labels", m.processEntityJSONObject(labels))
		}
	} else if m.serviceK8sMeta.EnableLabels {
		log.Contents.Add("labels", m.processEntityJSONObject(obj.GetLabels()))
	}
	if len(m.serviceK8sMeta.CustomResourceWorkflowAnnotationAllowList) > 0 {
		if annos := filterStringMapByAllowList(obj.GetAnnotations(), m.serviceK8sMeta.CustomResourceWorkflowAnnotationAllowList); annos != nil {
			log.Contents.Add("annotations", m.processEntityJSONObject(annos))
		}
	} else if m.serviceK8sMeta.EnableAnnotations {
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.GetAnnotations()))
	}
	if statusObj := pickUnstructuredFieldCopy(obj.Object, m.serviceK8sMeta.CustomResourceWorkflowStatusPathAllowList); statusObj != nil {
		log.Contents.Add("status", m.processEntityJSONObject(statusObj))
	}
	return []models.PipelineEvent{log}
}

func (m *metaCollector) processPodArgoWorkflowLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	obj, ok := data.Raw.(*k8smeta.PodArgoWorkflow)
	if !ok {
		return nil
	}
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	m.processEntityLinkCommonPart(log.Contents, k8smeta.CUSTOM_RESOURCE_ARGO_WORKFLOW, obj.Workflow.GetNamespace(), obj.Workflow.GetName(),
		obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, method, data.FirstObservedTime, data.LastObservedTime)
	log.Contents.Add(entityLinkRelationTypeFieldName, m.serviceK8sMeta.Workflow2Pod)
	log.Timestamp = uint64(time.Now().Unix())
	return []models.PipelineEvent{log}
}
