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
	"fmt"
	"reflect"
	"regexp"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	api "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/labels"
	core "k8s.io/client-go/listers/core/v1"
)

var (
	deploymentPodReg = regexp.MustCompile(`^([\w\-]+)\-[0-9a-z]{9,10}\-[0-9a-z]{5}$`)
	setPodReg        = regexp.MustCompile(`^([\w\-]+)\-[0-9a-z]{5}$`)
)

func ExtractPodWorkloadName(name string) string {
	if name == "" {
		return ""
	}
	if res := deploymentPodReg.FindStringSubmatch(name); len(res) > 1 {
		return res[1]
	} else if res := setPodReg.FindStringSubmatch(name); len(res) > 1 {
		return res[1]
	} else {
		return name
	}
}

// collectPods list the kubernetes nodes by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectPods(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	pods, err := lister.(core.PodLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	nodes = make([]*helper.MetaNode, 0, len(pods))
	for _, p := range pods {
		node := helper.NewMetaNode(string(p.UID), Pod).WithLabels(p.Labels).
			WithParents(make([]string, 0, 4)).WithAttributes(make(helper.Attributes, 16))
		var totalRestartNum int32
		for i := 0; i < len(p.Status.ContainerStatuses); i++ {
			totalRestartNum += p.Status.ContainerStatuses[i].RestartCount
		}
		node.WithAttribute(KeyNamespace, p.Namespace).
			WithAttribute(KeyPhase, p.Status.Phase).
			WithAttribute(KeyPodIP, p.Status.PodIP).
			WithAttribute(KeyRestartCount, totalRestartNum).
			WithAttribute(KeyAddresses, p.Spec.NodeName).
			WithAttribute(KeyWorkload, ExtractPodWorkloadName(p.Name))
		if len(p.Spec.Volumes) > 0 {
			var claimNames []string
			// PVC name consist of lower case alphanumeric characters, "-" or "."
			// and must start and end with an alphanumeric character.
			for i := 0; i < len(p.Spec.Volumes); i++ {
				if p.Spec.Volumes[i].VolumeSource.PersistentVolumeClaim != nil {
					claimNames = append(claimNames, p.Spec.Volumes[i].VolumeSource.PersistentVolumeClaim.ClaimName)
				}
			}
			node.WithAttribute(KeyVolumeClaim, strings.Join(claimNames, ","))
		}
		if p.Spec.HostNetwork {
			node.WithAttribute(KeyHostNetwork, "true")
		}
		for i := 0; i < len(p.Spec.Containers); i++ {
			prefix := fmt.Sprintf("container.%d.", i)
			node.WithAttribute(prefix+KeyContainerName, p.Spec.Containers[i].Name).WithAttribute(prefix+KeyImageName, p.Spec.Containers[i].Image)
		}
		addCommonAttributes(&p.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}

// collectServices list the kubernetes Services by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectNodes(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	k8sNodes, err := lister.(core.NodeLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Node {
		nodes = make([]*helper.MetaNode, 0, len(k8sNodes))
	}
	for _, n := range k8sNodes {
		id := string(n.UID)
		if !in.DisableReportParents {
			in.nodeMapping[n.Name] = id
		}
		if in.Node {
			node := helper.NewMetaNode(id, Node).WithAttributes(make(helper.Attributes, 20)).WithLabels(n.Labels).
				WithAttribute(KeyKernelVersion, n.Status.NodeInfo.KernelVersion).
				WithAttribute(KeyArchitecture, n.Status.NodeInfo.Architecture).
				WithAttribute(KeyBootID, n.Status.NodeInfo.BootID).
				WithAttribute(KeyContainerRuntimeVersion, n.Status.NodeInfo.ContainerRuntimeVersion).
				WithAttribute(KeyKubeProxyVersion, n.Status.NodeInfo.KubeProxyVersion).
				WithAttribute(KeyKubeletVersion, n.Status.NodeInfo.KubeletVersion).
				WithAttribute(KeyMachineID, n.Status.NodeInfo.MachineID).
				WithAttribute(KeyOperatingSystem, n.Status.NodeInfo.OperatingSystem).
				WithAttribute(KeyOSImage, n.Status.NodeInfo.OSImage).
				WithAttribute(KeySystemUUID, n.Status.NodeInfo.SystemUUID)

			if num, ok := n.Status.Allocatable.Cpu().AsInt64(); ok {
				node.WithAttribute(KeyAllocatableCPU, num)
			}
			if num, ok := n.Status.Allocatable.Memory().AsInt64(); ok {
				node.WithAttribute(KeyAllocatableMem, num)
			}
			if num, ok := n.Status.Allocatable.StorageEphemeral().AsInt64(); ok {
				node.WithAttribute(KeyAllocatableEphemeralStorage, num)
			}
			if num, ok := n.Status.Allocatable.Pods().AsInt64(); ok {
				node.WithAttribute(KeyAllocatablePods, num)
			}
			if num, ok := n.Status.Allocatable.Storage().AsInt64(); ok {
				node.WithAttribute(KeyAllocatableStorage, num)
			}
			for i, address := range n.Status.Addresses {
				node.WithAttribute(KeyAddresses+"."+strconv.Itoa(i)+"."+string(address.Type), address.Address)
			}
			for index, t := range n.Spec.Taints {
				node.WithAttribute(KeyTaints+"."+strconv.Itoa(index), t.ToString())
			}
			if n.Spec.Unschedulable {
				node.WithAttribute(KeyUnschedulable, true)
			}
			n.Status.Allocatable.Cpu().AsInt64()
			addCommonAttributes(&n.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}

// collectServices list the kubernetes Services by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectServices(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	services, err := lister.(core.ServiceLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Service {
		nodes = make([]*helper.MetaNode, 0, len(services))
	}
	servicePortString := func(p *api.ServicePort) string {
		if p.NodePort == 0 {
			return fmt.Sprintf("%d/%s", p.Port, p.Protocol)
		}
		return fmt.Sprintf("%d:%d/%s", p.Port, p.NodePort, p.Protocol)
	}
	for _, s := range services {
		id := string(s.UID)
		if !in.DisableReportParents {
			in.addMatcher(s.Namespace, Service, newLabelMatcher(s.Name, id, labels.SelectorFromSet(s.Spec.Selector)))
		}
		if in.Service {
			node := helper.NewMetaNode(id, Service).WithAttributes(make(helper.Attributes, 8)).WithLabels(s.Labels).
				WithAttribute(KeyNamespace, s.Namespace).
				WithAttribute(KeyClusterIP, s.Spec.ClusterIP).
				WithAttribute(KeyType, s.Spec.Type)
			for i := 0; i < len(s.Spec.Ports); i++ {
				res := make([]string, 0, len(s.Spec.Ports))
				for i := 0; i < len(s.Spec.Ports); i++ {
					res = append(res, servicePortString(&s.Spec.Ports[i]))
				}
				node.WithAttribute(KeyPorts, strings.Join(res, ","))
			}
			if s.Spec.LoadBalancerIP != "" {
				node.WithAttribute(KeyLoadBalancerIP, s.Spec.LoadBalancerIP)
			}
			addCommonAttributes(&s.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}

// collectNamespaces list the kubernetes Namespaces by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectNamespaces(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.Namespace {
		return
	}
	namespaces, err := lister.(core.NamespaceLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	nodes = make([]*helper.MetaNode, 0, len(namespaces))
	for _, ns := range namespaces {
		node := helper.NewMetaNode(string(ns.UID), Namespace).WithAttributes(make(helper.Attributes, 2)).WithLabels(ns.Labels)
		addCommonAttributes(&ns.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}

// collectPersistentVolumeClaims list the kubernetes PersistentVolumeClaims by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectPersistentVolumeClaims(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.PersistentVolumeClaim {
		return
	}
	persistentVolumeClaims, err := lister.(core.PersistentVolumeClaimLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	getStorageClass := func(pvc *api.PersistentVolumeClaim) string {
		// Use Beta storage class annotation first
		storageClassName := pvc.Annotations[api.BetaStorageClassAnnotation]
		if storageClassName != "" {
			return storageClassName
		}
		if pvc.Spec.StorageClassName != nil {
			storageClassName = *pvc.Spec.StorageClassName
		}
		return storageClassName
	}
	nodes = make([]*helper.MetaNode, 0, len(persistentVolumeClaims))
	for _, pvc := range persistentVolumeClaims {
		node := helper.NewMetaNode(string(pvc.UID), PersistentVolumeClaim).WithAttributes(make(helper.Attributes, 8)).WithLabels(pvc.Labels).
			WithAttribute(KeyNamespace, pvc.Namespace).
			WithAttribute(KeyPhase, pvc.Status.Phase).
			WithAttribute(KeyStorageClass, getStorageClass(pvc)).
			WithAttribute(KeyVolume, pvc.Spec.VolumeName)
		if quantity, ok := pvc.Spec.Resources.Requests[api.ResourceStorage]; ok {
			node.WithAttribute(KeyCapacity, quantity.String())
		}
		if len(pvc.Spec.AccessModes) > 0 {
			node.WithAttribute(KeyAccessMode, pvc.Spec.AccessModes[0])
		}
		addCommonAttributes(&pvc.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}

// collectPersistentVolume list the kubernetes PersistentVolumes by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectPersistentVolume(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.PersistentVolume {
		return
	}
	persistentVolumes, err := lister.(core.PersistentVolumeLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	getDriver := func(pv *api.PersistentVolume) string {
		persistentVolumeSource := reflect.ValueOf(pv.Spec.PersistentVolumeSource)
		// persistentVolumeSource will have exactly one field which won't be nil,
		// depending on the type of backing driver used to create the persistent volume
		// Iterate over the fields and return the non-nil field name
		for i := 0; i < persistentVolumeSource.NumField(); i++ {
			if !reflect.ValueOf(persistentVolumeSource.Field(i).Interface()).IsNil() {
				return persistentVolumeSource.Type().Field(i).Name
			}
		}
		return ""
	}
	nodes = make([]*helper.MetaNode, 0, len(persistentVolumes))
	for _, pv := range persistentVolumes {
		node := helper.NewMetaNode(string(pv.UID), PersistentVolume).WithAttributes(make(helper.Attributes, 8)).WithLabels(pv.Labels).
			WithAttribute(KeyPhase, pv.Status.Phase).
			WithAttribute(KeyStorageClass, pv.Spec.StorageClassName).
			WithAttribute(KeyCapacity, pv.Spec.Capacity.Storage().String())
		if pv.Spec.ClaimRef != nil {
			node.WithAttribute(KeyVolumeClaim, pv.Spec.ClaimRef.Name)
		}
		if len(pv.Spec.AccessModes) > 0 {
			node.WithAttribute(KeyAccessMode, pv.Spec.AccessModes[0])
		}
		if driver := getDriver(pv); driver != "" {
			node.WithAttribute(KeyStorageDriver, driver)
		}
		addCommonAttributes(&pv.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}

// collectConfigmaps list the kubernetes configmaps by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectConfigmaps(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.Configmap {
		return
	}

	configmaps, err := lister.(core.ConfigMapLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	nodes = make([]*helper.MetaNode, 0, len(configmaps))
	for _, cm := range configmaps {
		node := helper.NewMetaNode(string(cm.UID), Configmap).
			WithAttributes(make(helper.Attributes, 8)).
			WithLabels(cm.Labels).
			WithAttribute(KeyNamespace, cm.Namespace)
		if cm.Immutable != nil {
			node.WithAttribute(KeyImmutable, cm.Immutable)
		}
		addCommonAttributes(&cm.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}

// collectSecrets list the kubernetes secrets by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectSecrets(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.Secret {
		return
	}
	secrets, err := lister.(core.SecretLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	nodes = make([]*helper.MetaNode, 0, len(secrets))
	for _, s := range secrets {
		node := helper.NewMetaNode(string(s.UID), Secret).
			WithAttributes(make(helper.Attributes, 8)).
			WithLabels(s.Labels).
			WithAttribute(KeyNamespace, s.Namespace)
		if s.Immutable != nil {
			node.WithAttribute(KeyImmutable, s.Immutable)
		}
		addCommonAttributes(&s.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}
