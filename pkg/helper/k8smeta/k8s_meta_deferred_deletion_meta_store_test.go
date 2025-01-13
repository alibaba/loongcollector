package k8smeta

import (
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/tools/cache"
)

func TestDeferredDeletion(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc, generatePodIPKey)
	cache.Start()
	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test",
			Namespace: "default",
		},
		Status: corev1.PodStatus{
			PodIP: "127.0.0.1",
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
	assert.Equal(t, 1, len(cache.Get([]string{"127.0.0.1"})))
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	time.Sleep(10 * time.Millisecond)
	cache.lock.RLock()
	if item, ok := cache.Items["default/test"]; !ok {
		t.Error("failed to deferred delete object from cache")
	} else {
		assert.Equal(t, true, item.Deleted)
	}
	cache.lock.RUnlock()
	assert.Equal(t, 1, len(cache.Get([]string{"127.0.0.1"})))
	time.Sleep(time.Duration(gracePeriod+1) * time.Second)
	cache.lock.RLock()
	if _, ok := cache.Items["default/test"]; ok {
		t.Error("failed to delete object from cache")
	}
	cache.lock.RUnlock()
	assert.Equal(t, 0, len(cache.Get([]string{"127.0.0.1"})))
}

func TestDeferredDeletionWithAddEvent(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc, generatePodIPKey)
	cache.Start()
	pod := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test",
			Namespace: "default",
		},
		Status: corev1.PodStatus{
			PodIP: "127.0.0.1",
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
	// add again
	pod2 := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test",
			Namespace: "default",
		},
		Status: corev1.PodStatus{
			PodIP: "127.0.0.2",
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: pod2,
		},
	}
	time.Sleep(10 * time.Millisecond)
	cache.lock.RLock()
	if item, ok := cache.Items["default/test"]; !ok {
		t.Error("failed to deferred delete object from cache")
	} else {
		assert.Equal(t, false, item.Deleted)
	}
	cache.lock.RUnlock()
	assert.Equal(t, 0, len(cache.Get([]string{"127.0.0.1"})))
	assert.Equal(t, 1, len(cache.Get([]string{"127.0.0.2"})))
	time.Sleep(time.Duration(gracePeriod+1) * time.Second)
	cache.lock.RLock()
	if _, ok := cache.Items["default/test"]; !ok {
		t.Error("should not delete object from cache")
	}
	cache.lock.RUnlock()
	assert.Equal(t, 1, len(cache.Get([]string{"127.0.0.2"})))
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

func TestFilter(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc)
	cache.Items["default/test"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "test",
				Namespace: "default",
				Labels: map[string]string{
					"app": "test",
				},
			},
		},
	}
	cache.Items["default/test2"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "test2",
				Namespace: "default",
				Labels: map[string]string{
					"app": "test2",
				},
			},
		},
	}
	cache.Items["default/test3"] = &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "test3",
				Namespace: "default",
				Labels: map[string]string{
					"app": "test2",
				},
			},
		},
	}
	objs := cache.Filter(func(obj *ObjectWrapper) bool {
		return obj.Raw.(*corev1.Pod).Labels["app"] == "test2"
	}, 1)
	assert.Len(t, objs, 1)
	assert.Equal(t, "test2", objs[0].Raw.(*corev1.Pod).Labels["app"])

	objs = cache.Filter(nil, 10)
	assert.Len(t, objs, 3)
}

func TestGet(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc, generateCommonKey)
	cache.Start()
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: &corev1.Pod{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test",
					Namespace: "default",
				},
			},
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: &corev1.Pod{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test2",
					Namespace: "default",
				},
			},
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: nil,
		},
	}
	// nil object in cache
	cache.Items["default/test3"] = &ObjectWrapper{
		Raw: nil,
	}
	cache.Index["default/test3"] = IndexItem{
		Keys: map[string]struct{}{
			"default/test3": {},
		},
	}
	// in index but not in cache
	cache.Index["default/test4"] = IndexItem{
		Keys: map[string]struct{}{
			"default/test4": {},
		},
	}

	time.Sleep(10 * time.Millisecond)
	objs := cache.Get([]string{"default/test", "default/test2", "default/test3", "default/test4", "default/test5"})
	assert.Len(t, objs, 2)
	assert.Equal(t, "test", objs["default/test"][0].Raw.(*corev1.Pod).Name)
	assert.Equal(t, "test2", objs["default/test2"][0].Raw.(*corev1.Pod).Name)
}

func TestIndex(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc, generateCommonKey)
	cache.Start()
	// add
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
	pod2 := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test2",
			Namespace: "default",
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: pod2,
		},
	}
	time.Sleep(time.Millisecond * 10)
	cache.lock.RLock()
	assert.Equal(t, 2, len(cache.Items))
	assert.Equal(t, 2, len(cache.Index))
	for _, idx := range cache.Index {
		assert.Equal(t, 1, len(idx.Keys))
	}
	cache.lock.RUnlock()

	// update
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeUpdate,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeUpdate,
		Object: &ObjectWrapper{
			Raw: pod2,
		},
	}
	time.Sleep(time.Millisecond * 10)
	cache.lock.RLock()
	assert.Equal(t, 2, len(cache.Items))
	assert.Equal(t, 2, len(cache.Index))
	for _, idx := range cache.Index {
		assert.Equal(t, 1, len(idx.Keys))
	}
	cache.lock.RUnlock()

	// delete
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod2,
		},
	}
	time.Sleep(time.Duration(gracePeriod) * time.Second)
	time.Sleep(time.Millisecond * 10)
	cache.lock.RLock()
	assert.Equal(t, 0, len(cache.Items))
	assert.Equal(t, 0, len(cache.Index))
	cache.lock.RUnlock()
}

func TestRegisterAndUnRegisterSendFunc(t *testing.T) {
	eventCh := make(chan *K8sMetaEvent)
	stopCh := make(chan struct{})
	gracePeriod := 1
	cache := NewDeferredDeletionMetaStore(eventCh, stopCh, int64(gracePeriod), cache.MetaNamespaceKeyFunc)
	cache.Start()
	counter := 0
	interval := 1
	cache.RegisterSendFunc("test", func(kme []*K8sMetaEvent) {
		counter++
	}, interval)
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
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeDelete,
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: "not exist",
		Object: &ObjectWrapper{
			Raw: pod,
		},
	}
	time.Sleep(10 * time.Millisecond)
	assert.Equal(t, 3, counter) // 1 for add event, 1 for timer event, 1 for delete event
	cache.UnRegisterSendFunc("test")
	time.Sleep(10 * time.Millisecond)
	pod2 := &corev1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "test2",
			Namespace: "default",
		},
	}
	eventCh <- &K8sMetaEvent{
		EventType: EventTypeAdd,
		Object: &ObjectWrapper{
			Raw: pod2,
		},
	}
	time.Sleep(time.Duration(interval) * time.Second)
	assert.Equal(t, 3, counter)
}
