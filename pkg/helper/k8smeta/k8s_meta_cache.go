package k8smeta

import (
	"context"
	"fmt"
	"time"

	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	v1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
	storage "k8s.io/api/storage/v1"
	meta "k8s.io/apimachinery/pkg/api/meta"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	"sigs.k8s.io/controller-runtime/pkg/client/apiutil"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type k8sMetaCache struct {
	metaStore *DeferredDeletionMetaStore
	clientset *kubernetes.Clientset

	eventCh chan *K8sMetaEvent
	stopCh  chan struct{}

	resourceType string
	schema       *runtime.Scheme
}

func newK8sMetaCache(stopCh chan struct{}, resourceType string) *k8sMetaCache {
	idxRules := getIdxRules(resourceType)
	m := &k8sMetaCache{}
	m.eventCh = make(chan *K8sMetaEvent, 100)
	m.stopCh = stopCh
	m.metaStore = NewDeferredDeletionMetaStore(m.eventCh, m.stopCh, 120, cache.MetaNamespaceKeyFunc, idxRules...)
	m.resourceType = resourceType
	m.schema = runtime.NewScheme()
	_ = v1.AddToScheme(m.schema)
	_ = batch.AddToScheme(m.schema)
	_ = app.AddToScheme(m.schema)
	_ = networking.AddToScheme(m.schema)
	_ = storage.AddToScheme(m.schema)
	return m
}

func (m *k8sMetaCache) init(clientset *kubernetes.Clientset) {
	m.clientset = clientset
	m.metaStore.Start()
	m.watch(m.stopCh)
}

func (m *k8sMetaCache) Get(key []string) map[string][]*ObjectWrapper {
	return m.metaStore.Get(key)
}

func (m *k8sMetaCache) GetSize() int {
	return len(m.metaStore.Items)
}

func (m *k8sMetaCache) GetQueueSize() int {
	return len(m.eventCh)
}

func (m *k8sMetaCache) List() []*ObjectWrapper {
	return m.metaStore.List()
}

func (m *k8sMetaCache) Filter(filterFunc func(*ObjectWrapper) bool, limit int) []*ObjectWrapper {
	return m.metaStore.Filter(filterFunc, limit)
}

func (m *k8sMetaCache) RegisterSendFunc(key string, sendFunc SendFunc, interval int) {
	m.metaStore.RegisterSendFunc(key, sendFunc, interval)
	logger.Debug(context.Background(), "register send func", m.resourceType)
}

func (m *k8sMetaCache) UnRegisterSendFunc(key string) {
	m.metaStore.UnRegisterSendFunc(key)
}

func (m *k8sMetaCache) watch(stopCh <-chan struct{}) {
	factory, informer := m.getFactoryInformer()
	if informer == nil {
		return
	}
	informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			nowTime := time.Now().Unix()
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeAdd,
				Object: &ObjectWrapper{
					ResourceType:      m.resourceType,
					Raw:               m.preProcess(obj),
					FirstObservedTime: nowTime,
					LastObservedTime:  nowTime,
				},
			}
			metaManager.addEventCount.Add(1)
		},
		UpdateFunc: func(oldObj interface{}, obj interface{}) {
			nowTime := time.Now().Unix()
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeUpdate,
				Object: &ObjectWrapper{
					ResourceType:      m.resourceType,
					Raw:               m.preProcess(obj),
					FirstObservedTime: nowTime,
					LastObservedTime:  nowTime,
				},
			}
			metaManager.updateEventCount.Add(1)
		},
		DeleteFunc: func(obj interface{}) {
			m.eventCh <- &K8sMetaEvent{
				EventType: EventTypeDelete,
				Object: &ObjectWrapper{
					ResourceType:     m.resourceType,
					Raw:              m.preProcess(obj),
					LastObservedTime: time.Now().Unix(),
				},
			}
			metaManager.deleteEventCount.Add(1)
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

func (m *k8sMetaCache) getFactoryInformer() (informers.SharedInformerFactory, cache.SharedIndexInformer) {
	var factory informers.SharedInformerFactory
	switch m.resourceType {
	case POD:
		factory = informers.NewSharedInformerFactory(m.clientset, time.Hour*24)
	default:
		factory = informers.NewSharedInformerFactory(m.clientset, time.Hour*1)
	}
	var informer cache.SharedIndexInformer
	switch m.resourceType {
	case POD:
		informer = factory.Core().V1().Pods().Informer()
	case SERVICE:
		informer = factory.Core().V1().Services().Informer()
	case DEPLOYMENT:
		informer = factory.Apps().V1().Deployments().Informer()
	case REPLICASET:
		informer = factory.Apps().V1().ReplicaSets().Informer()
	case STATEFULSET:
		informer = factory.Apps().V1().StatefulSets().Informer()
	case DAEMONSET:
		informer = factory.Apps().V1().DaemonSets().Informer()
	case CRONJOB:
		informer = factory.Batch().V1().CronJobs().Informer()
	case JOB:
		informer = factory.Batch().V1().Jobs().Informer()
	case NODE:
		informer = factory.Core().V1().Nodes().Informer()
	case NAMESPACE:
		informer = factory.Core().V1().Namespaces().Informer()
	case CONFIGMAP:
		informer = factory.Core().V1().ConfigMaps().Informer()
	case PERSISTENTVOLUME:
		informer = factory.Core().V1().PersistentVolumes().Informer()
	case PERSISTENTVOLUMECLAIM:
		informer = factory.Core().V1().PersistentVolumeClaims().Informer()
	case STORAGECLASS:
		informer = factory.Storage().V1().StorageClasses().Informer()
	case INGRESS:
		informer = factory.Networking().V1().Ingresses().Informer()
	default:
		logger.Error(context.Background(), "ENTITY_PIPELINE_REGISTER_ERROR", "resourceType not support", m.resourceType)
		return factory, nil
	}
	return factory, informer
}

func getIdxRules(resourceType string) []IdxFunc {
	switch resourceType {
	case NODE:
		return []IdxFunc{generateNodeKey}
	case POD:
		return []IdxFunc{generateCommonKey, generatePodIPKey, generateContainerIDKey, generateHostIPKey}
	case SERVICE:
		return []IdxFunc{generateCommonKey, generateServiceIPKey}
	default:
		return []IdxFunc{generateCommonKey}
	}
}

func (m *k8sMetaCache) preProcess(obj interface{}) interface{} {
	switch m.resourceType {
	case POD:
		return m.preProcessPod(obj)
	default:
		return m.preProcessCommon(obj)
	}
}

func (m *k8sMetaCache) preProcessCommon(obj interface{}) interface{} {
	runtimeObj, ok := obj.(runtime.Object)
	if !ok {
		logger.Error(context.Background(), "K8S_META_PRE_PROCESS_ERROR", "object is not runtime object", obj)
	}
	metaObj, err := meta.Accessor(runtimeObj)
	if err != nil {
		logger.Error(context.Background(), "K8S_META_PRE_PROCESS_ERROR", "object is not meta object", err)
	}
	// fill empty kind
	if runtimeObj.GetObjectKind().GroupVersionKind().Empty() {
		gvk, err := apiutil.GVKForObject(runtimeObj, m.schema)
		if err != nil {
			logger.Error(context.Background(), "K8S_META_PRE_PROCESS_ERROR", "get GVK for object error", err)
			return obj
		}
		runtimeObj.GetObjectKind().SetGroupVersionKind(gvk)
	}
	// remove unnecessary annotations
	if metaObj.GetAnnotations() != nil {
		if _, ok := metaObj.GetAnnotations()["kubectl.kubernetes.io/last-applied-configuration"]; ok {
			metaObj.GetAnnotations()["kubectl.kubernetes.io/last-applied-configuration"] = ""
		}
	}
	return runtimeObj
}

func (m *k8sMetaCache) preProcessPod(obj interface{}) interface{} {
	m.preProcessCommon(obj)
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return obj
	}
	pod.ManagedFields = nil
	pod.Status.Conditions = nil
	pod.Spec.Tolerations = nil
	return pod
}

