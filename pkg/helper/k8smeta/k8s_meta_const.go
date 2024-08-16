package k8smeta

import v1 "k8s.io/api/core/v1"

const (
	// entity type
	POD     = "pod"
	SERVICE = "service"
	// entity link type
	//revive:disable:var-naming
	LINK_SPLIT_CHARACTER = "_"
	POD_SERVICE          = "pod_service"
	//revive:enable:var-naming
)

const (
	EventTypeAdd            = "add"
	EventTypeUpdate         = "update"
	EventTypeDelete         = "delete"
	EventTypeDeferredDelete = "deferredDelete"
	EventTypeTimer          = "timer"
)

type PodMetadata struct {
	Namespace    string            `json:"namespace"`
	WorkloadName string            `json:"workloadName"`
	WorkloadKind string            `json:"workloadKind"`
	ServiceName  string            `json:"serviceName"`
	Labels       map[string]string `json:"labels"`
	Envs         map[string]string `json:"envs"`
	Images       map[string]string `json:"images"`
	IsDeleted    bool              `json:"-"`
}

type PodService struct {
	Pod     *v1.Pod
	Service *v1.Service
}
