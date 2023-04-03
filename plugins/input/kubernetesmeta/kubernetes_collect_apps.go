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
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/labels"
	apps "k8s.io/client-go/listers/apps/v1"

	"strconv"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

// collectDeployment list the kubernetes nodes by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectDeployment(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	deploymentLister := lister.(apps.DeploymentLister)
	deployments, err := deploymentLister.List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Deployment {
		nodes = make([]*helper.MetaNode, 0, len(deployments))
	}
	for _, d := range deployments {
		id := string(d.UID)
		if !in.DisableReportParents {
			s, _ := metav1.LabelSelectorAsSelector(d.Spec.Selector)
			in.addMatcher(d.Namespace, Deployment, newLabelMatcher(d.Name, id, s))
		}
		if in.Deployment {
			node := helper.NewMetaNode(id, Deployment).WithLabels(d.Labels).WithAttributes(make(helper.Attributes, 10))
			desiredReplicas := 1
			if d.Spec.Replicas != nil {
				desiredReplicas = int(*d.Spec.Replicas)
			}
			node.WithAttribute(KeyNamespace, d.Namespace).
				WithAttribute(KeyObservedGeneration, d.Status.ObservedGeneration).
				WithAttribute(KeyDesiredReplicas, desiredReplicas).
				WithAttribute(KeyReplicas, d.Status.Replicas).
				WithAttribute(KeyUpdatedReplicas, d.Status.UpdatedReplicas).
				WithAttribute(KeyAvailableReplicas, d.Status.AvailableReplicas).
				WithAttribute(KeyUnavailableReplicas, d.Status.UnavailableReplicas).
				WithAttribute(KeyStrategy, d.Spec.Strategy.Type)
			addCommonAttributes(&d.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}

// collectDaemonSet list the kubernetes daemonSets by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectDaemonSet(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	daemonSetLister := lister.(apps.DaemonSetLister)
	daemonSets, err := daemonSetLister.List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.DaemonSet {
		nodes = make([]*helper.MetaNode, 0, len(daemonSets))
	}
	for _, d := range daemonSets {
		id := string(d.UID)
		if !in.DisableReportParents {
			s, _ := metav1.LabelSelectorAsSelector(d.Spec.Selector)
			in.addMatcher(d.Namespace, DaemonSet, newLabelMatcher(d.Name, id, s))
		}
		if in.DaemonSet {
			node := helper.NewMetaNode(id, DaemonSet).WithLabels(d.Labels).WithAttributes(make(helper.Attributes, 8)).
				WithAttribute(KeyNamespace, d.Namespace).
				WithAttribute(KeyObservedGeneration, d.Status.ObservedGeneration).
				WithAttribute(KeyDesiredReplicas, d.Status.DesiredNumberScheduled).
				WithAttribute(KeyReplicas, d.Status.CurrentNumberScheduled).
				WithAttribute(KeyMisscheduledReplicas, d.Status.NumberMisscheduled)
			addCommonAttributes(&d.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}

// collectStatefulSet list the kubernetes statefulSets by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectStatefulSet(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	statefulSets, err := lister.(apps.StatefulSetLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.StatefulSet {
		nodes = make([]*helper.MetaNode, 0, len(statefulSets))
	}
	for _, d := range statefulSets {
		id := string(d.UID)
		if !in.DisableReportParents {
			s, _ := metav1.LabelSelectorAsSelector(d.Spec.Selector)
			in.addMatcher(d.Namespace, StatefulSet, newLabelMatcher(d.Name, id, s))
		}
		if in.StatefulSet {
			node := helper.NewMetaNode(id, StatefulSet).WithLabels(d.Labels).WithAttributes(make(helper.Attributes, 8))
			desiredReplicas := 1
			if d.Spec.Replicas != nil {
				desiredReplicas = int(*d.Spec.Replicas)
			}
			node.WithAttribute(KeyObservedGeneration, d.Status.ObservedGeneration).
				WithAttribute(KeyNamespace, d.Namespace).
				WithAttribute(KeyDesiredReplicas, strconv.Itoa(desiredReplicas)).
				WithAttribute(KeyReplicas, strconv.Itoa(int(d.Status.Replicas))).
				WithAttribute(KeyUpdatedReplicas, strconv.Itoa(int(d.Status.UpdatedReplicas)))
			addCommonAttributes(&d.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}
