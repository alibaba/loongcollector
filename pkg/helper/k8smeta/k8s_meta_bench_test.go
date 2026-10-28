//go:build stress

package k8smeta

import (
	"fmt"
	"sync/atomic"
	"testing"
	"time"

	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/tools/cache"
)

func generatePods(n int) []*corev1.Pod {
	pods := make([]*corev1.Pod, n)
	for i := 0; i < n; i++ {
		pods[i] = &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:            fmt.Sprintf("pod-%d", i),
				Namespace:       fmt.Sprintf("ns-%d", i%100),
				ResourceVersion: "1",
				Generation:      1,
				Labels:          map[string]string{"app": "bench"},
			},
			Status: corev1.PodStatus{
				Phase: corev1.PodRunning,
				PodIP: fmt.Sprintf("10.0.%d.%d", i/256, i%256),
			},
		}
	}
	return pods
}

func setupStore(eventChSize int) (*DeferredDeletionMetaStore, chan *K8sMetaEvent, chan struct{}) {
	eventCh := make(chan *K8sMetaEvent, eventChSize)
	stopCh := make(chan struct{})
	store := NewDeferredDeletionMetaStore(eventCh, stopCh, 120, cache.MetaNamespaceKeyFunc, generateCommonKey)
	store.Start()
	manager := GetMetaManagerInstance()
	manager.ready.Store(true)
	return store, eventCh, stopCh
}

// BenchmarkStoreEventThroughput measures how many events/sec the Store can
// process and dispatch to a fast consumer.
func BenchmarkStoreEventThroughput(b *testing.B) {
	for _, objCount := range []int{1000, 10000, 100000} {
		b.Run(fmt.Sprintf("objects=%d", objCount), func(b *testing.B) {
			store, eventCh, stopCh := setupStore(10000)
			defer close(stopCh)

			var received int64
			store.RegisterSendFunc("bench", func(events []*K8sMetaEvent) {
				atomic.AddInt64(&received, int64(len(events)))
			}, 86400, 5000, 2000)

			pods := generatePods(objCount)

			b.ResetTimer()
			for i := 0; i < b.N; i++ {
				pod := pods[i%objCount]
				eventCh <- &K8sMetaEvent{
					EventType: EventTypeUpdate,
					Object: &ObjectWrapper{
						ResourceType:      POD,
						Raw:               pod,
						FirstObservedTime: time.Now().Unix(),
						LastObservedTime:  time.Now().Unix(),
					},
				}
			}
			b.StopTimer()

			time.Sleep(100 * time.Millisecond)
			b.ReportMetric(float64(atomic.LoadInt64(&received)), "received")
		})
	}
}

// BenchmarkUpdateFilter measures the overhead of isSignificantUpdate filtering.
func BenchmarkUpdateFilter(b *testing.B) {
	b.Run("significant_change", func(b *testing.B) {
		oldPod := &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{Generation: 1, Labels: map[string]string{"app": "web"}},
			Status:     corev1.PodStatus{Phase: corev1.PodRunning, PodIP: "10.0.0.1"},
		}
		newPod := &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{Generation: 1, Labels: map[string]string{"app": "web"}},
			Status:     corev1.PodStatus{Phase: corev1.PodSucceeded, PodIP: "10.0.0.1"},
		}
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			isSignificantUpdate(POD, oldPod, newPod)
		}
	})

	b.Run("insignificant_change", func(b *testing.B) {
		oldPod := &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{Generation: 1, Labels: map[string]string{"app": "web"}},
			Status: corev1.PodStatus{
				Phase: corev1.PodRunning, PodIP: "10.0.0.1",
				Conditions: []corev1.PodCondition{{Type: corev1.PodReady, Status: corev1.ConditionTrue}},
			},
		}
		newPod := &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{Generation: 1, Labels: map[string]string{"app": "web"}},
			Status: corev1.PodStatus{
				Phase: corev1.PodRunning, PodIP: "10.0.0.1",
				Conditions: []corev1.PodCondition{{Type: corev1.PodReady, Status: corev1.ConditionFalse}},
			},
		}
		b.ResetTimer()
		for i := 0; i < b.N; i++ {
			isSignificantUpdate(POD, oldPod, newPod)
		}
	})
}

