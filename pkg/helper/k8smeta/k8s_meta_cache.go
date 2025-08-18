package k8smeta

import (
	"context"
	"fmt"
	"time"

	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	batchv1beta1 "k8s.io/api/batch/v1beta1"
	v1 "k8s.io/api/core/v1"
	extensionsV1beta1 "k8s.io/api/extensions/v1beta1"
	networking "k8s.io/api/networking/v1"
	storage "k8s.io/api/storage/v1"
	meta "k8s.io/apimachinery/pkg/api/meta"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	"sigs.k8s.io/controller-runtime/pkg/client/apiutil"

	"github.com/alibaba/ilogtail/pkg/logger"
)

const hostIPIndexPrefix = "host/"

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
	_ = batchv1beta1.AddToScheme(m.schema)
	_ = app.AddToScheme(m.schema)
	_ = extensionsV1beta1.AddToScheme(m.schema)
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
	defer panicRecover()
	factory, informer := m.getFactoryInformer()
	if informer == nil {
		return
	}
	informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			defer panicRecover()
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
			defer panicRecover()
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
			defer panicRecover()
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
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "service cache sync timeout")
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
		informer = m.getCronJobInformer(factory)
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
		informer = m.getIngressInformer(factory)
	default:
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "resourceType not support", m.resourceType)
		return factory, nil
	}
	// add watch error handler
	err := informer.SetWatchErrorHandler(func(r *cache.Reflector, err error) {
		if err != nil {
			logger.Error(context.Background(), K8sMetaUnifyErrorCode, "resourceType", m.resourceType, "watchError", err)
		}
	})
	if err != nil {
		logger.Error(context.Background(), K8sMetaUnifyErrorCode, "fail to handle watch error handler", err)
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
	case CRONJOB:
		return m.preProcessCronJob(obj)
	case INGRESS:
		return m.preProcessIngress(obj)
	default:
		return m.preProcessCommon(obj)
	}
}

func (m *k8sMetaCache) preProcessCommon(obj interface{}) interface{} {
	runtimeObj, ok := obj.(runtime.Object)
	if !ok {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "object is not runtime object", obj)
		return obj
	}
	metaObj, err := meta.Accessor(runtimeObj)
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "object is not meta object", err)
		return obj
	}
	// fill empty kind
	if runtimeObj.GetObjectKind().GroupVersionKind().Empty() {
		gvk, err := apiutil.GVKForObject(runtimeObj, m.schema)
		if err != nil {
			logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "get GVK for object error", err)
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
	processedObj := m.preProcessCommon(obj)
	pod, ok := processedObj.(*v1.Pod)
	if !ok {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "object is not pod after common preprocessing", processedObj)
		return processedObj
	}
	pod.ManagedFields = []metav1.ManagedFieldsEntry{}
	pod.Status.Conditions = []v1.PodCondition{}
	pod.Spec.Tolerations = []v1.Toleration{}
	return pod
}

func (m *k8sMetaCache) preProcessCronJob(obj interface{}) interface{} {
	// 尝试处理v1 CronJob
	if cronJob, ok := obj.(*batch.CronJob); ok {
		return m.preProcessCommon(cronJob)
	}

	// 尝试处理v1beta1 CronJob，转换为v1格式
	if cronJob, ok := obj.(*batchv1beta1.CronJob); ok {
		// 转换为v1格式，保持与现有代码的兼容性
		v1CronJob := &batch.CronJob{
			ObjectMeta: metav1.ObjectMeta{
				Name:              cronJob.Name,
				Namespace:         cronJob.Namespace,
				Labels:            cronJob.Labels,
				Annotations:       cronJob.Annotations,
				CreationTimestamp: cronJob.CreationTimestamp,
			},
			Spec: batch.CronJobSpec{
				Schedule: cronJob.Spec.Schedule,
				Suspend:  cronJob.Spec.Suspend,
			},
		}
		return m.preProcessCommon(v1CronJob)
	}

	// 如果都不是，返回原始对象
	return m.preProcessCommon(obj)
}

