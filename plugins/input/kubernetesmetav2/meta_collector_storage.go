package kubernetesmetav2

import (
	"time"

	storage "k8s.io/api/storage/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processStorageClassEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*storage.StorageClass); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("reclaim_policy", string(*obj.ReclaimPolicy))
		log.Contents.Add("volume_binding_mode", string(*obj.VolumeBindingMode))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processStorageClassNamespaceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.StorageClassNamespace); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Namespace.Kind, obj.Namespace.Namespace, obj.Namespace.Name, obj.StorageClass.Kind, obj.StorageClass.Namespace, obj.StorageClass.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, m.serviceK8sMeta.Namespace2StorageClass)
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