// TestStressRealistic simulates the production environment:
//   - cache_size ~238k objects
//   - real-time updates ~60/sec
//   - Timer full-sync every timerInterval seconds (dumps all objects at once)
//   - downstream consumption ~200 events/sec (simulated by sleep in SendFunc)
//
// Run with: go test -tags stress -count=1 -v -run TestStressRealistic github.com/alibaba/ilogtail/pkg/helper/k8smeta
func TestStressRealistic(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	const (
		cacheSize       = 238000
		realtimeRate    = 60               // real-time updates per second
		timerInterval   = 30 * time.Second // shortened for test (prod: 6000s)
		testDuration    = 65 * time.Second // long enough to see 2 timer fires
		eventChSize     = 5000
		drainBatch      = 2000
		downstreamDelay = 5 * time.Millisecond // ~200 events/sec per batch
	)

	store, eventCh, stopCh := setupStore(eventChSize)
	defer close(stopCh)

	// populate cache BEFORE registering SendFunc
	t.Logf("Populating cache with %d objects...", cacheSize)
	pods := generatePods(cacheSize)
	for i, pod := range pods {
		eventCh <- &K8sMetaEvent{
			EventType: EventTypeAdd,
			Object: &ObjectWrapper{
				ResourceType:      POD,
				Raw:               pod,
				FirstObservedTime: time.Now().Unix(),
				LastObservedTime:  time.Now().Unix(),
			},
		}
		if (i+1)%50000 == 0 {
			t.Logf("  populated %d/%d", i+1, cacheSize)
			time.Sleep(100 * time.Millisecond)
		}
	}
	t.Logf("Cache populated. Waiting for Store to process...")
	time.Sleep(2 * time.Second)

	// register SendFunc after population, so Timer starts with full cache
	var received int64
	store.RegisterSendFunc("realistic", func(events []*K8sMetaEvent) {
		time.Sleep(downstreamDelay)
		atomic.AddInt64(&received, int64(len(events)))
	}, int(timerInterval.Seconds()), eventChSize, drainBatch)

	var realtimeSent, realtimeDropped int64

	t.Logf("Starting realistic stress test: %v duration, realtime=%d/sec, timer=%v",
		testDuration, realtimeRate, timerInterval)

	deadline := time.After(testDuration)
	ticker := time.NewTicker(time.Second / time.Duration(realtimeRate))
	defer ticker.Stop()
	start := time.Now()

loop:
	for {
		select {
		case <-deadline:
			break loop
		case <-ticker.C:
			idx := int(atomic.LoadInt64(&realtimeSent)) % cacheSize
			pod := pods[idx]
			pod.ResourceVersion = fmt.Sprintf("%d", time.Now().UnixNano())
			select {
			case eventCh <- &K8sMetaEvent{
				EventType: EventTypeUpdate,
				Object: &ObjectWrapper{
					ResourceType:      POD,
					Raw:               pod,
					FirstObservedTime: time.Now().Unix(),
					LastObservedTime:  time.Now().Unix(),
				},
			}:
			default:
				atomic.AddInt64(&realtimeDropped, 1)
			}
			atomic.AddInt64(&realtimeSent, 1)
		}
	}

	// wait for pipeline to drain
	t.Logf("Test phase complete, draining pipeline...")
	time.Sleep(3 * time.Second)
	elapsed := time.Since(start)
	recv := atomic.LoadInt64(&received)
	sent := atomic.LoadInt64(&realtimeSent)
	rtDropped := atomic.LoadInt64(&realtimeDropped)

	t.Logf("========== Results ==========")
	t.Logf("Duration:           %v", elapsed.Truncate(time.Millisecond))
	t.Logf("Cache size:         %d", cacheSize)
	t.Logf("Realtime sent:      %d (%.0f/sec)", sent, float64(sent)/testDuration.Seconds())
	t.Logf("Realtime eventCh drop: %d", rtDropped)
	t.Logf("Received total:     %d (%.0f/sec)", recv, float64(recv)/elapsed.Seconds())
	t.Logf("Timer interval:     %v", timerInterval)
	t.Logf("EventCh:            size=%d, drain=%d", eventChSize, drainBatch)
	t.Logf("Downstream delay:   %v per batch", downstreamDelay)
}

