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
	batch "k8s.io/client-go/listers/batch/v1"
	batchbeta "k8s.io/client-go/listers/batch/v1beta1"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

// collectJobs collect the core metadata from the kubernetes jobs.
func (in *InputKubernetesMeta) collectJobs(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	jobLister := lister.(batch.JobLister)
	jobs, err := jobLister.List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Job {
		nodes = make([]*helper.MetaNode, 0, len(jobs))
	}
	for _, j := range jobs {
		id := string(j.UID)
		if !in.DisableReportParents {
			s, _ := metav1.LabelSelectorAsSelector(j.Spec.Selector)
			in.addMatcher(j.Namespace, Job, newLabelMatcher(j.Name, id, s))
		}
		if in.Job {
			node := helper.NewMetaNode(id, Job).WithLabels(j.Labels).WithAttributes(make(helper.Attributes, 8)).
				WithAttribute(KeyNamespace, j.Namespace).
				WithAttribute(KeyActive, j.Status.Active).
				WithAttribute(KeySucceeded, j.Status.Succeeded).
				WithAttribute(KeyFailed, j.Status.Failed)
			addCommonAttributes(&j.ObjectMeta, node)
			if j.Status.StartTime != nil {
				node.WithAttribute(KeyStartTime, j.Status.StartTime.Time.Unix())
			}
			if j.Status.CompletionTime != nil {
				node.WithAttribute(KeyCompletionTime, j.Status.CompletionTime.Time.Unix())
			}
			nodes = append(nodes, node)
		}
	}
	return
}

// collectCronJobs collect the core metadata from the kubernetes cron jobs.
func (in *InputKubernetesMeta) collectCronJobs(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	cronJobs, err := lister.(batchbeta.CronJobLister).List(selector)
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
				WithAttribute(KeySuspend, j.Spec.Suspend != nil && *j.Spec.Suspend).
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
