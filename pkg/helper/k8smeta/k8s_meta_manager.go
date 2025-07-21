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
	clientset *kubernetes.Clientset
	stopCh    chan struct{}

	ready atomic.Bool

	metadataHandler *metadataHandler
	cacheMap        map[string]MetaCache
	linkGenerator   *LinkGenerator
	linkRegisterMap map[string][]string
	registerLock    sync.RWMutex

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

	//details metrics
	podSizeGauge              selfmonitor.GaugeMetric
	nodeSizeGauge             selfmonitor.GaugeMetric
	serviceSizeGauge          selfmonitor.GaugeMetric
	deploymentSizeGauge       selfmonitor.GaugeMetric
	daemonsetSizeGauge        selfmonitor.GaugeMetric
	statefulSetSizeGauge      selfmonitor.GaugeMetric
	configMapSizeGauge        selfmonitor.GaugeMetric
	jobSizeGauge              selfmonitor.GaugeMetric
	cronJobSizeGauge          selfmonitor.GaugeMetric
	namespaceSizeGauge        selfmonitor.GaugeMetric
	persistentVolumeSizeGauge selfmonitor.GaugeMetric
	pvcSizeGauge              selfmonitor.GaugeMetric
	storageClassSizeGauge     selfmonitor.GaugeMetric
	ingressSizeGauge          selfmonitor.GaugeMetric
	replicasetSizeGauge       selfmonitor.GaugeMetric
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
	// 创建 Kubernetes 客户端
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		return err
	}
	m.clientset = clientset

	m.metricRecord = selfmonitor.MetricsRecord{}
	m.addEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaAddEventTotal)
	m.updateEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaUpdateEventTotal)
	m.deleteEventCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaDeleteEventTotal)
	m.cacheResourceGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaCacheSize)
	m.queueSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaQueueSize)
	m.httpRequestCount = selfmonitor.NewCounterMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPRequestTotal)
	m.httpAvgDelayMs = selfmonitor.NewAverageMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPAvgDelayMs)
	m.httpMaxDelayMs = selfmonitor.NewMaxMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaHTTPMaxDelayMs)

	//add k8s detail metrics
	m.podSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaPodSize)
	m.nodeSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaNodeSize)
	m.serviceSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaServiceSize)
	m.deploymentSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaDeploymentSize)
	m.daemonsetSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaDaemonsetSize)
	m.statefulSetSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaStatefulSetSize)
	m.configMapSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaConfigMapSize)
	m.jobSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaJobSize)
	m.cronJobSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaCronJobSize)
	m.namespaceSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaNamspaceSize)
	m.pvcSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaPVCSize)
	m.persistentVolumeSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaPersistentVolumeSize)
	m.storageClassSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaStorageClassSize)
	m.ingressSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaIngressSize)
	m.replicasetSizeGauge = selfmonitor.NewGaugeMetricAndRegister(&m.metricRecord, selfmonitor.MetricRunnerK8sMetaReplicaSetSize)

	go func() {
		startTime := time.Now()
		for _, cache := range m.cacheMap {
			cache.init(clientset)
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
	if cache, ok := m.cacheMap[resourceType]; ok {
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
		logger.Error(context.Background(), "ENTITY_PIPELINE_REGISTER_ERROR", "resourceType not support", resourceType)
	}
}

func (m *MetaManager) UnRegisterAllSendFunc(projectName, configName string) {
	for _, cache := range m.cacheMap {
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
	for _, cache := range manager.cacheMap {
		queueSize += cache.GetQueueSize()
		cacheSize += cache.GetSize()
	}
	manager.queueSizeGauge.Set(float64(queueSize))
	manager.cacheResourceGauge.Set(float64(cacheSize))

	for name, cache := range manager.cacheMap {
		switch name {
		case POD:
			manager.podSizeGauge.Set(float64(cache.GetSize()))
		case NODE:
			manager.nodeSizeGauge.Set(float64(cache.GetSize()))
		case SERVICE:
			manager.serviceSizeGauge.Set(float64(cache.GetSize()))
		case DEPLOYMENT:
			manager.deploymentSizeGauge.Set(float64(cache.GetSize()))
		case DAEMONSET:
			manager.daemonsetSizeGauge.Set(float64(cache.GetSize()))
		case STATEFULSET:
			manager.statefulSetSizeGauge.Set(float64(cache.GetSize()))
		case CONFIGMAP:
			manager.configMapSizeGauge.Set(float64(cache.GetSize()))
		case JOB:
			manager.jobSizeGauge.Set(float64(cache.GetSize()))
		case CRONJOB:
			manager.cronJobSizeGauge.Set(float64(cache.GetSize()))
		case NAMESPACE:
			manager.namespaceSizeGauge.Set(float64(cache.GetSize()))
		case PERSISTENTVOLUME:
			manager.persistentVolumeSizeGauge.Set(float64(cache.GetSize()))
		case PERSISTENTVOLUMECLAIM:
			manager.pvcSizeGauge.Set(float64(cache.GetSize()))
		case STORAGECLASS:
			manager.storageClassSizeGauge.Set(float64(cache.GetSize()))
		case INGRESS:
			manager.ingressSizeGauge.Set(float64(cache.GetSize()))
		case REPLICASET:
			manager.replicasetSizeGauge.Set(float64(cache.GetSize()))
		}

	}

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
