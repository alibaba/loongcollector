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
	SendFunc SendFunc
	StopCh   chan struct{}
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

func (m *DeferredDeletionMetaStore) RegisterSendFunc(key string, f SendFunc, interval int) {
	sendFuncWithStopCh := &SendFuncWithStopCh{
		SendFunc: f,
		StopCh:   make(chan struct{}),
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
	idxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	// should delete oldIdxKeys in two cases:
	// 1. update event
	// 2. add event when the previous object is between deleted and deferred delete
	if obj, ok := m.Items[key]; ok {
		oldIdxKeys := m.getIdxKeys(obj)
		event.Object.FirstObservedTime = obj.FirstObservedTime

		// Use incremental index update: only modify changed index keys
		keysToRemove, keysToAdd := m.getIndexKeyDiff(oldIdxKeys, idxKeys)

		// Remove only the keys that are no longer needed
		for _, idxKey := range keysToRemove {
			m.Index[idxKey].Remove(key)
			if len(m.Index[idxKey].Keys) == 0 {
				delete(m.Index, idxKey)
			}
		}

		// Add only the new keys
		for _, idxKey := range keysToAdd {
			if _, ok := m.Index[idxKey]; !ok {
				m.Index[idxKey] = NewIndexItem()
			}
			m.Index[idxKey].Add(key)
		}

		// Release memory by setting Raw to nil before overwriting
		obj.Raw = nil
	} else {
		// New object: add all index keys
		for _, idxKey := range idxKeys {
			if _, ok := m.Index[idxKey]; !ok {
				m.Index[idxKey] = NewIndexItem()
			}
			m.Index[idxKey].Add(key)
		}
	}

	m.Items[key] = event.Object
	m.lock.Unlock()
	m.registerLock.RLock()
	for _, f := range m.sendFuncs {
		f.SendFunc([]*K8sMetaEvent{event})
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
	m.registerLock.RLock()
	for _, f := range m.sendFuncs {
		f.SendFunc([]*K8sMetaEvent{event})
	}
	m.registerLock.RUnlock()
	go func() {
		// wait and add a deferred delete event
		time.Sleep(time.Duration(m.gracePeriod) * time.Second)
		m.eventCh <- &K8sMetaEvent{
			EventType: EventTypeDeferredDelete,
			Object:    event.Object,
		}
	}()
}

func (m *DeferredDeletionMetaStore) handleDeferredDeleteEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "handleDeferredDeleteEvent keyFunc error", err)
		return
	}
	idxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	defer m.lock.Unlock()
	if obj, ok := m.Items[key]; ok {
		if obj.Deleted {
			delete(m.Items, key)
			for _, idxKey := range idxKeys {
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
		allItems := make([]*K8sMetaEvent, 0)
		m.lock.RLock()
		for _, obj := range m.Items {
			if !obj.Deleted {
				obj.LastObservedTime = time.Now().Unix()
				allItems = append(allItems, &K8sMetaEvent{
					EventType: EventTypeUpdate,
					Object:    obj,
				})
			}
		}
		m.lock.RUnlock()
		f.SendFunc(allItems)
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
