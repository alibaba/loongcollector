package k8smeta

import (
	"context"
	"sync"
	"time"

	"k8s.io/client-go/tools/cache"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type IndexItem struct {
	Keys map[string]struct{} // alternative to set, struct{} is zero memory
}

func NewIndexItem() IndexItem {
	return IndexItem{
		Keys: make(map[string]struct{}),
	}
}

func (i IndexItem) Add(key string) {
	i.Keys[key] = struct{}{}
}

func (i IndexItem) Remove(key string) {
	delete(i.Keys, key)
}

type DeferredDeletionMetaStore struct {
	keyFunc    cache.KeyFunc
	indexRules []IdxFunc

	eventCh chan *K8sMetaEvent
	stopCh  <-chan struct{}

	// cache
	Items map[string]*ObjectWrapper
	Index map[string]IndexItem
	lock  sync.RWMutex

	// timer
	gracePeriod  int64
	registerLock sync.RWMutex
	sendFuncs    map[string]*SendFuncWithStopCh
}

type TimerEvent struct {
	ConfigName string
	Interval   int
}

type SendFuncWithStopCh struct {
	SendFunc   SendFunc
	StopCh     chan struct{}
	EventCh    chan []*K8sMetaEvent
	DrainBatch int
}

func NewDeferredDeletionMetaStore(eventCh chan *K8sMetaEvent, stopCh <-chan struct{}, gracePeriod int64, keyFunc cache.KeyFunc, indexRules ...IdxFunc) *DeferredDeletionMetaStore {
	m := &DeferredDeletionMetaStore{
		keyFunc:    keyFunc,
		indexRules: indexRules,

		eventCh: eventCh,
		stopCh:  stopCh,

		Items: make(map[string]*ObjectWrapper),
		Index: make(map[string]IndexItem),

		gracePeriod: gracePeriod,
		sendFuncs:   make(map[string]*SendFuncWithStopCh),
	}
	return m
}

func (m *DeferredDeletionMetaStore) Start() {
	go m.handleEvent()
}

func (m *DeferredDeletionMetaStore) Get(key []string) map[string][]*ObjectWrapper {
	m.lock.RLock()
	defer m.lock.RUnlock()
	result := make(map[string][]*ObjectWrapper)
	for _, k := range key {
		realKeys, ok := m.Index[k]
		if !ok {
			continue
		}
		for realKey := range realKeys.Keys {
			if obj, ok := m.Items[realKey]; ok {
				if obj.Raw != nil {
					result[k] = append(result[k], obj)
				} else {
					logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "raw object not found", realKey)
				}
			} else {
				logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "key not found", realKey)
			}
		}
	}
	return result
}

func (m *DeferredDeletionMetaStore) List() []*ObjectWrapper {
	m.lock.RLock()
	defer m.lock.RUnlock()
	result := make([]*ObjectWrapper, 0)
	for _, item := range m.Items {
		result = append(result, item)
	}
	return result
}

func (m *DeferredDeletionMetaStore) Filter(filterFunc func(*ObjectWrapper) bool, limit int) []*ObjectWrapper {
	m.lock.RLock()
	defer m.lock.RUnlock()
	result := make([]*ObjectWrapper, 0)
	for _, item := range m.Items {
		if filterFunc != nil {
			if filterFunc(item) {
				result = append(result, item)
			}
		} else {
			result = append(result, item)
		}
		if limit > 0 && len(result) >= limit {
			break
		}
	}
	return result
}

// sendBatch dispatches events with throttling for large batches (Timer full-sync).
// Returns true if StopCh was triggered and the caller should exit.
func (s *SendFuncWithStopCh) sendBatch(events []*K8sMetaEvent) bool {
	if len(events) > s.DrainBatch {
		throttle := time.NewTimer(50 * time.Millisecond)
		defer throttle.Stop()
		for i := 0; i < len(events); i += s.DrainBatch {
			end := i + s.DrainBatch
			if end > len(events) {
				end = len(events)
			}
			s.SendFunc(events[i:end])
			select {
			case <-throttle.C:
				throttle.Reset(50 * time.Millisecond)
			case <-s.StopCh:
				return true
			}
		}
	} else {
		s.SendFunc(events)
	}
	return false
}

