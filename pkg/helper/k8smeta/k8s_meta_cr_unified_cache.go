package k8smeta

import (
	"context"
	"fmt"
	"sync"
	"time"

	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/dynamic/dynamicinformer"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	"github.com/alibaba/ilogtail/pkg/logger"
)

// crUnifiedCache is a MetaCache for one third-party API resource (dynamic informer + unstructured objects).
type crUnifiedCache struct {
	metaStore *DeferredDeletionMetaStore
	eventCh   chan *K8sMetaEvent
	stopCh    chan struct{}

	resourceType string
	gvr          schema.GroupVersionResource

	mu             sync.Mutex
	dynamicClient  dynamic.Interface
	informer       cache.SharedIndexInformer
	factory        dynamicinformer.DynamicSharedInformerFactory
	watchStarted   bool
	watchStartOnce sync.Once

	authFailMu     sync.Mutex
	authFailCount  int
	authGiveUp     chan struct{}
	authGiveUpOnce sync.Once
}

func newCRUnifiedCache(stopCh chan struct{}, resourceType string, gvr schema.GroupVersionResource) *crUnifiedCache {
	c := &crUnifiedCache{
		stopCh:       stopCh,
		resourceType: resourceType,
		gvr:          gvr,
		eventCh:      make(chan *K8sMetaEvent, 100),
	}
	c.metaStore = NewDeferredDeletionMetaStore(c.eventCh, stopCh, 120, cache.MetaNamespaceKeyFunc, generateCommonKey)
	c.authGiveUp = make(chan struct{})
	return c
}

func (c *crUnifiedCache) init(_ *kubernetes.Clientset) {
	// Built-in clientset unused; dynamic client is wired via setRESTConfig from MetaManager.Init.
}

// SetGVRIfNotStarted updates the informer GVR before the dynamic informer starts; later calls are ignored.
func (c *crUnifiedCache) SetGVRIfNotStarted(gvr schema.GroupVersionResource) {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.watchStarted {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "custom resource informer already started; GVR change ignored", "gvr", gvr.String())
		return
	}
	c.gvr = gvr
}

func restConfigForDynamicClient(cfg *rest.Config) *rest.Config {
	if cfg == nil {
		return nil
	}
	d := *cfg
	// Dynamic client + unstructured ListWatch expect JSON; shared *rest.Config uses protobuf for clientset.
	d.ContentType = runtime.ContentTypeJSON
	d.AcceptContentTypes = runtime.ContentTypeJSON
	return &d
}

