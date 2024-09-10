package kubernetesmetav2

import (
	"strconv"
	"time"

	batch "k8s.io/api/batch/v1" //nolint:typecheck

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processJobEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*batch.Job); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("status", obj.Status.String())
		containerInfos := []map[string]string{}
		for _, container := range obj.Spec.Template.Spec.Containers {
			containerInfo := map[string]string{
				"name":  container.Name,
				"image": container.Image,
			}
			containerInfos = append(containerInfos, containerInfo)
		}
		log.Contents.Add("containers", m.processEntityJSONArray(containerInfos))
		log.Contents.Add("suspend", strconv.FormatBool(*obj.Spec.Suspend))
		log.Contents.Add("backoff_limit", strconv.FormatInt(int64(*obj.Spec.BackoffLimit), 10))
		log.Contents.Add("completion", strconv.FormatInt(int64(*obj.Spec.Completions), 10))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processCronJobEntity(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*batch.CronJob); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		log.Timestamp = uint64(time.Now().Unix())
		m.processEntityCommonPart(log.Contents, obj.Kind, obj.Namespace, obj.Name, method, data.FirstObservedTime, data.LastObservedTime, obj.CreationTimestamp)

		// custom fields
		log.Contents.Add("api_version", obj.APIVersion)
		log.Contents.Add("namespace", obj.Namespace)
		log.Contents.Add("labels", m.processEntityJSONObject(obj.Labels))
		log.Contents.Add("annotations", m.processEntityJSONObject(obj.Annotations))
		log.Contents.Add("schedule", obj.Spec.Schedule)
		log.Contents.Add("suspend", strconv.FormatBool(*obj.Spec.Suspend))
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processJobCronJobLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.JobCronJob); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Job.Kind, obj.Job.Namespace, obj.Job.Name, obj.CronJob.Kind, obj.CronJob.Namespace, obj.CronJob.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}

func (m *metaCollector) processPodJobLink(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent {
	if obj, ok := data.Raw.(*k8smeta.PodJob); ok {
		log := &models.Log{}
		log.Contents = models.NewLogContents()
		m.processEntityLinkCommonPart(log.Contents, obj.Pod.Kind, obj.Pod.Namespace, obj.Pod.Name, obj.Job.Kind, obj.Job.Namespace, obj.Job.Name, method, data.FirstObservedTime, data.LastObservedTime)
		log.Contents.Add(entityLinkRelationTypeFieldName, "related_to")
		log.Timestamp = uint64(time.Now().Unix())
		return []models.PipelineEvent{log}
	}
	return nil
}