func (m *DeferredDeletionMetaStore) RegisterSendFunc(key string, f SendFunc, interval int, eventChSize int, drainBatch int) {
	sendFuncWithStopCh := &SendFuncWithStopCh{
		SendFunc:   f,
		StopCh:     make(chan struct{}),
		EventCh:    make(chan []*K8sMetaEvent, eventChSize),
		DrainBatch: drainBatch,
	}
	m.registerLock.Lock()
	m.sendFuncs[key] = sendFuncWithStopCh
	m.registerLock.Unlock()

	go func() {
		defer panicRecover()
		event := &K8sMetaEvent{
			EventType: EventTypeTimer,
			Object: &ObjectWrapper{
				Raw: &TimerEvent{
					ConfigName: key,
					Interval:   interval,
				},
			},
		}
		manager := GetMetaManagerInstance()
		for {
			if manager.IsReady() {
				break
			}
			time.Sleep(1 * time.Second)
		}

		m.eventCh <- event
		ticker := time.NewTicker(time.Duration(interval) * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				m.eventCh <- event
			case e := <-sendFuncWithStopCh.EventCh:
				if sendFuncWithStopCh.sendBatch(e) {
					return
				}
				n := len(sendFuncWithStopCh.EventCh)
			drainEventCh:
				for i := 0; i < n && i < sendFuncWithStopCh.DrainBatch; i++ {
					select {
					case next := <-sendFuncWithStopCh.EventCh:
						if sendFuncWithStopCh.sendBatch(next) {
							return
						}
					default:
						break drainEventCh
					}
				}
			case <-sendFuncWithStopCh.StopCh:
				return
			}
		}
	}()
}

func (m *DeferredDeletionMetaStore) UnRegisterSendFunc(key string) {
	m.registerLock.Lock()
	if stopCh, ok := m.sendFuncs[key]; ok {
		close(stopCh.StopCh)
	}
	delete(m.sendFuncs, key)
	m.registerLock.Unlock()
}

// realtime events (add, update, delete) and timer events are handled sequentially
func (m *DeferredDeletionMetaStore) handleEvent() {
	defer panicRecover()
	for {
		select {
		case event := <-m.eventCh:
			switch event.EventType {
			case EventTypeAdd, EventTypeUpdate:
				m.handleAddOrUpdateEvent(event)
			case EventTypeDelete:
				m.handleDeleteEvent(event)
			case EventTypeDeferredDelete:
				m.handleDeferredDeleteEvent(event)
			case EventTypeTimer:
				m.handleTimerEvent(event)
			default:
				logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "unknown event type", event.EventType)
			}
		case <-m.stopCh:
			m.registerLock.Lock()
			for _, f := range m.sendFuncs {
				close(f.StopCh)
			}
			m.registerLock.Unlock()
			return
		}
	}
}

func (m *DeferredDeletionMetaStore) handleAddOrUpdateEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "handle k8s meta with keyFunc error", err)
		return
	}
	newIdxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	// should delete oldIdxKeys in two cases:
	// 1. update event
	// 2. add event when the previous object is between deleted and deferred delete
	if obj, ok := m.Items[key]; ok {
		oldIdxKeys := m.getIdxKeys(obj)
		event.Object.FirstObservedTime = obj.FirstObservedTime

		// Use incremental index update: only modify changed index keys
		keysToRemove, keysToAdd := m.getIndexKeyDiff(oldIdxKeys, newIdxKeys)

		// Remove only the keys that are no longer needed
		for _, idxKey := range keysToRemove {
			if indexItem, ok := m.Index[idxKey]; ok {
				indexItem.Remove(key)
				if len(indexItem.Keys) == 0 {
					delete(m.Index, idxKey)
				}
			}
		}

		// Add only the new keys
		for _, idxKey := range keysToAdd {
			if _, ok := m.Index[idxKey]; !ok {
				m.Index[idxKey] = NewIndexItem()
			}
			m.Index[idxKey].Add(key)
		}
	} else {
		// New object: add all index keys
		for _, idxKey := range newIdxKeys {
			if _, ok := m.Index[idxKey]; !ok {
				m.Index[idxKey] = NewIndexItem()
			}
			m.Index[idxKey].Add(key)
		}
	}

	// cache is always updated regardless of whether the notification below succeeds;
	// dropped notifications will be compensated by the periodic timer full-sync
	m.Items[key] = event.Object
	m.lock.Unlock()
	m.registerLock.RLock()
	for _, f := range m.sendFuncs {
		select {
		case f.EventCh <- []*K8sMetaEvent{event}:
		default:
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "send buffer full, dropping event", "object_key", key)
		}
	}
	m.registerLock.RUnlock()
}

