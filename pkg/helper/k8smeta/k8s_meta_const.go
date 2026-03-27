package k8smeta

import (
	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	v1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
)

const (
	EntityCollectorUserAgent = "loongcollector-singleton"

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
	// CUSTOM_RESOURCE_ARGO_WORKFLOW is the unified MetaCache key / event ResourceType for Argo Workflow CR entities.
	CUSTOM_RESOURCE_ARGO_WORKFLOW = "customresource/argoproj.io/workflow"
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
	POD_ARGO_WORKFLOW        = "pod->customresource/argoproj.io/workflow"
	//revive:enable:var-naming

	// ArgoWorkflowKind is the Kubernetes kind for argoproj.io Workflow CRs.
	ArgoWorkflowKind = "Workflow"

	// DefaultArgoWorkflowAPIGroup is the Workflow CRD API group (ownerRef APIVersion match + dynamic informer Group).
	DefaultArgoWorkflowAPIGroup = "argoproj.io"
	// DefaultArgoWorkflowAPIVersion is the Workflow informer API version.
	DefaultArgoWorkflowAPIVersion = "v1alpha1"
	// DefaultArgoWorkflowResource is the Workflow informer resource name (plural).
	DefaultArgoWorkflowResource = "workflows"
	// DefaultArgoWorkflowPodLabelKey is the Pod label used as fallback to resolve Workflow name.
	DefaultArgoWorkflowPodLabelKey = "workflows.argoproj.io/workflow"

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

// ArgoWorkflowCollectorOptions configures Pod↔Workflow link matching and the Workflow dynamic informer GVR.
// A zero value means: use DefaultArgoWorkflowAPIGroup, DefaultArgoWorkflowAPIVersion, DefaultArgoWorkflowResource, DefaultArgoWorkflowPodLabelKey.
// Apply via MetaManager.ConfigureArgoWorkflowCollector before the first EnsureArgoWorkflowInformerStarted.
type ArgoWorkflowCollectorOptions struct {
	APIGroup            string
	APIVersion          string
	Resource            string
	PodWorkflowLabelKey string
}

// PodArgoWorkflow links a Pod to an Argo Workflow CR (unstructured).
type PodArgoWorkflow struct {
	Pod      *v1.Pod
	Workflow *unstructured.Unstructured
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
