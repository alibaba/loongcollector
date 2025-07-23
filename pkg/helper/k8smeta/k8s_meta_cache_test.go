package k8smeta

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
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
