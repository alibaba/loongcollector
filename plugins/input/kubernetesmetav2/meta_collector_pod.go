package kubernetesmetav2

import (
	"strconv"
	"time"

	v1 "k8s.io/api/core/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processPodEntity(data *k8smeta.ObjectWrapper, method string) models.PipelineEvent {
	if obj, ok := data.Raw.(*v1.Pod); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Contents.Add(entityDomainFieldName, "k8s")
		log.Contents.Add(entityTypeFieldName, "pod")
		log.Contents.Add(entityIDFieldName, genKeyByPod(obj))
		log.Contents.Add(entityMethodFieldName, method)

		log.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(data.FirstObservedTime, 10))
		log.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(data.LastObservedTime, 10))
		log.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval), 10))

		log.Contents.Add(entityNamespaceFieldName, obj.Namespace)
		log.Contents.Add(entityNameFieldName, obj.Name)
		log.Contents.Add(entityCategoryFieldName, defaultEntityCategory)
		log.Timestamp = uint64(time.Now().Unix())

		// custom fields
		log.Contents.Add("apiVersion", obj.APIVersion)
		log.Contents.Add("kind", "pod")
		log.Contents.Add("name", obj.Name)
		log.Contents.Add("namespace", obj.Namespace)
		for k, v := range obj.Labels {
			log.Contents.Add("_label_"+k, v)
		}
		for k, v := range obj.Annotations {
			log.Contents.Add("_annotations_"+k, v)
		}
		for i, container := range obj.Spec.Containers {
			log.Contents.Add("container_"+strconv.FormatInt(int64(i), 10)+"_name", container.Name)
			log.Contents.Add("container_"+strconv.FormatInt(int64(i), 10)+"_image", container.Image)
		}
		return log
	}
	return nil
}

func (m *metaCollector) processPodReplicasetLink(data *k8smeta.ObjectWrapper, method string) models.PipelineEvent {
	if obj, ok := data.Raw.(*v1.Pod); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Contents.Add(entityLinkSrcDomainFieldName, "k8s")
		log.Contents.Add(entityLinkSrcEntityTypeFieldName, "pod")
		log.Contents.Add(entityLinkSrcEntityIDFieldName, genKeyByPod(obj))

		if len(obj.OwnerReferences) > 0 {
			ownerReferences := obj.OwnerReferences[0]
			log.Contents.Add(entityLinkDestDomainFieldName, "k8s")
			log.Contents.Add(entityLinkDestEntityTypeFieldName, "replicaset")
			log.Contents.Add(entityLinkDestEntityIDFieldName, genKey(obj.Namespace, ownerReferences.Kind, ownerReferences.Name))
		} else {
			// 数据不完整就没有意义
			return nil
		}

		log.Contents.Add(entityLinkRelationTypeFieldName, "contain")

		switch method {
		case "create", "update":
			log.Contents.Add(entityMethodFieldName, "update")
		case "delete":
			log.Contents.Add(entityMethodFieldName, method)
		default:
			// 数据不完整就没有意义
			return nil
		}

		log.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(data.FirstObservedTime, 10))
		log.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(data.LastObservedTime, 10))
		log.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval), 10))
		log.Contents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
		log.Timestamp = uint64(time.Now().Unix())
		return log
	}
	return nil
}

func (m *metaCollector) processPodServiceLink(data *k8smeta.ObjectWrapper, method string) models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.PodService); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Contents.Add(entityLinkSrcDomainFieldName, "k8s")
		log.Contents.Add(entityLinkSrcEntityTypeFieldName, "pod")
		log.Contents.Add(entityLinkSrcEntityIDFieldName, genKeyByPod(obj.Pod))

		log.Contents.Add(entityLinkDestDomainFieldName, "k8s")
		log.Contents.Add(entityLinkDestEntityTypeFieldName, "service")
		log.Contents.Add(entityLinkDestEntityIDFieldName, genKeyByService(obj.Service))

		log.Contents.Add(entityLinkRelationTypeFieldName, "link")

		switch method {
		case "create", "update":
			log.Contents.Add(entityMethodFieldName, "update")
		case "delete":
			log.Contents.Add(entityMethodFieldName, method)
		default:
			// 数据不完整就没有意义
			return nil
		}

		log.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(data.FirstObservedTime, 10))
		log.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(data.LastObservedTime, 10))
		log.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval), 10))
		log.Contents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
		log.Timestamp = uint64(time.Now().Unix())
		return log
	}
	return nil
}