// TestStressLargeCluster simulates a 560k-object cluster with 15+ link types.
// Each event in the consumer goroutine triggers ~16x processing (1 entity + 15 links).
//
// Run with: go test -tags stress -count=1 -v -run TestStressLargeCluster github.com/alibaba/ilogtail/pkg/helper/k8smeta
func TestStressLargeCluster(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	const (
		cacheSize       = 560000
		realtimeRate    = 86               // ~5172 updates/min ÷ 60
		linkTypes       = 15               // number of link types, each event processed 1+15=16 times
		timerInterval   = 30 * time.Second // shortened for test (prod: 300s)
		testDuration    = 35 * time.Second
		eventChSize     = 10000
		drainBatch      = 2000
		downstreamDelay = 5 * time.Millisecond
	)

	store, eventCh, stopCh := setupStore(eventChSize)
	defer close(stopCh)

	t.Logf("Populating cache with %d objects...", cacheSize)
	pods := generatePods(cacheSize)
	for i, pod := range pods {
		eventCh <- &K8sMetaEvent{
			EventType: EventTypeAdd,
			Object: &ObjectWrapper{
				ResourceType:      POD,
				Raw:               pod,
				FirstObservedTime: time.Now().Unix(),
				LastObservedTime:  time.Now().Unix(),
			},
		}
		if (i+1)%100000 == 0 {
			t.Logf("  populated %d/%d", i+1, cacheSize)
			time.Sleep(200 * time.Millisecond)
		}
	}
	t.Logf("Cache populated. Waiting for Store to process...")
	time.Sleep(3 * time.Second)

	var received int64
	// simulate per-event cost: 1 entity + 15 link generations
	// each link generation adds ~0.3ms of processing
	store.RegisterSendFunc("large-cluster", func(events []*K8sMetaEvent) {
		for range events {
			// simulate entity processing + 15 link generations
			for j := 0; j < 1+linkTypes; j++ {
				time.Sleep(20 * time.Microsecond)
			}
		}
		atomic.AddInt64(&received, int64(len(events)))
	}, int(timerInterval.Seconds()), eventChSize, drainBatch)

	var realtimeSent, realtimeDropped int64

	t.Logf("Starting large cluster stress: %v, rate=%d/sec, links=%d, buffer=%d",
		testDuration, realtimeRate, linkTypes, eventChSize)

	deadline := time.After(testDuration)
	ticker := time.NewTicker(time.Second / time.Duration(realtimeRate))
	defer ticker.Stop()
	start := time.Now()

loop:
	for {
		select {
		case <-deadline:
			break loop
		case <-ticker.C:
			idx := int(atomic.LoadInt64(&realtimeSent)) % cacheSize
			pod := pods[idx]
			pod.ResourceVersion = fmt.Sprintf("%d", time.Now().UnixNano())
			select {
			case eventCh <- &K8sMetaEvent{
				EventType: EventTypeUpdate,
				Object: &ObjectWrapper{
					ResourceType:      POD,
					Raw:               pod,
					FirstObservedTime: time.Now().Unix(),
					LastObservedTime:  time.Now().Unix(),
				},
			}:
			default:
				atomic.AddInt64(&realtimeDropped, 1)
			}
			atomic.AddInt64(&realtimeSent, 1)
		}
	}

	t.Logf("Test phase complete, draining pipeline...")
	time.Sleep(3 * time.Second)
	elapsed := time.Since(start)
	recv := atomic.LoadInt64(&received)
	sent := atomic.LoadInt64(&realtimeSent)
	rtDropped := atomic.LoadInt64(&realtimeDropped)

	perEventCost := time.Duration(20*(1+linkTypes)) * time.Microsecond
	maxConsumerRate := float64(time.Second) / float64(perEventCost)

	t.Logf("========== Large Cluster Results ==========")
	t.Logf("Duration:           %v", elapsed.Truncate(time.Millisecond))
	t.Logf("Cache size:         %d", cacheSize)
	t.Logf("Link types:         %d (per-event cost: %v, max consumer: %.0f/sec)", linkTypes, perEventCost, maxConsumerRate)
	t.Logf("Realtime sent:      %d (%.0f/sec)", sent, float64(sent)/testDuration.Seconds())
	t.Logf("Realtime eventCh drop: %d", rtDropped)
	t.Logf("Received total:     %d (%.0f/sec)", recv, float64(recv)/elapsed.Seconds())
	t.Logf("Timer interval:     %v", timerInterval)
	t.Logf("EventCh:            size=%d, drain=%d", eventChSize, drainBatch)
}

