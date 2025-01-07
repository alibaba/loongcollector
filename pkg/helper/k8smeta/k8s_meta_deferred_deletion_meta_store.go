package k8smeta

import (
	"context"
	"sync"
	"time"

	"k8s.io/client-go/tools/cache"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type DeferredDeletionMetaStore struct {
	keyFunc    cache.KeyFunc
	indexRules []IdxFunc

	eventCh chan *K8sMetaEvent
	stopCh  <-chan struct{}

	// cache
	Items map[string]*ObjectWrapper
	Index map[string][]string
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
		Index: make(map[string][]string),

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
		for _, realKey := range realKeys {
			result[k] = append(result[k], m.Items[realKey])
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
			case EventTypeAdd:
				m.handleAddEvent(event)
			case EventTypeUpdate:
				m.handleUpdateEvent(event)
			case EventTypeDelete:
				m.handleDeleteEvent(event)
			case EventTypeDeferredDelete:
				m.handleDeferredDeleteEvent(event)
			case EventTypeTimer:
				m.handleTimerEvent(event)
			default:
				logger.Error(context.Background(), "unknown event type", event.EventType)
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

func (m *DeferredDeletionMetaStore) handleAddEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Error(context.Background(), "K8S_META_HANDLE_ALARM", "handle k8s meta with keyFunc error", err)
		return
	}
	idxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	m.Items[key] = event.Object
	for _, idxKey := range idxKeys {
		if _, ok := m.Index[idxKey]; !ok {
			m.Index[idxKey] = make([]string, 0)
		}
		m.Index[idxKey] = append(m.Index[idxKey], key)
	}
	m.lock.Unlock()
	m.registerLock.RLock()
	for _, f := range m.sendFuncs {
		f.SendFunc([]*K8sMetaEvent{event})
	}
	m.registerLock.RUnlock()
}

func (m *DeferredDeletionMetaStore) handleUpdateEvent(event *K8sMetaEvent) {
	key, err := m.keyFunc(event.Object.Raw)
	if err != nil {
		logger.Error(context.Background(), "K8S_META_HANDLE_ALARM", "handle k8s meta with keyFunc error", err)
		return
	}
	idxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	if obj, ok := m.Items[key]; ok {
		event.Object.FirstObservedTime = obj.FirstObservedTime
	}
	m.Items[key] = event.Object
	for _, idxKey := range idxKeys {
		if _, ok := m.Index[idxKey]; !ok {
			m.Index[idxKey] = make([]string, 0)
		}
		m.Index[idxKey] = append(m.Index[idxKey], key)
	}
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
		logger.Error(context.Background(), "K8S_META_HANDLE_ALARM", "handle k8s meta with keyFunc error", err)
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
		logger.Error(context.Background(), "handleDeferredDeleteEvent keyFunc error", err)
		return
	}
	idxKeys := m.getIdxKeys(event.Object)
	m.lock.Lock()
	defer m.lock.Unlock()
	if obj, ok := m.Items[key]; ok {
		if obj.Deleted {
			delete(m.Items, key)
			for _, idxKey := range idxKeys {
				for i, k := range m.Index[idxKey] {
					if k == key {
						m.Index[idxKey] = append(m.Index[idxKey][:i], m.Index[idxKey][i+1:]...)
						break
					}
				}
				if len(m.Index[idxKey]) == 0 {
					delete(m.Index, idxKey)
				}
			}
		} else {
			// there is a new add event between delete event and deferred delete event
			// clear invalid index
			newIdxKeys := m.getIdxKeys(obj)
			for i := range idxKeys {
				if idxKeys[i] != newIdxKeys[i] {
					for j, k := range m.Index[idxKeys[i]] {
						if k == key {
							m.Index[idxKeys[i]] = append(m.Index[idxKeys[i]][:j], m.Index[idxKeys[i]][j+1:]...)
							break
						}
					}
					if len(m.Index[idxKeys[i]]) == 0 {
						delete(m.Index, idxKeys[i])
					}
				}
			}
		}
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
			logger.Error(context.Background(), "K8S_META_HANDLE_ALARM", "handle k8s meta with idx rules error", err)
			return nil
		}
		result = append(result, idxKeys...)
	}
	return result
}
