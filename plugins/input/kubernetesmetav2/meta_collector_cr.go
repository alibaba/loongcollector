package kubernetesmetav2

import (
	"strings"
	"time"

	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

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

	if cfg.EnableLabels {
		log.Contents.Add("labels", m.processEntityJSONObject(obj.GetLabels()))
	}
	if cfg.EnableAnnotations {
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.GetAnnotations()))
	}
	return []models.PipelineEvent{log}
}

func (m *metaCollector) processNamespaceCustomResourceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	obj, ok := data.Raw.(*k8smeta.NamespaceCustomResource)
	if !ok {
		return nil
	}
	nsLinkSuffix := k8smeta.LINK_SPLIT_CHARACTER + k8smeta.NAMESPACE
	if !strings.HasSuffix(data.ResourceType, nsLinkSuffix) {
		return nil
	}
	entityType := strings.TrimSuffix(data.ResourceType, nsLinkSuffix)
	if entityType == "" {
		return nil
	}
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
	podCRPrefix := k8smeta.POD + k8smeta.LINK_SPLIT_CHARACTER
	if !strings.HasPrefix(data.ResourceType, podCRPrefix) {
		return nil
	}
	entityType := strings.TrimPrefix(data.ResourceType, podCRPrefix)
	if entityType == "" {
		return nil
	}
	cfg, ok := m.crConfigs[entityType]
	if !ok || cfg.Entity2PodRelation == "" {
		return nil
	}
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	m.processEntityLinkCommonPart(log.Contents, entityType, obj.CR.GetNamespace(), obj.CR.GetName(),
		obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, method, data.FirstObservedTime, data.LastObservedTime)
	log.Contents.Add(entityLinkRelationTypeFieldName, cfg.Entity2PodRelation)
	log.Timestamp = uint64(time.Now().Unix())
	return []models.PipelineEvent{log}
}