func generateCommonKey(obj interface{}) ([]string, error) {
	meta, err := meta.Accessor(obj)
	if err != nil {
		return []string{}, err
	}
	return []string{generateNameWithNamespaceKey(meta.GetNamespace(), meta.GetName())}, nil
}

func generateNodeKey(obj interface{}) ([]string, error) {
	node, err := meta.Accessor(obj)
	if err != nil {
		return []string{}, err
	}
	return []string{node.GetName()}, nil
}

func generateNameWithNamespaceKey(namespace, name string) string {
	return fmt.Sprintf("%s/%s", namespace, name)
}

func generatePodIPKey(obj interface{}) ([]string, error) {
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return []string{}, fmt.Errorf("object is not a pod")
	}
	return []string{pod.Status.PodIP}, nil
}

func generateContainerIDKey(obj interface{}) ([]string, error) {
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return []string{}, fmt.Errorf("object is not a pod")
	}
	result := make([]string, len(pod.Status.ContainerStatuses))
	for i, containerStatus := range pod.Status.ContainerStatuses {
		result[i] = truncateContainerID(containerStatus.ContainerID)
	}
	return result, nil
}

func generateHostIPKey(obj interface{}) ([]string, error) {
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return []string{}, fmt.Errorf("object is not a pod")
	}
	return []string{pod.Status.HostIP}, nil
}

func generateServiceIPKey(obj interface{}) ([]string, error) {
	svc, ok := obj.(*v1.Service)
	if !ok {
		return []string{}, fmt.Errorf("object is not a service")
	}
	results := make([]string, 0)
	for _, ip := range svc.Spec.ClusterIPs {
		if ip != "" {
			results = append(results, ip)
		}
	}
	for _, ip := range svc.Spec.ExternalIPs {
		if ip != "" {
			results = append(results, ip)
		}
	}
	if svc.Spec.LoadBalancerIP != "" {
		results = append(results, svc.Spec.LoadBalancerIP)
	}
	return results, nil
}
