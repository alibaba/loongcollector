// Copyright 2021 iLogtail Authors
//
// Shared informer lifecycle helpers: merge global stop with give-up channel,
// Reflector watch-error give-up counting, and cache sync with exponential backoff.

package k8smeta

import (
	"context"
	"sync"
	"time"

	"k8s.io/client-go/tools/cache"

	"github.com/alibaba/ilogtail/pkg/logger"
)

// informerGiveUp holds state for merging shutdown with “give up” after repeated
// non-recoverable Reflector errors (see informerGiveUpFailureThreshold).
type informerGiveUp struct {
	mu    sync.Mutex
	count int
	ch    chan struct{}
	once  sync.Once
}

func newInformerGiveUp() *informerGiveUp {
	return &informerGiveUp{ch: make(chan struct{})}
}

// mergedStop returns a channel that closes when either globalStop or give-up fires.
func (g *informerGiveUp) mergedStop(globalStop <-chan struct{}) chan struct{} {
	merged := make(chan struct{})
	go func() {
		select {
		case <-globalStop:
		case <-g.ch:
		}
		close(merged)
	}()
	return merged
}

// watchErrorHandlerOpts configures logging for attachWatchErrorHandler.
type watchErrorHandlerOpts struct {
	ResourceType  string
	GVR           string // optional; when non-empty, included in Error and Warning logs
	GiveUpStopMsg string // Warning message body on give-up (first kv key in logger.Warning)
}

// attachWatchErrorHandler registers SetWatchErrorHandler with give-up counting.
func attachWatchErrorHandler(informer cache.SharedIndexInformer, g *informerGiveUp, o watchErrorHandlerOpts) error {
	return informer.SetWatchErrorHandler(func(_ *cache.Reflector, err error) {
		if err == nil {
			return
		}
		kvs := []interface{}{"resourceType", o.ResourceType, "watchError", err}
		if o.GVR != "" {
			kvs = append(kvs, "gvr", o.GVR)
		}
		logger.Error(context.Background(), K8sMetaUnifyErrorCode, kvs...)
		if !isInformerGiveUpFailure(err) {
			return
		}
		g.mu.Lock()
		g.count++
		n := g.count
		g.mu.Unlock()
		if n < informerGiveUpFailureThreshold {
			return
		}
		g.once.Do(func() {
			var wkvs []interface{}
			if o.GVR != "" {
				wkvs = []interface{}{
					o.GiveUpStopMsg, "resourceType", o.ResourceType,
					"gvr", o.GVR,
					"failures", n,
				}
			} else {
				wkvs = []interface{}{
					o.GiveUpStopMsg, "resourceType", o.ResourceType,
					"failures", n,
				}
			}
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, wkvs...)
			close(g.ch)
		})
	})
}

// informerCacheSyncOpts configures logging for waitInformerCacheSync.
type informerCacheSyncOpts struct {
	ResourceType string
	// GVR non-empty selects CR-style log lines (dynamic informer + gvr) and logs success on sync.
	GVR string
}

// waitInformerCacheSync blocks until hasSynced or mergedStop is closed, with exponential backoff
// between WaitForCacheSync polls (same as previous k8sMetaCache.watch / crUnifiedCache loops).
func waitInformerCacheSync(mergedStop <-chan struct{}, hasSynced cache.InformerSynced, o informerCacheSyncOpts) {
	backoff := time.Second
	const maxBackoff = 10 * time.Second
	for {
		if cache.WaitForCacheSync(mergedStop, hasSynced) {
			if o.GVR != "" {
				logger.Info(context.Background(), "dynamic informer cache synced", "gvr", o.GVR)
			}
			return
		}
		select {
		case <-mergedStop:
			if o.GVR != "" {
				logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "dynamic informer cache sync aborted", "gvr", o.GVR)
			} else {
				logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "informer cache sync aborted", "resourceType", o.ResourceType)
			}
			return
		default:
		}
		if o.GVR != "" {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "dynamic informer cache sync timeout", "gvr", o.GVR, "nextRetryIn", backoff.String())
		} else {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "service cache sync timeout", "resourceType", o.ResourceType, "nextRetryIn", backoff.String())
		}
		time.Sleep(backoff)
		if backoff < maxBackoff {
			backoff *= 2
			if backoff > maxBackoff {
				backoff = maxBackoff
			}
		}
	}
}
