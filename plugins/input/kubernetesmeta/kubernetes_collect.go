// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package kubernetesmeta

import (
	"github.com/alibaba/ilogtail/helper"

	api "k8s.io/api/core/v1"
	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/labels"
)

// The kinds of kubernetes resources.
const (
	Pod                   = "Pod"
	Service               = "Service"
	Deployment            = "Deployment"
	DaemonSet             = "DaemonSet"
	StatefulSet           = "StatefulSet"
	Job                   = "Job"
	CronJob               = "CronJob"
	Node                  = "Node"
	Namespace             = "Namespace"
	PersistentVolume      = "PersistentVolume"
	PersistentVolumeClaim = "PersistentVolumeClaim"
	StorageClass          = "StorageClass"
	Ingress               = "Ingress"
	Configmap             = "ConfigMap"
	Secret                = "Secret"
)

// Keys used in meta node.
const (
	KeyName                        = "name"
	KeyNamespace                   = "namespace"
	KeyCreationTime                = "creation_time"
	KeyPhase                       = "phase"
	KeyClusterIP                   = "cluster_ip"
	KeyPodIP                       = "pod_ip"
	KeyType                        = "type"
	KeyPorts                       = "ports"
	KeyLoadBalancerIP              = "load_balancer_ip"
	KeyAddresses                   = "addresses"
	KeyWorkload                    = "workload"
	KeyTaints                      = "taints"
	KeyUnschedulable               = "unschedulable"
	KeyRestartCount                = "restart_count"
	KeyHostNetwork                 = "host_network"
	KeyContainerName               = "container_name"
	KeyImageName                   = "image_name"
	KeyStorageClass                = "storage_class"
	KeyVolume                      = "volume"
	KeyVolumeClaim                 = "volume_claim"
	KeyCapacity                    = "request_capacity"
	KeyStorageDriver               = "storage_driver"
	KeyAccessMode                  = "access_mode"
	KeyProvisioner                 = "provisioner"
	KeyObservedGeneration          = "observed_generation"
	KeyDesiredReplicas             = "desired_replicas"
	KeyReplicas                    = "replicas"
	KeyUpdatedReplicas             = "updated_replicas"
	KeyAvailableReplicas           = "available_replicas"
	KeyUnavailableReplicas         = "unavailable_replicas"
	KeyStrategy                    = "strategy"
	KeyMisscheduledReplicas        = "misscheduled_replicas"
	KeySchedule                    = "schedule"
	KeySuspend                     = "suspend"
	KeyActiveJobs                  = "active_jobs"
	KeyLastScheduleTime            = "last_schedule_time"
	KeyStartTime                   = "start_time"
	KeyCompletionTime              = "completion_time"
	KeyActive                      = "active"
	KeySucceeded                   = "succeeded"
	KeyFailed                      = "failed"
	KeyRules                       = "rules"
	KeyAllocatableCPU              = "allocatable_cpu"
	KeyAllocatableMem              = "allocatable_mem"
	KeyAllocatableEphemeralStorage = "allocatable_ephemeral_storage"
	KeyAllocatablePods             = "allocatable_pods"
	KeyAllocatableStorage          = "allocatable_storage"
	KeyKernelVersion               = "kernel_version"
	KeyArchitecture                = "architecture"
	KeyBootID                      = "boot_id"
	KeyContainerRuntimeVersion     = "container_runtime_version"
	KeyKubeProxyVersion            = "kube_proxy_version"
	KeyKubeletVersion              = "kubelet_version"
	KeyMachineID                   = "machine_id"
	KeyOperatingSystem             = "operating_system"
	KeyOSImage                     = "os_image"
	KeySystemUUID                  = "system_uuid"
	KeyImmutable                   = "immutable"
	KeyResourceVersion             = "resource_version"
)

type (
	// collectFunc collect the kubernetes core metadata by the lister of the informer.
	collectFunc func(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error)
	// collector wrapped the collectFunc with fixed name and lister.
	collector struct {
		name    string
		lister  interface{}
		collect collectFunc
	}
	// labelMatchers used to find the parents of pods.
	labelMatchers map[string][]*labelMatcher
	// labelMatcher stores the relationship between resource and selector.
	labelMatcher struct {
		name     string
		uid      string
		selector labels.Selector
	}
)

// newLabelMatcher create a new label matcher.
func newLabelMatcher(name, uid string, selector labels.Selector) *labelMatcher {
	return &labelMatcher{
		name, uid, selector,
	}
}

// newCollector create a new collector.
func newCollector(name string, lister interface{}, collect collectFunc) *collector {
	return &collector{name, lister, collect}
}