func (m *DeferredDeletionMetaStore) handleDeleteEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "handle k8s meta with keyFunc error", err)
		return
	}
	m.lock.Lock()
	if obj, ok := m.Items[key]; ok {
		obj.Deleted = true
		event.Object.FirstObservedTime = obj.FirstObservedTime
	}
	m.lock.Unlock()
	// Dropped delete notifications are NOT compensated by Timer full-sync
	// (Timer only sends non-deleted items). Downstream relies on keepAliveSeconds
	// TTL to expire stale entities, so the delete will take effect after Interval*2.
	m.registerLock.RLock()
	for _, f := range m.sendFuncs {
		select {
		case f.EventCh <- []*K8sMetaEvent{event}:
		default:
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "send buffer full, dropping delete event", "object_key", key)
		}
	}
	m.registerLock.RUnlock()
	// time.AfterFunc avoids a sleeping goroutine per delete during gracePeriod
	time.AfterFunc(time.Duration(m.gracePeriod)*time.Second, func() {
		select {
		case m.eventCh <- &K8sMetaEvent{
			EventType: EventTypeDeferredDelete,
			Object:    event.Object,
		}:
		default:
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode,
				"eventCh full, deferred delete dropped", "key", key)
		}
	})
}

func (m *DeferredDeletionMetaStore) handleDeferredDeleteEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "handleDeferredDeleteEvent keyFunc error", err)
		return
	}
	newIdxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	defer m.lock.Unlock()
	if obj, ok := m.Items[key]; ok {
		if obj.Deleted {
			delete(m.Items, key)
			for _, idxKey := range newIdxKeys {
				if _, ok := m.Index[idxKey]; !ok {
					continue
				}
				m.Index[idxKey].Remove(key)
				if len(m.Index[idxKey].Keys) == 0 {
					delete(m.Index, idxKey)
				}
			}
		}
		// if deleted is false, there is a new add event between delete event and deferred delete event
	}
}

func (m *DeferredDeletionMetaStore) handleTimerEvent(event *K8sMetaEvent) {
	timerEvent := event.Object.Raw.(*TimerEvent)
	m.registerLock.RLock()
	defer m.registerLock.RUnlock()
	if f, ok := m.sendFuncs[timerEvent.ConfigName]; ok {
		// skip building the full snapshot if EventCh is already full
		if len(f.EventCh) == cap(f.EventCh) {
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "send buffer full, skipping timer snapshot build", "config_name", timerEvent.ConfigName)
			return
		}
		allItems := make([]*K8sMetaEvent, 0, len(m.Items))
		m.lock.RLock()
		for _, obj := range m.Items {
			if !obj.Deleted {
				// shallow copy ObjectWrapper so we can set LastObservedTime without
				// mutating the cached entry. Raw is effectively immutable after
				// being stored, so sharing the pointer is safe.
				newObj := *obj
				newObj.LastObservedTime = time.Now().Unix()
				allItems = append(allItems, &K8sMetaEvent{
					EventType: EventTypeUpdate,
					Object:    &newObj,
				})
			}
		}
		m.lock.RUnlock()
		select {
		case f.EventCh <- allItems:
		default:
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "send buffer full, skipping timer event", "config_name", timerEvent.ConfigName)
		}
	}
}

func (m *DeferredDeletionMetaStore) getIdxKeys(obj *ObjectWrapper) []string {
	result := make([]string, 0)
	for _, rule := range m.indexRules {
		idxKeys, err := rule(obj.Raw)
		if err != nil {
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "handle k8s meta with idx rules error", err)
			return nil
		}
		result = append(result, idxKeys...)
	}
	return result
}

// getIndexKeyDiff returns keys to remove and keys to add for incremental index update
func (m *DeferredDeletionMetaStore) getIndexKeyDiff(oldKeys, newKeys []string) (toRemove, toAdd []string) {

	if len(oldKeys) == 0 && len(newKeys) == 0 {
		return nil, nil
	}

	oldKeySet := make(map[string]struct{})
	newKeySet := make(map[string]struct{})

	for _, key := range oldKeys {
		oldKeySet[key] = struct{}{}
	}
	for _, key := range newKeys {
		newKeySet[key] = struct{}{}
	}

	// Find keys to remove (in old but not in new)
	for _, key := range oldKeys {
		if _, exists := newKeySet[key]; !exists {
			toRemove = append(toRemove, key)
		}
	}

	// Find keys to add (in new but not in old)
	for _, key := range newKeys {
		if _, exists := oldKeySet[key]; !exists {
			toAdd = append(toAdd, key)
		}
	}

	return toRemove, toAdd
}
