package k8smeta

import (
	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	v1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
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
	POD_NAMESPACE                   = "pod->namesapce"
	SERVICE_NAMESPACE               = "service->namesapce"
	DEPLOYMENT_NAMESPACE            = "deployment->namesapce"
	DAEMONSET_NAMESPACE             = "daemonset->namesapce"
	STATEFULSET_NAMESPACE           = "statefulset->namesapce"
	CONFIGMAP_NAMESPACE             = "configmap->namesapce"
	JOB_NAMESPACE                   = "job->namesapce"
	CRONJOB_NAMESPACE               = "cronjob->namesapce"
	PERSISTENTVOLUMECLAIM_NAMESPACE = "persistentvolumeclaim->namesapce"
	INGRESS_NAMESPACE               = "ingress->namesapce"
	//revive:disable:var-naming
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

const (
	EventTypeAdd            = "add"
	EventTypeUpdate         = "update"
	EventTypeDelete         = "delete"
	EventTypeDeferredDelete = "deferredDelete"
	EventTypeTimer          = "timer"
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