// addCommonAttributes wrapped each meta nodes with same common tags.
func addCommonAttributes(meta *v1.ObjectMeta, n *helper.MetaNode) *helper.MetaNode {
	n.WithAttribute(KeyName, meta.Name).WithAttribute(KeyCreationTime, meta.CreationTimestamp.Unix()).WithAttribute(KeyResourceVersion, meta.ResourceVersion)
	return n
}

func (in *InputKubernetesMeta) addMatcher(namespace, resourceType string, matcher *labelMatcher) {
	ns, ok := in.matchers[namespace]
	if !ok {
		group := make(labelMatchers, 16)
		in.matchers[namespace] = group
		ns = group
	}
	ns[resourceType] = append(ns[resourceType], matcher)
}

func (in *InputKubernetesMeta) addCronJobMapping(namespace, id, name string, refs []api.ObjectReference) {
	ns, ok := in.cronjobActives[namespace]
	if !ok {
		group := make(map[string][]api.ObjectReference, 16)
		in.cronjobActives[namespace] = group
		ns = group
	}
	ns[id] = refs
	in.cronjobMapping[id] = name
}

func (in *InputKubernetesMeta) addIngressMapping(namespace, id, name string, refs map[string]struct{}) {
	ns, ok := in.ingressRelationMapping[namespace]
	if !ok {
		group := make(map[string]map[string]struct{}, 16)
		in.ingressRelationMapping[namespace] = group
		ns = group
	}
	ns[id] = refs
	in.ingressMapping[id] = name
}

// free release the cache objects.
func (in *InputKubernetesMeta) free() {
	in.nodeMapping = make(map[string]string, len(in.nodeMapping))
	in.cronjobMapping = make(map[string]string, len(in.cronjobMapping))
	in.cronjobActives = make(map[string]map[string][]api.ObjectReference, len(in.cronjobActives))
	in.matchers = make(map[string]labelMatchers, len(in.matchers))
	in.ingressMapping = make(map[string]string, len(in.ingressMapping))
	in.ingressRelationMapping = make(map[string]map[string]map[string]struct{}, len(in.ingressRelationMapping))
}

// addPodParents construct the relationship between pods and downstream resources.
// When some dependencies founded, the relations would be added to the parents of pods.
func (in *InputKubernetesMeta) addPodParents(pods []*helper.MetaNode) {
	// add cronjob matchers
	for ns, jobRefs := range in.cronjobActives {
		jobs, ok := in.matchers[ns][Job]
		if !ok {
			continue
		}
		for id, references := range jobRefs {
			for i := 0; i < len(references); i++ {
				for _, selector := range jobs {
					if selector.uid == string(references[i].UID) {
						in.matchers[ns][CronJob] = append(in.matchers[ns][CronJob],
							newLabelMatcher(in.cronjobMapping[id], id, selector.selector))
					}
				}
			}
		}
	}
	// add parents
	for _, pod := range pods {
		nodeName := pod.Attributes[KeyAddresses].(string)
		delete(pod.Attributes, KeyAddresses)
		uid, ok := in.nodeMapping[nodeName]
		if ok {
			pod.WithParent(Node, uid, nodeName)
		}
		nsSelectors, ok := in.matchers[pod.Attributes[KeyNamespace].(string)]
		if !ok {
			continue
		}
		set := labels.Set(pod.Labels)
		for category, selectors := range nsSelectors {
			for _, s := range selectors {
				if s.selector.Matches(set) {
					pod.WithParent(category, s.uid, s.name)
				}
			}
		}
	}
}

// addJobParents construct the relationship between job and cronjob.
func (in *InputKubernetesMeta) addJobParents(jobs []*helper.MetaNode) {
	for _, job := range jobs {
		jns := job.Attributes[KeyNamespace].(string)
		group, ok := in.cronjobActives[jns]
		if !ok {
			continue
		}
		for id, references := range group {
			for _, reference := range references {
				if string(reference.UID) == job.ID {
					job.WithParent(CronJob, id, in.cronjobMapping[id])
				}
			}
		}
	}
}

// addServiceParents construct the relationship between service and ingress.
func (in *InputKubernetesMeta) addServiceParents(services []*helper.MetaNode) {
	for _, service := range services {
		sns := service.Attributes[KeyNamespace].(string)
		group, ok := in.ingressRelationMapping[sns]
		if !ok {
			continue
		}
		serviceName := service.Attributes[KeyName].(string)
		for id, names := range group {
			for k := range names {
				if k == serviceName {
					service.WithParent(Ingress, id, in.ingressMapping[id])
				}
			}
		}
	}
}
