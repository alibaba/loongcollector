package k8smeta

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
	batchv1 "k8s.io/api/batch/v1"
	batchv1beta1 "k8s.io/api/batch/v1beta1"
	v1 "k8s.io/api/core/v1"
	extensionsv1beta1 "k8s.io/api/extensions/v1beta1"
	networkingv1 "k8s.io/api/networking/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/intstr"
)

func TestPreProcessPod(t *testing.T) {
	cache := newK8sMetaCache(make(chan struct{}), "Pod")
	pod := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test",
			Namespace: "default",
			Annotations: map[string]string{
				"kubectl.kubernetes.io/last-applied-configuration": "test",
			},
			ManagedFields: []metav1.ManagedFieldsEntry{
				{
					Manager:   "test",
					Operation: "test",
					Time:      &metav1.Time{Time: time.Now()},
				},
			},
		},
		Status: v1.PodStatus{
			Conditions: []v1.PodCondition{
				{
					Type:   v1.PodReady,
					Status: v1.ConditionTrue,
				},
			},
		},
		Spec: v1.PodSpec{
			Tolerations: []v1.Toleration{
				{
					Key: "test",
				},
			},
		},
	}
	processedPod := cache.preProcessPod(pod).(*v1.Pod)
	assert.Equal(t, processedPod.Annotations["kubectl.kubernetes.io/last-applied-configuration"], "")
	assert.Equal(t, processedPod.ManagedFields, []metav1.ManagedFieldsEntry{})
	assert.Equal(t, processedPod.Status.Conditions, []v1.PodCondition{})
	assert.Equal(t, processedPod.Spec.Tolerations, []v1.Toleration{})
}

func TestPreProcessPod_NilInput(t *testing.T) {
	cache := newK8sMetaCache(make(chan struct{}), "Pod")
	result := cache.preProcessPod(nil)
	assert.Nil(t, result)
}

func TestPreProcessPod_NonPodObject(t *testing.T) {
	cache := newK8sMetaCache(make(chan struct{}), "Pod")
	service := &v1.Service{}
	result := cache.preProcessPod(service)
	assert.Equal(t, service, result)
}

func TestPreProcessCronJobV1beta1(t *testing.T) {
	cache := newK8sMetaCache(make(chan struct{}), CRONJOB)
	suspend := true
	cronJob := &batchv1beta1.CronJob{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "cronjob",
			Namespace: "default",
			Labels:    map[string]string{"app": "demo"},
		},
		Spec: batchv1beta1.CronJobSpec{
			Schedule: "*/5 * * * *",
			Suspend:  &suspend,
		},
	}

	processed, ok := cache.preProcessCronJob(cronJob).(*batchv1.CronJob)
	require.True(t, ok)
	assert.Equal(t, "cronjob", processed.Name)
	assert.Equal(t, "default", processed.Namespace)
	assert.Equal(t, "*/5 * * * *", processed.Spec.Schedule)
	assert.Equal(t, &suspend, processed.Spec.Suspend)
	assert.Equal(t, "batch/v1", processed.APIVersion)
	assert.Equal(t, "CronJob", processed.Kind)
}

func TestPreProcessIngressV1beta1(t *testing.T) {
	cache := newK8sMetaCache(make(chan struct{}), INGRESS)
	ingress := &extensionsv1beta1.Ingress{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "ingress",
			Namespace: "default",
		},
		Spec: extensionsv1beta1.IngressSpec{
			Rules: []extensionsv1beta1.IngressRule{
				{
					Host: "example.com",
					IngressRuleValue: extensionsv1beta1.IngressRuleValue{
						HTTP: &extensionsv1beta1.HTTPIngressRuleValue{
							Paths: []extensionsv1beta1.HTTPIngressPath{
								{
									Path: "/named",
									Backend: extensionsv1beta1.IngressBackend{
										ServiceName: "named-service",
										ServicePort: intstr.FromString("http"),
									},
								},
								{
									Path: "/numbered",
									Backend: extensionsv1beta1.IngressBackend{
										ServiceName: "numbered-service",
										ServicePort: intstr.FromInt32(8080),
									},
								},
							},
						},
					},
				},
			},
		},
	}

	processed, ok := cache.preProcessIngress(ingress).(*networkingv1.Ingress)
	require.True(t, ok)
	require.Len(t, processed.Spec.Rules, 1)
	require.NotNil(t, processed.Spec.Rules[0].HTTP)
	require.Len(t, processed.Spec.Rules[0].HTTP.Paths, 2)
	assert.Equal(t, "named-service", processed.Spec.Rules[0].HTTP.Paths[0].Backend.Service.Name)
	assert.Equal(t, "http", processed.Spec.Rules[0].HTTP.Paths[0].Backend.Service.Port.Name)
	assert.Equal(t, "numbered-service", processed.Spec.Rules[0].HTTP.Paths[1].Backend.Service.Name)
	assert.Equal(t, int32(8080), processed.Spec.Rules[0].HTTP.Paths[1].Backend.Service.Port.Number)
	assert.Equal(t, "networking.k8s.io/v1", processed.APIVersion)
	assert.Equal(t, "Ingress", processed.Kind)
}
