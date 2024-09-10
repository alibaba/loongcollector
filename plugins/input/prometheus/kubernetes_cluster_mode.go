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

package prometheus

import (
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"context"
	"errors"
	"os"
	"strings"
	"time"

	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	appsv1 "k8s.io/client-go/kubernetes/typed/apps/v1"
	"k8s.io/client-go/rest"
)

// KubernetesMeta means workload meta
type KubernetesMeta struct {
	context               pipeline.Context
	kubernetesClusterMode bool
	replicas              int32
	currentNum            int
	namespace             string
	workloadName          string
	readReplicasErrCount  int
	appsClient            *appsv1.AppsV1Client
}

func NewKubernetesMeta(ctx pipeline.Context) *KubernetesMeta {
	return &KubernetesMeta{
		context: ctx,
	}
}

func (p *KubernetesMeta) isWorkingOnClusterMode() bool {
	return p.kubernetesClusterMode
}

func (p *KubernetesMeta) readKubernetesWorkloadMeta() bool {
	// history use ILOGTAIL_PROMETHEUS_CLUSTER_REPLICAS env to set statis replicas, not only use it to distinct kubernetes cluster mode.
	if !*flags.StatefulSetFlag {
		return false
	}
	readNamespaceFunc := func() (string, error) {
		if ns := os.Getenv("POD_NAMESPACE"); ns != "" {
			return ns, nil
		}
		if data, err := os.ReadFile("/var/run/secrets/kubernetes.io/serviceaccount/namespace"); err == nil {
			if ns := strings.TrimSpace(string(data)); len(ns) > 0 {
				return ns, nil
			}
		}
		return "", errors.New("fail to found namespace")
	}

	var err error
	p.namespace, err = readNamespaceFunc()
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "read namespace err", err)
		return false
	}
	config, err := rest.InClusterConfig()
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "create in cluster config err", err)
		return false
	}
	p.appsClient, err = appsv1.NewForConfig(config)
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "create in cluster config err", err)
		return false
	}
	podName := os.Getenv("POD_NAME")
	if p.currentNum = helper.ExtractStatefulSetNum(podName); p.currentNum == -1 {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "read current num err", p.workloadName)
		return false
	}
	if p.workloadName = helper.ExtractPodWorkload(podName); p.workloadName == "" {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "read workload name err", p.workloadName)
		return false
	}
	for i := 0; i < 3; i++ {
		if _, err = p.getPrometheusReplicas(); err == nil || i == 2 {
			break
		}
		time.Sleep(time.Millisecond * 500)
	}
	if err != nil {
		logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "read replicas err", err)
		p.replicas = 1 // means maybe repeat scrape
	}
	p.kubernetesClusterMode = true
	logger.Info(p.context.GetRuntimeContext(), "replicas", p.replicas, "num", p.currentNum, "workload", p.workloadName, "namespace", p.namespace)
	return p.kubernetesClusterMode
}

func (p *KubernetesMeta) getPrometheusReplicas() (change bool, err error) {
	res, err := p.appsClient.StatefulSets(p.namespace).Get(context.Background(), p.workloadName, metav1.GetOptions{})
	if err != nil {
		p.readReplicasErrCount++
		if p.readReplicasErrCount >= 30 {
			logger.Error(p.context.GetRuntimeContext(), "KUBE_PROMETHEUS_ALARM", "cannot get kubernetes ilogtail cluster mode replicas", err)
			p.readReplicasErrCount = 0
		}
		return false, err
	}
	if *res.Spec.Replicas != p.replicas {
		p.replicas = *res.Spec.Replicas
		return true, nil
	}
	return false, nil
}
