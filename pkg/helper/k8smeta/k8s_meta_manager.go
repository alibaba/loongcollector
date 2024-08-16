package k8smeta

import (
	"context"
	"sync"
	"sync/atomic"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	controllerConfig "sigs.k8s.io/controller-runtime/pkg/client/config"

	"github.com/alibaba/ilogtail/pkg/logger"
)

var metaManager *MetaManager

var onceManager sync.Once

type MetaCache interface {
	Get(key []string) map[string][]*ObjectWrapper
	RegisterSendFunc(key string, sendFunc SendFunc, interval int)
	UnRegisterSendFunc(key string)
	init()
	watch(stopCh <-chan struct{})
}

type FlushCh struct {
	Ch         chan *K8sMetaEvent
	ConfigName string
}

type MetaManager struct {
	clientset *kubernetes.Clientset
	stopCh    chan struct{}

	eventCh chan *K8sMetaEvent
	ready   atomic.Bool

	PodCache     *podCache
	ServiceCache *serviceCache
}

func GetMetaManagerInstance() *MetaManager {
	onceManager.Do(func() {
		metaManager = &MetaManager{
			stopCh:  make(chan struct{}),
			eventCh: make(chan *K8sMetaEvent, 1000),
		}
		metaManager.PodCache = newPodCache(metaManager.stopCh)
		metaManager.ServiceCache = newServiceCache(metaManager.stopCh)
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

	go func() {
		m.ServiceCache.clientset = m.clientset
		m.ServiceCache.init()
		m.PodCache.clientset = m.clientset
		m.PodCache.serviceMetaStore = m.ServiceCache.metaStore
		m.PodCache.init()
		m.ready.Store(true)
		logger.Info(context.Background(), "init k8s meta manager", "success")
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

func (m *MetaManager) RegisterSendFunc(configName string, resourceType string, sendFunc SendFunc, interval int) {
	switch resourceType {
	case POD:
		m.PodCache.RegisterSendFunc(configName, sendFunc, interval)
	case SERVICE:
		m.ServiceCache.RegisterSendFunc(configName, sendFunc, interval)
	default:
		logger.Error(context.Background(), "ENTITY_PIPELINE_REGISTER_ERROR", "resourceType not support", resourceType)
	}
}

func (m *MetaManager) UnRegisterSendFunc(configName string, resourceType string) {
	switch resourceType {
	case POD:
		m.PodCache.UnRegisterSendFunc(configName)
	case SERVICE:
		m.ServiceCache.UnRegisterSendFunc(configName)
	default:
		logger.Error(context.Background(), "ENTITY_PIPELINE_UNREGISTER_ERROR", "resourceType not support", resourceType)
	}
}

func (m *MetaManager) runServer() {
	metadataHandler := newMetadataHandler()
	go metadataHandler.K8sServerRun(m.stopCh)
}
