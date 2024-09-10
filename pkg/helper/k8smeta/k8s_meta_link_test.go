package k8smeta

import (
	"testing"

	"github.com/stretchr/testify/assert"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

func TestGetPodServiceLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	serviceCache := newK8sMetaCache(make(chan struct{}), SERVICE)
	serviceCache.metaStore.Items["default/service1"] = &ObjectWrapper{
		Raw: &corev1.Service{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "service1",
				Namespace: "default",
			},
			Spec: corev1.ServiceSpec{
				Selector: map[string]string{
					"app": "test",
				},
			},
		},
	}
	serviceCache.metaStore.Items["default/service2"] = &ObjectWrapper{
		Raw: &corev1.Service{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "service2",
				Namespace: "default",
			},
			Spec: corev1.ServiceSpec{
				Selector: map[string]string{
					"app": "test2",
				},
			},
		},
	}
	podCache.metaStore.Items["default/pod1"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "pod1",
				Namespace: "default",
				Labels: map[string]string{
					"app": "test",
					"env": "test",
				},
			},
			Spec: corev1.PodSpec{
				Containers: []corev1.Container{
					{
						Name:  "test",
						Image: "test",
					},
				},
			},
		},
	}
	podCache.metaStore.Items["default/pod2"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "pod2",
				Namespace: "default",
				Labels: map[string]string{
					"app": "test2",
					"env": "test2",
				},
			},
			Spec: corev1.PodSpec{
				Containers: []corev1.Container{
					{
						Name:  "test2",
						Image: "test2",
					},
				},
			},
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:     podCache,
		SERVICE: serviceCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodServiceLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "service1", results[0].Object.Raw.(*PodService).Service.Name)
	assert.Equal(t, "service2", results[1].Object.Raw.(*PodService).Service.Name)
}
