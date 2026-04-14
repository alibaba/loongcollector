package k8smeta

import (
	"context"
	"fmt"
	"sync"
	"time"

	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/client-go/discovery"
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

	mu              sync.Mutex
	dynamicClient   dynamic.Interface
	discoveryClient discovery.DiscoveryInterface
	informer        cache.SharedIndexInformer
	factory         dynamicinformer.DynamicSharedInformerFactory
	watchStarted    bool
	watchStartOnce  sync.Once

	giveUpMu    sync.Mutex
	giveUpCount int
	giveUpCh    chan struct{}
	giveUpOnce  sync.Once
}

func newCRUnifiedCache(stopCh chan struct{}, resourceType string, gvr schema.GroupVersionResource) *crUnifiedCache {
	c := &crUnifiedCache{
		stopCh:       stopCh,
		resourceType: resourceType,
		gvr:          gvr,
		eventCh:      make(chan *K8sMetaEvent, 100),
	}
	c.metaStore = NewDeferredDeletionMetaStore(c.eventCh, stopCh, 120, cache.MetaNamespaceKeyFunc, generateCommonKey)
	c.giveUpCh = make(chan struct{})
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
	disco, derr := discovery.NewDiscoveryClientForConfig(restConfigForDynamicClient(cfg))
	if derr != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "discovery client for custom resource informer unavailable; will not pre-check GVR", "resourceType", c.resourceType, "error", derr)
		c.discoveryClient = nil
	} else {
		c.discoveryClient = disco
	}
	return nil
}

// EnsureWatchStarted starts the dynamic informer (once) when the dynamic client is ready.
// Important: never enter sync.Once when dynamicClient is nil.
func (c *crUnifiedCache) EnsureWatchStarted() {
	c.mu.Lock()
	dyn := c.dynamicClient
	c.mu.Unlock()
	if dyn == nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "dynamic client not ready, skip custom resource informer; ensure MetaManager.Init completed")
		return
	}
	c.watchStartOnce.Do(func() {
		c.mu.Lock()
		c.metaStore.Start()
		gvr := c.gvr
		if !gvrDiscoveryAvailable(c.discoveryClient, gvr) {
			c.watchStarted = true
			c.mu.Unlock()
			return
		}
		c.factory = dynamicinformer.NewDynamicSharedInformerFactory(dyn, time.Hour)
		c.informer = c.factory.ForResource(c.gvr).Informer()
		_, err := c.informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				defer panicRecover()
				u := trimmedCRCopyFromInformer(obj, c.resourceType)
				if u == nil {
					return
				}
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
				u := trimmedCRCopyFromInformer(obj, c.resourceType)
				if u == nil {
					return
				}
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
				u := trimmedCRCopyFromInformer(obj, c.resourceType)
				if u == nil {
					return
				}
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
		if err != nil {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "fail to add dynamic informer event handler", err, "resourceType", c.resourceType, "gvr", c.gvr.String())
		}
		if err := c.informer.SetWatchErrorHandler(func(_ *cache.Reflector, err error) {
			if err != nil {
				logger.Error(context.Background(), K8sMetaUnifyErrorCode, "resourceType", c.resourceType, "watchError", err)
				if isInformerGiveUpFailure(err) {
					c.giveUpMu.Lock()
					c.giveUpCount++
					n := c.giveUpCount
					c.giveUpMu.Unlock()
					if n >= informerGiveUpFailureThreshold {
						c.giveUpOnce.Do(func() {
							logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "stopping dynamic informer after repeated errors (RBAC/auth or missing API resource; no further retries)", "resourceType", c.resourceType, "gvr", c.gvr.String(), "failures", n)
							close(c.giveUpCh)
						})
					}
				}
			}
		}); err != nil {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "fail to set dynamic informer watch error handler", err)
		}
		c.watchStarted = true
		inf := c.informer
		c.mu.Unlock()

		mergedStop := make(chan struct{})
		go func() {
			select {
			case <-c.stopCh:
			case <-c.giveUpCh:
			}
			close(mergedStop)
		}()
		go c.factory.Start(mergedStop)
		go func() {
			backoff := time.Second
			const maxBackoff = 10 * time.Second
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
				logger.Error(context.Background(), K8sMetaUnifyErrorCode, "dynamic informer cache sync timeout", "gvr", gvr.String(), "nextRetryIn", backoff.String())
				time.Sleep(backoff)
				if backoff < maxBackoff {
					backoff *= 2
					if backoff > maxBackoff {
						backoff = maxBackoff
					}
				}
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

// trimmedCRCopyFromInformer builds a detached object for the meta cache without full-object DeepCopy:
// copies apiVersion/kind, metadata (without managedFields), and status via NestedFieldCopy — spec is omitted.
// This avoids mutating the informer-shared *unstructured.Unstructured and skips copying large spec blobs.
func trimmedCRCopyFromInformer(obj interface{}, resourceType string) *unstructured.Unstructured {
	switch t := obj.(type) {
	case *unstructured.Unstructured:
		return buildTrimmedCRCopy(t, resourceType)
	case cache.DeletedFinalStateUnknown:
		if u, ok := t.Obj.(*unstructured.Unstructured); ok {
			return buildTrimmedCRCopy(u, resourceType)
		}
	}
	return nil
}

func buildTrimmedCRCopy(u *unstructured.Unstructured, resourceType string) *unstructured.Unstructured {
	if u == nil {
		return nil
	}
	out := &unstructured.Unstructured{Object: make(map[string]interface{})}
	if gv := u.GetAPIVersion(); gv != "" {
		out.SetAPIVersion(gv)
	}
	if k := u.GetKind(); k != "" {
		out.SetKind(k)
	}
	metaVal, metaFound, metaErr := unstructured.NestedFieldCopy(u.Object, "metadata")
	if metaErr != nil {
		logger.Debug(context.Background(), K8sMetaUnifyErrorCode, "nested copy metadata for CR cache", metaErr, "resourceType", resourceType)
	} else if metaFound {
		if metaMap, ok := metaVal.(map[string]interface{}); ok {
			delete(metaMap, "managedFields")
			if err := unstructured.SetNestedMap(out.Object, metaMap, "metadata"); err != nil {
				logger.Debug(context.Background(), K8sMetaUnifyErrorCode, "set metadata on trimmed CR", err, "resourceType", resourceType)
			}
		}
	}
	statusVal, statusFound, statusErr := unstructured.NestedFieldCopy(u.Object, "status")
	if statusErr != nil {
		logger.Debug(context.Background(), K8sMetaUnifyErrorCode, "nested copy status for CR cache", statusErr, "resourceType", resourceType)
	} else if statusFound {
		if err := unstructured.SetNestedField(out.Object, statusVal, "status"); err != nil {
			logger.Debug(context.Background(), K8sMetaUnifyErrorCode, "set status on trimmed CR", err, "resourceType", resourceType)
		}
	}
	return out
}
