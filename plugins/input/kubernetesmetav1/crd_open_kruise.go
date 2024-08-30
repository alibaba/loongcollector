// Copyright 2023 iLogtail Authors
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

package kubernetesmetav1

import (
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	kruisekubernetes "github.com/openkruise/kruise-api/client/clientset/versioned"
	informers "github.com/openkruise/kruise-api/client/informers/externalversions"
	"github.com/openkruise/kruise-api/client/listers/apps/v1alpha1"
	"github.com/openkruise/kruise-api/client/listers/apps/v1beta1"
	api "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/labels"
	restclient "k8s.io/client-go/rest"

	"strconv"
	"time"
)

func (in *InputKubernetesMeta) InitKruise(cfg *restclient.Config) {
	logger.Debug(in.context.GetRuntimeContext(), "enable kruise meta")
	kcs, err := kruisekubernetes.NewForConfig(cfg)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "enable kruise meta err", err)
		return
	}
	var options []informers.SharedInformerOption
	if len(in.SelectedNamespaces) == 0 {
		options = append(options, informers.WithNamespace(api.NamespaceAll))
	} else {
		for _, ns := range in.SelectedNamespaces {
			options = append(options, informers.WithNamespace(ns))
		}
	}
	in.kuriseInformerFactory = informers.NewSharedInformerFactoryWithOptions(kcs, time.Minute*30, options...)

	if in.Pod || in.Deployment {
		in.collectors = append(in.collectors, newCollector(Deployment, in.kuriseInformerFactory.Apps().V1alpha1().CloneSets().Lister(), in.collectKruiseCloneset))
	}
	if in.Pod || in.DaemonSet {
		in.collectors = append(in.collectors, newCollector(DaemonSet, in.kuriseInformerFactory.Apps().V1alpha1().DaemonSets().Lister(), in.collectKruiseDaemonSet))
	}
	if in.Pod || in.StatefulSet {
		in.collectors = append(in.collectors, newCollector(StatefulSet, in.kuriseInformerFactory.Apps().V1beta1().StatefulSets().Lister(), in.collectKruiseStatefulSet))
	}
	if in.Pod || in.CronJob || in.Job {
		in.collectors = append(in.collectors, newCollector(CronJob, in.kuriseInformerFactory.Apps().V1alpha1().AdvancedCronJobs().Lister(), in.collectKruiseCronJobs))
	}
	in.kuriseInformerFactory.Start(in.informerStopChan)
}

func (in *InputKubernetesMeta) collectKruiseCloneset(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	logger.Debug(in.context.GetRuntimeContext(), "collect kruise cloneset")
	clonesetLister := lister.(v1alpha1.CloneSetLister)
	cloneset, err := clonesetLister.List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Deployment {
		nodes = make([]*helper.MetaNode, 0, len(cloneset))
	}
	for _, d := range cloneset {
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
				WithAttribute(KeyUnavailableReplicas, d.Status.Replicas-d.Status.AvailableReplicas).
				WithAttribute(KeyStrategy, d.Spec.UpdateStrategy.Type)
			addCommonAttributes(&d.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}

func (in *InputKubernetesMeta) collectKruiseDaemonSet(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	logger.Debug(in.context.GetRuntimeContext(), "collect kruise daemonset")
	daemonSetLister := lister.(v1alpha1.DaemonSetLister)
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

func (in *InputKubernetesMeta) collectKruiseStatefulSet(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	logger.Debug(in.context.GetRuntimeContext(), "collect kruise statefulset")
	statefulSets, err := lister.(v1beta1.StatefulSetLister).List(selector)
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

func (in *InputKubernetesMeta) collectKruiseCronJobs(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	logger.Debug(in.context.GetRuntimeContext(), "collect kruise cronjob")
	cronJobs, err := lister.(v1alpha1.AdvancedCronJobLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.CronJob {
		nodes = make([]*helper.MetaNode, 0, len(cronJobs))
	}
	for _, j := range cronJobs {
		id := string(j.UID)
		if !in.DisableReportParents && len(j.Status.Active) > 0 {
			in.addCronJobMapping(j.Namespace, id, j.Name, j.Status.Active)
		}
		if in.CronJob {
			node := helper.NewMetaNode(id, CronJob).WithLabels(j.Labels).WithAttributes(make(helper.Attributes, 8)).
				WithAttribute(KeyNamespace, j.Namespace).
				WithAttribute(KeySchedule, j.Spec.Schedule).
				WithAttribute(KeySuspend, j.Spec.Paused != nil && *j.Spec.Paused).
				WithAttribute(KeyActiveJobs, len(j.Status.Active))
			if j.Status.LastScheduleTime != nil {
				node.WithAttribute(KeyLastScheduleTime, j.Status.LastScheduleTime.Unix())
			}
			addCommonAttributes(&j.ObjectMeta, node)
			nodes = append(nodes, node)
		}
	}
	return
}
