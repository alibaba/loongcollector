package k8smeta

import (
	"context"
	"fmt"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	controllerConfig "sigs.k8s.io/controller-runtime/pkg/client/config"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

var metaManager *MetaManager

var onceManager sync.Once

type MetaCache interface {
	Get(key []string) map[string][]*ObjectWrapper
	GetSize() int
	GetQueueSize() int
	List() []*ObjectWrapper
	Filter(filterFunc func(*ObjectWrapper) bool, limit int) []*ObjectWrapper
	RegisterSendFunc(key string, sendFunc SendFunc, interval int)
	UnRegisterSendFunc(key string)
	init(*kubernetes.Clientset)
	watch(stopCh <-chan struct{})
}

type FlushCh struct {
	Ch         chan *K8sMetaEvent
	ConfigName string
}

type MetaManager struct {
	clientset  *kubernetes.Clientset
	restConfig *rest.Config
	stopCh     chan struct{}

	ready atomic.Bool

	metadataHandler *metadataHandler
	cacheMap        map[string]MetaCache
	cacheMu         sync.RWMutex
	linkGenerator   *LinkGenerator
	linkRegisterMap map[string][]string
	registerLock    sync.RWMutex

	nsPolicyMu     sync.RWMutex
	nsPolicyRegs   []nsPolicyReg
	nextNsPolicyID int

	// self metrics
	projectNames       map[string]int
	metricRecord       selfmonitor.MetricsRecord
	addEventCount      selfmonitor.CounterMetric
	updateEventCount   selfmonitor.CounterMetric
	deleteEventCount   selfmonitor.CounterMetric
	cacheResourceGauge selfmonitor.GaugeMetric
	queueSizeGauge     selfmonitor.GaugeMetric
	httpRequestCount   selfmonitor.CounterMetric
	httpAvgDelayMs     selfmonitor.CounterMetric
	httpMaxDelayMs     selfmonitor.GaugeMetric
}

func GetMetaManagerInstance() *MetaManager {
	onceManager.Do(func() {
		metaManager = &MetaManager{
			stopCh: make(chan struct{}),
		}
		metaManager.metadataHandler = newMetadataHandler(metaManager)
		metaManager.cacheMap = make(map[string]MetaCache)
		for _, resource := range AllResources {
			metaManager.cacheMap[resource] = newK8sMetaCache(metaManager.stopCh, resource)
		}
		metaManager.linkGenerator = NewK8sMetaLinkGenerator(metaManager.cacheMap)
		metaManager.linkRegisterMap = make(map[string][]string)
		metaManager.projectNames = make(map[string]int)
	})
	return metaManager
}

// RegisterCustomResourceCollector registers a dynamic informer cache keyed by cfg.EntityType (after Normalize).
// Optional PodLink registers Pod→CR link generation for PodLinkTypeForEntity(EntityType).
// Safe before or after Init; if Init already ran, the dynamic client is attached immediately.
func (m *MetaManager) RegisterCustomResourceCollector(cfg CustomResourceCollectorConfig) error {
	if err := cfg.Normalize(); err != nil {
		return err
	}
	m.cacheMu.Lock()
	if exist, ok := m.cacheMap[cfg.EntityType]; ok {
		if uc, isCR := exist.(*crUnifiedCache); isCR {
			uc.SetGVRIfNotStarted(cfg.ToGVR())
		}
	} else {
		m.cacheMap[cfg.EntityType] = newCRUnifiedCache(m.stopCh, cfg.EntityType, cfg.ToGVR())
		if m.restConfig != nil {
			if uc, ok := m.cacheMap[cfg.EntityType].(*crUnifiedCache); ok {
				if err := uc.setRESTConfig(m.restConfig); err != nil {
					logger.Error(context.Background(), K8sMetaUnifyErrorCode, "setRESTConfig for custom resource cache", err, "entityType", cfg.EntityType)
				}
			}
		}
	}
	m.cacheMu.Unlock()

	if cfg.PodLink != nil {
		m.linkGenerator.registerPodCRLink(PodLinkTypeForEntity(cfg.EntityType), &podCRLinkRuntime{
			entityType:          cfg.EntityType,
			ownerKind:           cfg.PodLink.OwnerKind,
			ownerAPIGroupSubstr: firstNonEmpty(cfg.PodLink.OwnerAPIGroupContains, cfg.APIGroup),
			podLabelKey:         cfg.PodLink.PodLabelKey,
		})
	}
	return nil
}

// EnsureCustomResourceInformerStarted starts the dynamic informer for an EntityType if the REST config is ready.
func (m *MetaManager) EnsureCustomResourceInformerStarted(entityType string) {
	m.cacheMu.RLock()
	c, ok := m.cacheMap[entityType]
	m.cacheMu.RUnlock()
	if !ok {
		return
	}
	if uc, ok := c.(*crUnifiedCache); ok {
		uc.EnsureWatchStarted()
	}
}

func (m *MetaManager) Init(configPath string) (err error) {
	var config *rest.Config
	if len(configPath) > 0 {
		config, err = clientcmd.BuildConfigFromFlags("", configPath)
		if err != nil {
			return err
		}
	} else {
		// 创建 Kubernetes 客户端配置
		config = controllerConfig.GetConfigOrDie()
	}
	// set protobuf support for config
	config.AcceptContentTypes = "application/vnd.kubernetes.protobuf,application/json"
	config.ContentType = "application/vnd.kubernetes.protobuf"
	config.UserAgent = EntityCollectorUserAgent

	// 创建 Kubernetes 客户端
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		return err
	}
	m.clientset = clientset
	m.restConfig = config

	// CR dynamic client: setRESTConfig errors are logged only; graceful degradation, built-in meta still starts.
	m.cacheMu.Lock()
	for _, c := range m.cacheMap {
		if uc, ok := c.(*crUnifiedCache); ok {
			if err := uc.setRESTConfig(config); err != nil {
				logger.Error(context.Background(), K8sMetaUnifyErrorCode, "setRESTConfig for custom resource cache at Init", err, "resourceType", uc.resourceType)
			}
		}
	}
	m.cacheMu.Unlock()

	m.metricRecord = selfmonitor.MetricsRecord{}
	m.addEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaAddEventTotal)
	m.updateEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaUpdateEventTotal)
	m.deleteEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaDeleteEventTotal)
	m.cacheResourceGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaCacheSize)
	m.queueSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaQueueSize)
	m.httpRequestCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPRequestTotal)
	m.httpAvgDelayMs = selfmonitor.NewAverageMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPAvgDelayMs)
	m.httpMaxDelayMs = selfmonitor.NewMaxMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPMaxDelayMs)

	go func() {
		startTime := time.Now()
		m.cacheMu.RLock()
		caches := make([]struct {
			name string
			c    MetaCache
		}, 0, len(m.cacheMap))
		for resourceType, cache := range m.cacheMap {
			caches = append(caches, struct {
				name string
				c    MetaCache
			}{resourceType, cache})
		}
		m.cacheMu.RUnlock()
		for _, ent := range caches {
			logger.Info(context.Background(), ent.name, "init success")
			ent.c.init(clientset)
		}
		m.ready.Store(true)
		logger.Info(context.Background(), "init k8s meta manager", "success", "latancy (ms)", fmt.Sprintf("%d", time.Since(startTime).Milliseconds()))
	}()
	return nil
}

