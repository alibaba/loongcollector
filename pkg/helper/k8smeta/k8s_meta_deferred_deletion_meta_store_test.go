package k8smeta

import (
	"testing"
	"time"

	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/tools/cache"
)

func TestDeferredDeletion(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc)
	cache.Start()
	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test",
			Namespace: "default",
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	cache.lock.RLock()
	if _, ok := cache.Items["default/test"]; !ok {
		t.Errorf("failed to add object to cache")
	}
	cache.lock.RUnlock()
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	cache.lock.RLock()
	if _, ok := cache.Items["default/test"]; !ok {
		t.Error("failed to deferred delete object from cache")
	}
	cache.lock.RUnlock()
	time.Sleep(time.Duration(gracePeriod+1) * time.Second)
	cache.lock.RLock()
	if _, ok := cache.Items["default/test"]; ok {
		t.Error("failed to delete object from cache")
	}
	cache.lock.RUnlock()
}

func TestRegisterWaitManagerReady(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc)
	manager := GetMetaManagerInstance()
	cache.RegisterSendFunc("test", func(kme []*K8sMetaEvent) {}, 100)
	select {
	case <-cache.eventCh:
		t.Error("should not receive event before manager is ready")
	case <-time.After(2 * time.Second):
	}
	manager.ready.Store(true)
	select {
	case <-cache.eventCh:
	case <-time.After(2 * time.Second):
		t.Error("should receive timer event immediately after manager is ready")
	}
}

func TestTimerSend(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	manager := GetMetaManagerInstance()
	manager.ready.Store(true)
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc)
	cache.Items["default/test"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "test",
				Namespace: "default",
			},
		},
	}
	cache.Start()
	resultCh := make(chan struct{})
	cache.RegisterSendFunc("test", func(kmes []*K8sMetaEvent) {
		resultCh <- struct{}{}
	}, 1)
	go func() {
		time.Sleep(3 * time.Second)
		close(stopCh)
	}()
	count := 0
	for {
		select {
		case <-resultCh:
			count++
		case <-stopCh:
			if count < 3 {
				t.Errorf("should receive 3 timer events, but got %d", count)
			}
			return
		}
	}
}