// TestStressTimerOnly isolates the Timer full-sync behavior:
// no real-time events, just Timer dumps of 238k objects with slow downstream.
//
// Run with: go test -tags stress -count=1 -v -run TestStressTimerOnly github.com/alibaba/ilogtail/pkg/helper/k8smeta
func TestStressTimerOnly(t *testing.T) {
	if testing.Short() {
		t.Skip("skipping stress test in short mode")
	}

	const (
		cacheSize       = 238000
		eventChSize     = 5000
		drainBatch      = 2000
		downstreamDelay = 5 * time.Millisecond
	)

	store, eventCh, stopCh := setupStore(eventChSize)
	defer close(stopCh)

	// populate cache BEFORE registering SendFunc,
	// so population events don't go through the slow consumer
	t.Logf("Populating cache with %d objects...", cacheSize)
	pods := generatePods(cacheSize)
	for i, pod := range pods {
		eventCh <- &K8sMetaEvent{
			EventType: EventTypeAdd,
			Object: &ObjectWrapper{
				ResourceType:      POD,
				Raw:               pod,
				FirstObservedTime: time.Now().Unix(),
				LastObservedTime:  time.Now().Unix(),
			},
		}
		if (i+1)%50000 == 0 {
			t.Logf("  populated %d/%d", i+1, cacheSize)
			time.Sleep(100 * time.Millisecond)
		}
	}
	t.Logf("Cache populated. Waiting for Store to process...")
	time.Sleep(2 * time.Second)

	// register SendFunc after population, so Timer fires start with full cache
	var received int64
	store.RegisterSendFunc("timer-test", func(events []*K8sMetaEvent) {
		time.Sleep(downstreamDelay)
		atomic.AddInt64(&received, int64(len(events)))
	}, 5, eventChSize, drainBatch)

	t.Logf("Waiting for Timer fires...")

	start := time.Now()
	// wait for ~15 seconds to see 2-3 Timer fires (interval=5s)
	time.Sleep(18 * time.Second)
	elapsed := time.Since(start)
	recv := atomic.LoadInt64(&received)

	t.Logf("========== Timer-Only Results ==========")
	t.Logf("Duration:        %v", elapsed.Truncate(time.Millisecond))
	t.Logf("Cache size:      %d", cacheSize)
	t.Logf("Received:        %d (%.1f%% of cache)", recv, float64(recv)*100/float64(cacheSize))
	t.Logf("Per Timer fire:  ~%d events expected", cacheSize)
	t.Logf("Throughput:      %.0f events/sec", float64(recv)/elapsed.Seconds())
	t.Logf("EventCh:         size=%d, drain=%d", eventChSize, drainBatch)
	t.Logf("Downstream:      %v delay per batch", downstreamDelay)
}
