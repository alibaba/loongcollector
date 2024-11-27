package k8smeta

import (
	"testing"

	"github.com/stretchr/testify/assert"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

func TestFindPodByServiceIPPort(t *testing.T) {
	manager := GetMetaManagerInstance()
	podCache := newK8sMetaCache(make(chan struct{}), POD)
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
						Ports: []corev1.ContainerPort{
							{
								ContainerPort: 80,
							},
						},
					},
				},
			},
			Status: corev1.PodStatus{
				PodIP: "1.1.1.1",
			},
		},
	}
	manager.cacheMap[POD] = podCache
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
				ClusterIPs: []string{
					"2.2.2.2",
				},
				Ports: []corev1.ServicePort{
					{
						Port: 80,
					},
				},
			},
		},
	}
	serviceCache.metaStore.Index["2.2.2.2"] = []string{"default/service1"}
	manager.cacheMap[SERVICE] = serviceCache
	handler := newMetadataHandler(GetMetaManagerInstance())
	podMetadata := handler.findPodByServiceIPPort("2.2.2.2", 0)
	assert.NotNil(t, podMetadata)
	assert.Equal(t, "pod1", podMetadata.PodName)

	podMetadata = handler.findPodByServiceIPPort("2.2.2.2", 80)
	assert.NotNil(t, podMetadata)
	assert.Equal(t, "pod1", podMetadata.PodName)

	podMetadata = handler.findPodByServiceIPPort("2.2.2.2", 90)
	assert.Nil(t, podMetadata)

	podMetadata = handler.findPodByServiceIPPort("3.3.3.3", 0)
	assert.Nil(t, podMetadata)
}

func TestFindPodByPodIPPort(t *testing.T) {
	handler := newMetadataHandler(GetMetaManagerInstance())
	pods := map[string][]*ObjectWrapper{
		"1.1.1.1": {
			{
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
								Ports: []corev1.ContainerPort{
									{
										ContainerPort: 80,
									},
								},
							},
						},
					},
					Status: corev1.PodStatus{
						PodIP: "1.1.1.1",
					},
				},
			},
		},
	}

	podMetadata := handler.findPodByPodIPPort("1.1.1.1", 0, pods)
	assert.NotNil(t, podMetadata)
	assert.Equal(t, "pod1", podMetadata.PodName)

	podMetadata = handler.findPodByPodIPPort("1.1.1.1", 80, pods)
	assert.NotNil(t, podMetadata)
	assert.Equal(t, "pod1", podMetadata.PodName)

	podMetadata = handler.findPodByPodIPPort("1.1.1.1", 90, pods)
	assert.Nil(t, podMetadata)

	podMetadata = handler.findPodByPodIPPort("2.2.2.2", 0, pods)
	assert.Nil(t, podMetadata)
}
