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

package kubernetesmetav1

import (
	"fmt"
	"os"
	"path/filepath"
	"time"

	kruise "github.com/openkruise/kruise-api/client/informers/externalversions"
	api "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/clientcmd"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

const pluginType = "metric_meta_kubernetes"
const (
	defaultIntervalMs = 300000
)

type InputKubernetesMeta struct {
	Pod                   bool
	Node                  bool
	Service               bool
	Deployment            bool
	DaemonSet             bool
	StatefulSet           bool
	Configmap             bool
	Job                   bool
	CronJob               bool
	Namespace             bool
	PersistentVolume      bool
	PersistentVolumeClaim bool
	StorageClass          bool
	Ingress               bool

	DisableReportParents   bool
	KubeConfigPath         string
	SelectedNamespaces     []string
	LabelSelectors         string
	IntervalMs             int
	EnableOpenKruise       bool
	Labels                 map[string]string
	context                pipeline.Context
	informerFactory        informers.SharedInformerFactory
	kuriseInformerFactory  kruise.SharedInformerFactory
	clientset              *kubernetes.Clientset
	selector               labels.Selector
	collectors             []*collector
	informerStopChan       chan struct{}
	nodeMapping            map[string]string                           // nodeID:nodeName
	matchers               map[string]labelMatchers                    // namespace:labelMatchers
	cronjobActives         map[string]map[string][]api.ObjectReference // namespace:cronjobID:[job references...]
	cronjobMapping         map[string]string                           // cronjobID:cronjobName
	ingressRelationMapping map[string]map[string]map[string]struct{}   // namespace:ingressID:[service names...]
	ingressMapping         map[string]string
}

func (in *InputKubernetesMeta) Init(context pipeline.Context) (int, error) {
	in.informerStopChan = make(chan struct{})
	in.context = context
	if in.IntervalMs < 5000 {
		logger.Warning(in.context.GetRuntimeContext(), "KUBERNETES_META_FETCH_INTERVAL_ALARM", "interval", "must over than 5000 ms")
		in.IntervalMs = defaultIntervalMs
	}
	// When kubeConfigPath is empty, cluster config would be read.
	if in.KubeConfigPath != "" {
		if _, err := os.Stat(in.KubeConfigPath); err != nil {
			path := filepath.Join(os.Getenv("HOME"), ".kube", "config")
			if _, err := os.Stat(path); err != nil {
				in.KubeConfigPath = ""
			} else {
				in.KubeConfigPath = path
			}
		}
	}
	c, err := clientcmd.BuildConfigFromFlags("", in.KubeConfigPath)
	if err != nil {
		return 0, fmt.Errorf("error in reading kube config: %v", err)
	}
	client, err := kubernetes.NewForConfig(c)
	if err != nil {
		return 0, fmt.Errorf("error in creating kubernetes client: %v", err)
	}
	in.clientset = client
	var options []informers.SharedInformerOption
	if len(in.SelectedNamespaces) == 0 {
		options = append(options, informers.WithNamespace(api.NamespaceAll))
	} else {
		for _, ns := range in.SelectedNamespaces {
			options = append(options, informers.WithNamespace(ns))
		}
	}
	in.informerFactory = informers.NewSharedInformerFactoryWithOptions(client, time.Minute*30, options...)
	in.addInformerListerCollectors()
	if in.EnableOpenKruise {
		in.InitKruise(c)
	}
	if in.LabelSelectors == "" {
		in.selector = labels.Everything()
	} else {
		selector, err := labels.Parse(in.LabelSelectors)
		if err != nil {
			logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_LABEL_SELECTOR_ERROR", "error", err)
			selector = labels.Everything()
		}
		in.selector = selector
	}
	in.informerFactory.Start(in.informerStopChan)
	in.nodeMapping = make(map[string]string, 16)
	in.matchers = make(map[string]labelMatchers, 16)
	in.cronjobActives = make(map[string]map[string][]api.ObjectReference, 16)
	in.cronjobMapping = make(map[string]string, 16)
	in.ingressRelationMapping = make(map[string]map[string]map[string]struct{}, 16)
	in.ingressMapping = make(map[string]string, 16)

	return 0, nil
}

func (in *InputKubernetesMeta) addInformerListerCollectors() {
	if in.Pod {
		in.collectors = append(in.collectors, newCollector(Pod, in.informerFactory.Core().V1().Pods().Lister(), in.collectPods))
	}
	if in.Namespace {
		in.collectors = append(in.collectors, newCollector(Namespace, in.informerFactory.Core().V1().Namespaces().Lister(), in.collectNamespaces))
	}
	if in.PersistentVolume {
		in.collectors = append(in.collectors, newCollector(PersistentVolume, in.informerFactory.Core().V1().
			PersistentVolumes().Lister(), in.collectPersistentVolume))
	}
	if in.PersistentVolumeClaim {
		in.collectors = append(in.collectors, newCollector(PersistentVolumeClaim, in.informerFactory.Core().V1().
			PersistentVolumeClaims().Lister(), in.collectPersistentVolumeClaims))
	}
	if in.StorageClass {
		in.collectors = append(in.collectors, newCollector(StorageClass, in.informerFactory.Storage().V1().
			StorageClasses().Lister(), in.collectStorageClass))
	}
	if in.Ingress || in.Service {
		in.collectors = append(in.collectors, newCollector(Ingress, in.informerFactory.Networking().V1beta1().Ingresses().Lister(), in.collectIngresses))
	}
	if in.Pod || in.Node {
		in.collectors = append(in.collectors, newCollector(Node, in.informerFactory.Core().V1().Nodes().Lister(), in.collectNodes))
	}
	if in.Pod || in.Service {
		in.collectors = append(in.collectors, newCollector(Service, in.informerFactory.Core().V1().Services().Lister(), in.collectServices))
	}
	if in.Pod || in.Deployment {
		in.collectors = append(in.collectors, newCollector(Deployment, in.informerFactory.Apps().V1().Deployments().Lister(), in.collectDeployment))
	}
	if in.Pod || in.DaemonSet {
		in.collectors = append(in.collectors, newCollector(DaemonSet, in.informerFactory.Apps().V1().DaemonSets().Lister(), in.collectDaemonSet))
	}
	if in.Pod || in.StatefulSet {
		in.collectors = append(in.collectors, newCollector(StatefulSet, in.informerFactory.Apps().V1().StatefulSets().Lister(), in.collectStatefulSet))
	}
	if in.Pod || in.Job {
		in.collectors = append(in.collectors, newCollector(Job, in.informerFactory.Batch().V1().Jobs().Lister(), in.collectJobs))
	}
	if in.Pod || in.CronJob || in.Job {
		in.collectors = append(in.collectors, newCollector(CronJob, in.informerFactory.Batch().V1beta1().CronJobs().Lister(), in.collectCronJobs))
	}
	if in.Configmap {
		in.collectors = append(in.collectors, newCollector(Configmap, in.informerFactory.Core().V1().ConfigMaps().Lister(), in.collectConfigmaps))
	}
}

func (in *InputKubernetesMeta) Description() string {
	return "collect the kubernetes metadata"
}

func (in *InputKubernetesMeta) Collect(collector pipeline.Collector) error {
	now := time.Now()
	transfer := func(nodes []*helper.MetaNode) {
		for _, node := range nodes {
			for k, v := range in.Labels {
				node.WithLabel(k, v)
			}
			helper.AddMetadata(collector, now, node)
		}
	}
	var pods, jobs, services []*helper.MetaNode
	for _, c := range in.collectors {
		nodes, err := c.collect(c.lister, in.selector)
		if err != nil {
			logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_COLLECT_ERROR", "error", err)
			continue
		}
		if len(nodes) == 0 {
			continue
		}

		switch c.name {
		case Pod:
			pods = nodes
		case Service:
			services = nodes
		case Job:
			jobs = nodes
		default:
			transfer(nodes)
		}
	}
	// wrapper the parents of pod nodes.
	if len(pods) > 0 {
		if !in.DisableReportParents {
			defer in.free()
			in.addPodParents(pods)
			in.addServiceParents(services)
			in.addJobParents(jobs)
			in.addServiceReference(services, pods)
		}
		transfer(pods)
		transfer(services)
		transfer(jobs)
	}
	return nil
}

func (in *InputKubernetesMeta) Stop() error {
	close(in.informerStopChan)
	return nil
}

func init() {
	pipeline.MetricInputs[pluginType] = func() pipeline.MetricInput {
		return &InputKubernetesMeta{
			Pod:                   true,
			Service:               true,
			Node:                  true,
			Deployment:            true,
			DaemonSet:             true,
			StatefulSet:           true,
			PersistentVolumeClaim: true,
			PersistentVolume:      true,
			Ingress:               true,
			StorageClass:          true,
			Job:                   true,
			CronJob:               true,
			Namespace:             true,
			Configmap:             true,
			IntervalMs:            defaultIntervalMs,
		}
	}
}
