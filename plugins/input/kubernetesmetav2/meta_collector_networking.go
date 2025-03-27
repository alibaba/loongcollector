package kubernetesmetav2

import (
	"time"

	networking "k8s.io/api/networking/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processIngressEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*networking.Ingress); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processIngressServiceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.IngressService); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Ingress.Name, obj.Ingress.Kind, obj.Ingress.Namespace, obj.Service.Name, obj.Service.Kind, obj.Service.Namespace, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, m.serviceK8sMeta.Ingress2Service)
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processIngressNamespaceLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.IngressNamespace); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Namespace.Kind, obj.Namespace.Namespace, obj.Namespace.Name, obj.Ingress.Kind, obj.Ingress.Namespace, obj.Ingress.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, m.serviceK8sMeta.Namespace2Ingress)
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}