func (m *k8sMetaCache) preProcessIngress(obj interface{}) interface{} {
	// 尝试处理v1 Ingress
	if ingress, ok := obj.(*networking.Ingress); ok {
		return m.preProcessCommon(ingress)
	}

	// 尝试处理v1beta1 Ingress，转换为v1格式
	if ingress, ok := obj.(*extensionsV1beta1.Ingress); ok {
		// 转换为v1格式，保持与现有代码的兼容性
		v1Ingress := &networking.Ingress{
			ObjectMeta: metav1.ObjectMeta{
				Name:              ingress.Name,
				Namespace:         ingress.Namespace,
				Labels:            ingress.Labels,
				Annotations:       ingress.Annotations,
				CreationTimestamp: ingress.CreationTimestamp,
			},
			Spec: networking.IngressSpec{
				IngressClassName: ingress.Spec.IngressClassName,
			},
		}
		return m.preProcessCommon(v1Ingress)
	}

	// 如果都不是，返回原始对象
	return m.preProcessCommon(obj)
}

func generateCommonKey(obj interface{}) ([]string, error) {
	meta, err := meta.Accessor(obj)
	if err != nil {
		return []string{}, err
	}
	return []string{generateNameWithNamespaceKey(meta.GetNamespace(), meta.GetName())}, nil
}

// CronJobInformer 在 client-go 1.26+下不仅提供batch/v1（1.21以上）支持还提供batch/v1beta1（1.21以下）支持，因此可以兼容1.21以下版本的k8s
func (m *k8sMetaCache) getCronJobInformer(factory informers.SharedInformerFactory) cache.SharedIndexInformer {
	if m.clientset == nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "clientset is nil")
		return nil
	}
	// 1. 探测 v1 是否支持当前k8s集群
	resourceList, err := m.clientset.Discovery().ServerResourcesForGroupVersion("batch/v1")
	if err == nil && containsResource(resourceList.APIResources, "cronjobs") {
		logger.Info(context.Background(), "Using CronJob v1 API")
		return factory.Batch().V1().CronJobs().Informer()
	}
	//2. 如不支持v1，那么 fallback到 v1beta1
	resourceList, err = m.clientset.Discovery().ServerResourcesForGroupVersion("batch/v1beta1")
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode,
			"Neither batch/v1 nor batch/v1beta1 CronJob API found", "error", err)
		return nil
	}
	if !containsResource(resourceList.APIResources, "cronjobs") {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode,
			"CronJob API not found in both v1 and v1beta1")
		return nil
	}
	logger.Info(context.Background(), "Using CronJob v1beta1 API")
	return factory.Batch().V1beta1().CronJobs().Informer()
}

// IngressInformer 在 client-go 1.26+下不仅提供networking.k8s.io/v1（1.19以上）支持还提供extensions/v1beta1（1.19以下）支持，因此可以兼容1.19以下版本的k8s
func (m *k8sMetaCache) getIngressInformer(factory informers.SharedInformerFactory) cache.SharedIndexInformer {
	if m.clientset == nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode, "clientset is nil")
		return nil
	}
	// 1. 探测 networking.k8s.io/v1 是否支持当前k8s集群
	resourceList, err := m.clientset.Discovery().ServerResourcesForGroupVersion("networking.k8s.io/v1")
	if err == nil && containsResource(resourceList.APIResources, "ingresses") {
		logger.Info(context.Background(), "Using Ingress networking.k8s.io/v1 API")
		return factory.Networking().V1().Ingresses().Informer()
	}

	// 2. 如不支持networking.k8s.io/v1，那么 fallback到 extensions/v1beta1
	resourceList, err = m.clientset.Discovery().ServerResourcesForGroupVersion("extensions/v1beta1")
	if err != nil {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode,
			"Neither networking.k8s.io/v1 nor extensions/v1beta1 Ingress API found", "error", err)
		return nil
	}
	if !containsResource(resourceList.APIResources, "ingresses") {
		logger.Warning(context.Background(), K8sMetaUnifyErrorCode,
			"Ingress API not found in both networking.k8s.io/v1 and extensions/v1beta1")
		return nil
	}
	logger.Info(context.Background(), "Using Ingress extensions/v1beta1 API")
	return factory.Extensions().V1beta1().Ingresses().Informer()
}

// helper: 判断资源列表里是否包含指定名称
func containsResource(resources []metav1.APIResource, name string) bool {
	for _, r := range resources {
		if r.Name == name {
			return true
		}
	}
	return false
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
	return []string{addHostIPIndexPrefex(pod.Status.HostIP)}, nil
}

func addHostIPIndexPrefex(ip string) string {
	return hostIPIndexPrefix + ip
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
