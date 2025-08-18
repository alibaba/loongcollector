package k8smeta

import (
	"context"

	"github.com/alibaba/ilogtail/pkg/logger"
	batch "k8s.io/api/batch/v1"
	batchv1beta1 "k8s.io/api/batch/v1beta1"
)

const (
	CronJobVersionV1      CronJobVersion = "v1"
	CronJobVersionV1Beta1 CronJobVersion = "v1beta1"
)

// GetCronJobVersion 检测 CronJob 对象的 API 版本
func GetCronJobVersion(obj interface{}) CronJobVersion {
	switch obj.(type) {
	case *batch.CronJob:
		return CronJobVersionV1
	case *batchv1beta1.CronJob:
		return CronJobVersionV1Beta1
	default:
		return ""
	}
}

// ConvertCronJobToV1 将任何版本的 CronJob 转换为 v1 版本
func ConvertCronJobToV1(obj interface{}) *batch.CronJob {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob
	case *batchv1beta1.CronJob:
		return convertV1Beta1ToV1(cronJob)
	default:
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "unsupported cronjob type", obj)
		return nil
	}
}

// convertV1Beta1ToV1 将 v1beta1 版本的 CronJob 转换为 v1 版本
func convertV1Beta1ToV1(cronJob *batchv1beta1.CronJob) *batch.CronJob {
	if cronJob == nil {
		return nil
	}

	v1CronJob := &batch.CronJob{
		ObjectMeta: cronJob.ObjectMeta,
		Spec: batch.CronJobSpec{
			Schedule:                cronJob.Spec.Schedule,
			TimeZone:                cronJob.Spec.TimeZone,
			StartingDeadlineSeconds: cronJob.Spec.StartingDeadlineSeconds,
			ConcurrencyPolicy:       batch.ConcurrencyPolicy(cronJob.Spec.ConcurrencyPolicy),
			Suspend:                 cronJob.Spec.Suspend,
			JobTemplate: batch.JobTemplateSpec{
				ObjectMeta: cronJob.Spec.JobTemplate.ObjectMeta,
				Spec:       cronJob.Spec.JobTemplate.Spec,
			},
			SuccessfulJobsHistoryLimit: cronJob.Spec.SuccessfulJobsHistoryLimit,
			FailedJobsHistoryLimit:     cronJob.Spec.FailedJobsHistoryLimit,
		},
		Status: batch.CronJobStatus{
			LastScheduleTime:   cronJob.Status.LastScheduleTime,
			LastSuccessfulTime: cronJob.Status.LastSuccessfulTime,
			Active:             cronJob.Status.Active,
		},
	}

	return v1CronJob
}

// IsCronJobObject 检查对象是否为 CronJob 类型
func IsCronJobObject(obj interface{}) bool {
	switch obj.(type) {
	case *batch.CronJob, *batchv1beta1.CronJob:
		return true
	default:
		return false
	}
}

// GetCronJobName 获取 CronJob 的名称，无论是什么版本
func GetCronJobName(obj interface{}) string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.Name
	case *batchv1beta1.CronJob:
		return cronJob.Name
	default:
		return ""
	}
}

// GetCronJobNamespace 获取 CronJob 的命名空间，无论是什么版本
func GetCronJobNamespace(obj interface{}) string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.Namespace
	case *batchv1beta1.CronJob:
		return cronJob.Namespace
	default:
		return ""
	}
}

// GetCronJobKind 获取 CronJob 的 Kind，无论是什么版本
func GetCronJobKind(obj interface{}) string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.Kind
	case *batchv1beta1.CronJob:
		return cronJob.Kind
	default:
		return ""
	}
}

// GetCronJobAPIVersion 获取 CronJob 的 APIVersion，无论是什么版本
func GetCronJobAPIVersion(obj interface{}) string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.APIVersion
	case *batchv1beta1.CronJob:
		return cronJob.APIVersion
	default:
		return ""
	}
}

// GetCronJobLabels 获取 CronJob 的标签，无论是什么版本
func GetCronJobLabels(obj interface{}) map[string]string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.Labels
	case *batchv1beta1.CronJob:
		return cronJob.Labels
	default:
		return nil
	}
}

// GetCronJobAnnotations 获取 CronJob 的注解，无论是什么版本
func GetCronJobAnnotations(obj interface{}) map[string]string {
	switch cronJob := obj.(type) {
	case *batch.CronJob:
		return cronJob.Annotations
	case *batchv1beta1.CronJob:
		return cronJob.Annotations
	default:
		return nil
	}
}