func (m *MetaManager) Run(stopCh chan struct{}) {
	m.stopCh = stopCh
	m.runServer()
}

func (m *MetaManager) IsReady() bool {
	return m.ready.Load()
}

func (m *MetaManager) RegisterSendFunc(projectName, configName, resourceType string, sendFunc SendFunc, interval int) {
	m.cacheMu.RLock()
	cache, ok := m.cacheMap[resourceType]
	m.cacheMu.RUnlock()
	if ok {
		cache.RegisterSendFunc(configName, func(events []*K8sMetaEvent) {
			defer panicRecover()
			sendFunc(events)
			m.registerLock.RLock()
			for _, linkType := range m.linkRegisterMap[configName] {
				if strings.HasPrefix(linkType, resourceType) {
					linkEvents := m.linkGenerator.GenerateLinks(events, linkType)
					if linkEvents != nil {
						sendFunc(linkEvents)
					}
				}
			}
			m.registerLock.RUnlock()
		}, interval)
		m.registerLock.Lock()
		if cnt, ok := m.projectNames[projectName]; ok {
			m.projectNames[projectName] = cnt + 1
		} else {
			m.projectNames[projectName] = 1
		}
		m.registerLock.Unlock()
		return
	}
	// register link
	if !isEntity(resourceType) {
		m.registerLock.Lock()
		if _, ok := m.linkRegisterMap[configName]; !ok {
			m.linkRegisterMap[configName] = make([]string, 0)
		}
		m.linkRegisterMap[configName] = append(m.linkRegisterMap[configName], resourceType)
		m.registerLock.Unlock()
	} else {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "resourceType not support", resourceType)
	}
}

