package k8smeta

import (
	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	batchv1beta1 "k8s.io/api/batch/v1beta1"
	v1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

const (
	// entity type
	POD                   = "pod"
	SERVICE               = "service"
	DEPLOYMENT            = "deployment"
	REPLICASET            = "replicaset"
	STATEFULSET           = "statefulset"
	DAEMONSET             = "daemonset"
	CRONJOB               = "cronjob"
	JOB                   = "job"
	NODE                  = "node"
	NAMESPACE             = "namespace"
	CONFIGMAP             = "configmap"
	PERSISTENTVOLUME      = "persistentvolume"
	PERSISTENTVOLUMECLAIM = "persistentvolumeclaim"
	STORAGECLASS          = "storageclass"
	INGRESS               = "ingress"
	CONTAINER             = "container"
	// entity link type, the direction is from resource which will be trigger to linked resource
	//revive:disable:var-naming
	LINK_SPLIT_CHARACTER     = "->"
	POD_NODE                 = "pod->node"
	POD_DEPLOYMENT           = "pod->deployment"
	POD_REPLICASET           = "pod->replicaset"
	REPLICASET_DEPLOYMENT    = "replicaset->deployment"
	POD_STATEFULSET          = "pod->statefulset"
	POD_DAEMONSET            = "pod->daemonset"
	JOB_CRONJOB              = "job->cronjob"
	POD_JOB                  = "pod->job"
	POD_PERSISENTVOLUMECLAIN = "pod->persistentvolumeclaim"
	POD_CONFIGMAP            = "pod->configmap"
	POD_SERVICE              = "pod->service"
	POD_CONTAINER            = "pod->container"
	INGRESS_SERVICE          = "ingress->service"
	//revive:enable:var-naming

	// add namespace link
	//revive:disable:var-naming
	POD_NAMESPACE                   = "pod->namespace"
	SERVICE_NAMESPACE               = "service->namespace"
	DEPLOYMENT_NAMESPACE            = "deployment->namespace"
	DAEMONSET_NAMESPACE             = "daemonset->namespace"
	STATEFULSET_NAMESPACE           = "statefulset->namespace"
	CONFIGMAP_NAMESPACE             = "configmap->namespace"
	JOB_NAMESPACE                   = "job->namespace"
	CRONJOB_NAMESPACE               = "cronjob->namespace"
	PERSISTENTVOLUMECLAIM_NAMESPACE = "persistentvolumeclaim->namespace"
	INGRESS_NAMESPACE               = "ingress->namespace"
	//revive:disable:var-naming
)

const (
	K8S_DEPLOYMENT_TYPE  = "Deployment"
	K8S_REPLICASET_TYPE  = "ReplicaSet"
	K8S_STATEFULSET_TYPE = "StatefulSet"
	K8S_DAEMONSET_TYPE   = "DaemonSet"
	K8S_CRONJOB_TYPE     = "CronJob"
	K8S_JOB_TYPE         = "Job"
)

// CronJobVersion represents the version of CronJob API being used
type CronJobVersion string

const (
	CronJobV1      CronJobVersion = "v1"
	CronJobV1Beta1 CronJobVersion = "v1beta1"
)

// CronJobWrapper wraps both v1 and v1beta1 CronJob types for unified processing
type CronJobWrapper struct {
	Version CronJobVersion
	V1      *batch.CronJob
	V1Beta1 *batchv1beta1.CronJob
}

// GetName returns the name of the CronJob regardless of version
func (cw *CronJobWrapper) GetName() string {
	if cw.V1 != nil {
		return cw.V1.Name
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Name
	}
	return ""
}

// GetNamespace returns the namespace of the CronJob regardless of version
func (cw *CronJobWrapper) GetNamespace() string {
	if cw.V1 != nil {
		return cw.V1.Namespace
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Namespace
	}
	return ""
}

// GetKind returns the kind of the CronJob regardless of version
func (cw *CronJobWrapper) GetKind() string {
	if cw.V1 != nil {
		return cw.V1.Kind
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Kind
	}
	return "CronJob"
}

// GetAPIVersion returns the API version of the CronJob
func (cw *CronJobWrapper) GetAPIVersion() string {
	if cw.V1 != nil {
		return cw.V1.APIVersion
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.APIVersion
	}
	return ""
}

// GetLabels returns the labels of the CronJob regardless of version
func (cw *CronJobWrapper) GetLabels() map[string]string {
	if cw.V1 != nil {
		return cw.V1.Labels
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Labels
	}
	return nil
}

// GetAnnotations returns the annotations of the CronJob regardless of version
func (cw *CronJobWrapper) GetAnnotations() map[string]string {
	if cw.V1 != nil {
		return cw.V1.Annotations
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Annotations
	}
	return nil
}

// GetCreationTimestamp returns the creation timestamp of the CronJob regardless of version
func (cw *CronJobWrapper) GetCreationTimestamp() metav1.Time {
	if cw.V1 != nil {
		return cw.V1.CreationTimestamp
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.CreationTimestamp
	}
	return metav1.Time{}
}

// GetSchedule returns the schedule of the CronJob regardless of version
func (cw *CronJobWrapper) GetSchedule() string {
	if cw.V1 != nil {
		return cw.V1.Spec.Schedule
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Spec.Schedule
	}
	return ""
}

// GetSuspend returns the suspend status of the CronJob regardless of version
func (cw *CronJobWrapper) GetSuspend() *bool {
	if cw.V1 != nil {
		return cw.V1.Spec.Suspend
	}
	if cw.V1Beta1 != nil {
		return cw.V1Beta1.Spec.Suspend
	}
	return nil
}

var AllResources = []string{
	POD,
	SERVICE,
	DEPLOYMENT,
	REPLICASET,
	STATEFULSET,
	DAEMONSET,
	CRONJOB,
	JOB,
	NODE,
	NAMESPACE,
	CONFIGMAP,
	PERSISTENTVOLUME,
	PERSISTENTVOLUMECLAIM,
	STORAGECLASS,
	INGRESS,
}

type PodNode struct {
	Pod  *v1.Pod
	Node *v1.Node
}

type PodDeployment struct {
	Pod        *v1.Pod
	Deployment *app.Deployment
}

type ReplicaSetDeployment struct {
	Deployment *app.Deployment
	ReplicaSet *app.ReplicaSet
}

type PodReplicaSet struct {
	ReplicaSet *app.ReplicaSet
	Pod        *v1.Pod
}

type PodStatefulSet struct {
	StatefulSet *app.StatefulSet
	Pod         *v1.Pod
}

type PodDaemonSet struct {
	DaemonSet *app.DaemonSet
	Pod       *v1.Pod
}

type JobCronJob struct {
	CronJob *batch.CronJob
	Job     *batch.Job
}

type PodJob struct {
	Job *batch.Job
	Pod *v1.Pod
}

type PodPersistentVolumeClaim struct {
	Pod                   *v1.Pod
	PersistentVolumeClaim *v1.PersistentVolumeClaim
}

type PodConfigMap struct {
	Pod       *v1.Pod
	ConfigMap *v1.ConfigMap
}

type PodService struct {
	Service *v1.Service
	Pod     *v1.Pod
}

type IngressService struct {
	Ingress *networking.Ingress
	Service *v1.Service
}

type PodContainer struct {
	Pod       *v1.Pod
	Container *v1.Container
}

type PodNamespace struct {
	Pod       *v1.Pod
	Namespace *v1.Namespace
}

type ServiceNamespace struct {
	Service   *v1.Service
	Namespace *v1.Namespace
}

type DeploymentNamespace struct {
	Deployment *app.Deployment
	Namespace  *v1.Namespace
}

type DaemonSetNamespace struct {
	DaemonSet *app.DaemonSet
	Namespace *v1.Namespace
}

type StatefulSetNamespace struct {
	StatefulSet *app.StatefulSet
	Namespace   *v1.Namespace
}

type ConfigMapNamespace struct {
	ConfigMap *v1.ConfigMap
	Namespace *v1.Namespace
}

type JobNamespace struct {
	Job       *batch.Job
	Namespace *v1.Namespace
}

type CronJobNamespace struct {
	CronJob   *batch.CronJob
	Namespace *v1.Namespace
}

type PersistentVolumeClaimNamespace struct {
	PersistentVolumeClaim *v1.PersistentVolumeClaim
	Namespace             *v1.Namespace
}

type IngressNamespace struct {
	Ingress   *networking.Ingress
	Namespace *v1.Namespace
}

const (
	EventTypeAdd            = "add"
	EventTypeUpdate         = "update"
	EventTypeDelete         = "delete"
	EventTypeDeferredDelete = "deferredDelete"
	EventTypeTimer          = "timer"

	K8sMetaUnifyErrorCode = "K8S_META_COLLECTOR_ERROR"
)

type PodMetadata struct {
	PodName      string            `json:"podName"`
	StartTime    int64             `json:"startTime"`
	Namespace    string            `json:"namespace"`
	WorkloadName string            `json:"workloadName"`
	WorkloadKind string            `json:"workloadKind"`
	Labels       map[string]string `json:"labels"`
	Envs         map[string]string `json:"envs"`
	Images       map[string]string `json:"images"`

	ServiceName  string   `json:"serviceName,omitempty"`
	ContainerIDs []string `json:"containerIDs,omitempty"`
	PodIP        string   `json:"podIP,omitempty"`
	IsDeleted    bool     `json:"-"`
}
