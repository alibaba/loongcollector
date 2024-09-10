package kubernetesmetav2

import (
	"strconv"
	"time"

	app "k8s.io/api/apps/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processDeploymentEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*app.Deployment); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("match_labels", m.processEntityJSONObject(obj.Spec.Selector.MatchLabels))
		log.Contents.Add("replicas", strconv.FormatInt(int64(*obj.Spec.Replicas), 10))
		log.Contents.Add("ready_replicas", strconv.FormatInt(int64(obj.Status.ReadyReplicas), 10))
		containerInfos := []map[string]string{}
		for _, container := range obj.Spec.Template.Spec.Containers {
			containerInfo := map[string]string{
				"name":  container.Name,
				"image": container.Image,
			}
			containerInfos = append(containerInfos, containerInfo)
		}
		log.Contents.Add("containers", m.processEntityJSONArray(containerInfos))

		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processDaemonSetEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*app.DaemonSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("match_labels", m.processEntityJSONObject(obj.Spec.Selector.MatchLabels))
		containerInfos := []map[string]string{}
		for _, container := range obj.Spec.Template.Spec.Containers {
			containerInfo := map[string]string{
				"name":  container.Name,
				"image": container.Image,
			}
			containerInfos = append(containerInfos, containerInfo)
		}
		log.Contents.Add("containers", m.processEntityJSONArray(containerInfos))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processStatefulSetEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*app.StatefulSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("match_labels", m.processEntityJSONObject(obj.Spec.Selector.MatchLabels))
		log.Contents.Add("replicas", strconv.FormatInt(int64(*obj.Spec.Replicas), 10))
		containerInfos := []map[string]string{}
		for _, container := range obj.Spec.Template.Spec.Containers {
			containerInfo := map[string]string{
				"name":  container.Name,
				"image": container.Image,
			}
			containerInfos = append(containerInfos, containerInfo)
		}
		log.Contents.Add("containers", m.processEntityJSONArray(containerInfos))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processReplicaSetEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*app.ReplicaSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("match_labels", m.processEntityJSONObject(obj.Spec.Selector.MatchLabels))
		log.Contents.Add("replicas", strconv.FormatInt(int64(*obj.Spec.Replicas), 10))
		containerInfos := []map[string]string{}
		for _, container := range obj.Spec.Template.Spec.Containers {
			containerInfo := map[string]string{
				"name":  container.Name,
				"image": container.Image,
			}
			containerInfos = append(containerInfos, containerInfo)
		}
		log.Contents.Add("containers", m.processEntityJSONArray(containerInfos))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processReplicaSetDeploymentLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.ReplicaSetDeployment); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.ReplicaSet.Kind, obj.ReplicaSet.Namespace, obj.ReplicaSet.Name, obj.Deployment.Kind, obj.Deployment.Namespace, obj.Deployment.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processPodReplicaSetLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.PodReplicaSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, obj.ReplicaSet.Kind, obj.ReplicaSet.Namespace, obj.ReplicaSet.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processPodStatefulSetLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.PodStatefulSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, obj.StatefulSet.Kind, obj.StatefulSet.Namespace, obj.StatefulSet.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processPodDaemonSetLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.PodDaemonSet); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, obj.DaemonSet.Kind, obj.DaemonSet.Namespace, obj.DaemonSet.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}
