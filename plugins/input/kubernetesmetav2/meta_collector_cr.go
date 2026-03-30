package kubernetesmetav2

import (
	"strings"
	"time"

	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) customResourceLabelAllowList(cfg k8smeta.CustomResourceCollectorConfig) []string {
	if len(cfg.LabelAllowList) > 0 {
		return cfg.LabelAllowList
	}
	return nil
}

func (m *metaCollector) customResourceAnnotationAllowList(cfg k8smeta.CustomResourceCollectorConfig) []string {
	if len(cfg.AnnotationAllowList) > 0 {
		return cfg.AnnotationAllowList
	}
	return nil
}

func (m *metaCollector) customResourceStatusPaths(cfg k8smeta.CustomResourceCollectorConfig) []string {
	if len(cfg.StatusPathAllowList) > 0 {
		return cfg.StatusPathAllowList
	}
	return nil
}

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
	cfg, ok := m.crConfigs[data.ResourceType]
	if !ok {
		return nil
	}
	obj, ok := data.Raw.(*unstructured.Unstructured)
	if !ok {
		return nil
	}
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	log.Timestamp = uint64(time.Now().Unix())
	kindKey := data.ResourceType
	m.processEntityCommonPart(log.Contents, kindKey, obj.GetNamespace(), obj.GetName(), method, data.FirstObservedTime, data.LastObservedTime, obj.GetCreationTimestamp())
	log.Contents.Add(entityKindFieldName, cfg.Kind)
	log.Contents.Add("api_version", obj.GetAPIVersion())
	log.Contents.Add("namespace", obj.GetNamespace())

	labelAllow := m.customResourceLabelAllowList(cfg)
	if len(labelAllow) > 0 {
		if labels := filterStringMapByAllowList(obj.GetLabels(), labelAllow); labels != nil {
			log.Contents.Add("labels", m.processEntityJSONObject(labels))
		}
	} else if cfg.EnableLabels {
		log.Contents.Add("labels", m.processEntityJSONObject(obj.GetLabels()))
	}
	annoAllow := m.customResourceAnnotationAllowList(cfg)
	if len(annoAllow) > 0 {
		if annos := filterStringMapByAllowList(obj.GetAnnotations(), annoAllow); annos != nil {
			log.Contents.Add("annotations", m.processEntityJSONObject(annos))
		}
	} else if cfg.EnableAnnotations {
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.GetAnnotations()))
	}
	if statusObj := pickUnstructuredFieldCopy(obj.Object, m.customResourceStatusPaths(cfg)); statusObj != nil {
		log.Contents.Add("status", m.processEntityJSONObject(statusObj))
	}
	return []models.PipelineEvent{log}
}

func (m *metaCollector) processNamespaceCustomResourceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	obj, ok := data.Raw.(*k8smeta.NamespaceCustomResource)
	if !ok {
		return nil
	}
	entityType := strings.TrimSuffix(data.ResourceType, k8smeta.LINK_SPLIT_CHARACTER+k8smeta.NAMESPACE)
	cfg := m.crConfigs[entityType]
	if cfg.EntityType == "" || cfg.Namespace2EntityRelation == "" {
		return nil
	}
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	m.processEntityLinkCommonPart(log.Contents, obj.Namespace.Kind, obj.Namespace.Namespace, obj.Namespace.Name,
		cfg.EntityType, obj.CR.GetNamespace(), obj.CR.GetName(), method, data.FirstObservedTime, data.LastObservedTime)
	log.Contents.Add(entityLinkRelationTypeFieldName, cfg.Namespace2EntityRelation)
	log.Timestamp = uint64(time.Now().Unix())
	return []models.PipelineEvent{log}
}

func (m *metaCollector) processPodCustomResourceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	obj, ok := data.Raw.(*k8smeta.PodCustomResource)
	if !ok {
		return nil
	}
	entityType := strings.TrimPrefix(data.ResourceType, k8smeta.POD+k8smeta.LINK_SPLIT_CHARACTER)
	cfg := m.crConfigs[entityType]
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	m.processEntityLinkCommonPart(log.Contents, entityType, obj.CR.GetNamespace(), obj.CR.GetName(),
		obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, method, data.FirstObservedTime, data.LastObservedTime)
	log.Contents.Add(entityLinkRelationTypeFieldName, cfg.Entity2PodRelation)
	log.Timestamp = uint64(time.Now().Unix())
	return []models.PipelineEvent{log}
}