func (c *crUnifiedCache) setRESTConfig(cfg *rest.Config) error {
	if cfg == nil {
		return fmt.Errorf("nil rest.Config")
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	dyn, err := dynamic.NewForConfig(restConfigForDynamicClient(cfg))
	if err != nil {
		return err
	}
	c.dynamicClient = dyn
	return nil
}

// EnsureWatchStarted starts the dynamic informer (once) when the dynamic client is ready.
// Important: never enter sync.Once when dynamicClient is nil — Once would still count as done and block forever.
func (c *crUnifiedCache) EnsureWatchStarted() {
	c.mu.Lock()
	ready := c.dynamicClient != nil
	c.mu.Unlock()
	if !ready {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "dynamic client not ready, skip custom resource informer; ensure MetaManager.Init completed")
		return
	}
	c.watchStartOnce.Do(func() {
		c.mu.Lock()
		if c.dynamicClient == nil {
			c.mu.Unlock()
			return
		}
		c.metaStore.Start()
		c.factory = dynamicinformer.NewDynamicSharedInformerFactory(c.dynamicClient, time.Hour)
		c.informer = c.factory.ForResource(c.gvr).Informer()
		_, _ = c.informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				defer panicRecover()
				u := objectToUnstructured(obj)
				if u == nil {
					return
				}
				trimWorkflowObjectForCache(u)
				now := time.Now().Unix()
				c.eventCh <- &K8sMetaEvent{
					EventType: EventTypeAdd,
					Object: &ObjectWrapper{
						ResourceType:      c.resourceType,
						Raw:               u,
						FirstObservedTime: now,
						LastObservedTime:  now,
					},
				}
				metaManager.addEventCount.Add(1)
			},
			UpdateFunc: func(_, obj interface{}) {
				defer panicRecover()
				u := objectToUnstructured(obj)
				if u == nil {
					return
				}
				trimWorkflowObjectForCache(u)
				now := time.Now().Unix()
				c.eventCh <- &K8sMetaEvent{
					EventType: EventTypeUpdate,
					Object: &ObjectWrapper{
						ResourceType:      c.resourceType,
						Raw:               u,
						FirstObservedTime: now,
						LastObservedTime:  now,
					},
				}
				metaManager.updateEventCount.Add(1)
			},
			DeleteFunc: func(obj interface{}) {
				defer panicRecover()
				u := objectToUnstructured(obj)
				if u == nil {
					return
				}
				trimWorkflowObjectForCache(u)
				c.eventCh <- &K8sMetaEvent{
					EventType: EventTypeDelete,
					Object: &ObjectWrapper{
						ResourceType:     c.resourceType,
						Raw:              u,
						LastObservedTime: time.Now().Unix(),
					},
				}
				metaManager.deleteEventCount.Add(1)
			},
		})
		if err := c.informer.SetWatchErrorHandler(func(_ *cache.Reflector, err error) {
			if err != nil {
				logger.Error(context.Background(), K8sMetaUnifyErrorCode, "resourceType", c.resourceType, "watchError", err)
				if isInformerAuthFailure(err) {
					c.authFailMu.Lock()
					c.authFailCount++
					n := c.authFailCount
					c.authFailMu.Unlock()
					if n >= informerAuthFailureStopAfter {
						c.authGiveUpOnce.Do(func() {
							logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "stopping dynamic informer after repeated RBAC/auth errors (no further retries)", "resourceType", c.resourceType, "gvr", c.gvr.String(), "failures", n)
							close(c.authGiveUp)
						})
					}
				}
			}
		}); err != nil {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "fail to set dynamic informer watch error handler", err)
		}
		c.watchStarted = true
		inf := c.informer
		gvr := c.gvr
		c.mu.Unlock()

		mergedStop := make(chan struct{})
		go func() {
			select {
			case <-c.stopCh:
			case <-c.authGiveUp:
			}
			close(mergedStop)
		}()
		go c.factory.Start(mergedStop)
		go func() {
			for {
				if cache.WaitForCacheSync(mergedStop, inf.HasSynced) {
					logger.Info(context.Background(), "dynamic informer cache synced", "gvr", gvr.String())
					return
				}
				select {
				case <-mergedStop:
					logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "dynamic informer cache sync aborted", "gvr", gvr.String())
					return
				default:
				}
				logger.Error(context.Background(), K8sMetaUnifyErrorCode, "dynamic informer cache sync timeout", "gvr", gvr.String())
				time.Sleep(time.Second)
			}
		}()
	})
}

func (c *crUnifiedCache) watch(<-chan struct{}) {}

func (c *crUnifiedCache) Get(key []string) map[string][]*ObjectWrapper {
	return c.metaStore.Get(key)
}

func (c *crUnifiedCache) GetSize() int {
	return len(c.metaStore.Items)
}

func (c *crUnifiedCache) GetQueueSize() int {
	return len(c.eventCh)
}

func (c *crUnifiedCache) List() []*ObjectWrapper {
	return c.metaStore.List()
}

func (c *crUnifiedCache) Filter(filterFunc func(*ObjectWrapper) bool, limit int) []*ObjectWrapper {
	return c.metaStore.Filter(filterFunc, limit)
}

func (c *crUnifiedCache) RegisterSendFunc(key string, sendFunc SendFunc, interval int) {
	c.EnsureWatchStarted()
	c.metaStore.RegisterSendFunc(key, sendFunc, interval)
	logger.Debug(context.Background(), "register send func", c.resourceType)
}

func (c *crUnifiedCache) UnRegisterSendFunc(key string) {
	c.metaStore.UnRegisterSendFunc(key)
}

func objectToUnstructured(obj interface{}) *unstructured.Unstructured {
	if u, ok := obj.(*unstructured.Unstructured); ok {
		return u.DeepCopy()
	}
	if tombstone, ok := obj.(cache.DeletedFinalStateUnknown); ok {
		if u, ok := tombstone.Obj.(*unstructured.Unstructured); ok {
			return u.DeepCopy()
		}
	}
	return nil
}

// trimWorkflowObjectForCache drops spec and managedFields to limit memory; metadata + status remain for linking and whitelisted export.
func trimWorkflowObjectForCache(u *unstructured.Unstructured) {
	if u == nil {
		return
	}
	unstructured.RemoveNestedField(u.Object, "spec")
	unstructured.RemoveNestedField(u.Object, "metadata", "managedFields")
}