func (m *MetaManager) UnRegisterAllSendFunc(projectName, configName string) {
	m.cacheMu.RLock()
	caches := make([]MetaCache, 0, len(m.cacheMap))
	for _, cache := range m.cacheMap {
		caches = append(caches, cache)
	}
	m.cacheMu.RUnlock()
	for _, cache := range caches {
		cache.UnRegisterSendFunc(configName)
	}
	m.registerLock.Lock()
	if cnt, ok := m.projectNames[projectName]; ok {
		if cnt == 1 {
			delete(m.projectNames, projectName)
		} else {
			m.projectNames[projectName] = cnt - 1
		}
	}
	delete(m.linkRegisterMap, configName)
	m.registerLock.Unlock()
}

func GetMetaManagerMetrics() []map[string]string {
	manager := GetMetaManagerInstance()
	if manager == nil || !manager.IsReady() {
		return nil
	}
	// cache
	queueSize := 0
	cacheSize := 0
	manager.cacheMu.RLock()
	for _, cache := range manager.cacheMap {
		queueSize += cache.GetQueueSize()
		cacheSize += cache.GetSize()
	}
	manager.cacheMu.RUnlock()
	manager.queueSizeGauge.Set(float64(queueSize))
	manager.cacheResourceGauge.Set(float64(cacheSize))
	// set labels
	manager.registerLock.RLock()
	projectName := make([]string, 0)
	projectName = append(projectName, *flags.DefaultLogProject)
	for name := range manager.projectNames {
		projectName = append(projectName, name)
	}
	manager.registerLock.RUnlock()
	manager.metricRecord.Labels = []selfmonitor.LabelPair{
		{
			Key:   selfmonitor.MetricLabelKeyMetricCategory,
			Value: selfmonitor.MetricLabelValueMetricCategoryRunner,
		},
		{
			Key:   selfmonitor.MetricLabelKeyClusterID,
			Value: *flags.ClusterID,
		},
		{
			Key:   selfmonitor.MetricLabelKeyRunnerName,
			Value: selfmonitor.MetricLabelValueRunnerNameK8sMeta,
		},
		{
			Key:   selfmonitor.MetricLabelKeyProject,
			Value: strings.Join(projectName, " "),
		},
	}

	return []map[string]string{
		manager.metricRecord.ExportMetricRecords(),
	}
}

func (m *MetaManager) runServer() {
	go m.metadataHandler.K8sServerRun(m.stopCh)
}

func isEntity(resourceType string) bool {
	return !strings.Contains(resourceType, LINK_SPLIT_CHARACTER)
}
