package k8smeta

import (
	"context"
	"fmt"
	"time"

	v1 "k8s.io/api/core/v1"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type serviceCache struct {
	metaStore *DeferredDeletionMetaStore
	clientset *kubernetes.Clientset

	eventCh chan *K8sMetaEvent
	stopCh  chan struct{}
}

func newServiceCache(stopCh chan struct{}) *serviceCache {
	idxRules := []IdxFunc{
		generateServiceNameKey,
	}
	m := &serviceCache{}
	m.eventCh = make(chan *K8sMetaEvent, 100)
	m.stopCh = stopCh
	m.metaStore = NewDeferredDeletionMetaStore(m.eventCh, m.stopCh, 120, cache.MetaNamespaceKeyFunc, idxRules...)
	return m
}

func (m *serviceCache) init() {
	m.metaStore.Start()
	m.watch(m.stopCh)
}

func (m *serviceCache) Get(key []string) map[string][]*ObjectWrapper {
	return m.metaStore.Get(key)
}

func (m *serviceCache) RegisterSendFunc(key string, sendFunc SendFunc, interval int) {
	m.metaStore.RegisterSendFunc(key, sendFunc, interval)
}

func (m *serviceCache) UnRegisterSendFunc(key string) {
	m.metaStore.UnRegisterSendFunc(key)
}

func generateServiceNameKey(obj interface{}) ([]string, error) {
	service, ok := obj.(*v1.Service)
	if !ok {
		return []string{}, fmt.Errorf("object is not a service")
	}
	return []string{service.Name}, nil
}

func (m *serviceCache) watch(stopCh <-chan struct{}) {
	factory := informers.NewSharedInformerFactory(m.clientset, time.Hour*1)
	informer := factory.Core().V1().Services().Informer()
	informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			nowTime := time.Now().Unix()
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeAdd,
				Object: &ObjectWrapper{
					ResourceType:      SERVICE,
					Raw:               obj,
					FirstObservedTime: nowTime,
					LastObservedTime:  nowTime,
				},
			}
		},
		UpdateFunc: func(oldObj interface{}, obj interface{}) {
			nowTime := time.Now().Unix()
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeUpdate,
				Object: &ObjectWrapper{
					ResourceType:      SERVICE,
					Raw:               obj,
					FirstObservedTime: nowTime,
					LastObservedTime:  nowTime,
				},
			}
		},
		DeleteFunc: func(obj interface{}) {
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeDelete,
				Object: &ObjectWrapper{
					ResourceType:     SERVICE,
					Raw:              obj,
					LastObservedTime: time.Now().Unix(),
				},
			}
		},
	})
	go factory.Start(stopCh)
	// wait infinite for first cache sync success
	for {
		if !cache.WaitForCacheSync(stopCh, informer.HasSynced) {
			logger.Error(context.Background(), "K8S_META_CACHE_SYNC_TIMEOUT", "service cache sync timeout")
			time.Sleep(1 * time.Second)
		} else {
			break
		}
	}
}
