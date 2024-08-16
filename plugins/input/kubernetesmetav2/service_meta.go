package kubernetesmetav2

import (
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type ProcessFunc func(data *k8smeta.ObjectWrapper, method string) models.PipelineEvent

//revive:disable:exported
type ServiceK8sMeta struct {
	//revive:enable:exported
	Interval int
	// entity switch
	Pod                   bool
	Node                  bool
	Service               bool
	Deployment            bool
	DaemonSet             bool
	StatefulSet           bool
	Configmap             bool
	Secret                bool
	Job                   bool
	CronJob               bool
	Namespace             bool
	PersistentVolume      bool
	PersistentVolumeClaim bool
	StorageClass          bool
	Ingress               bool
	// entity link switch
	PodReplicasetLink bool
	PodServiceLink    bool
	// other
	metaManager   *k8smeta.MetaManager
	collector     pipeline.Collector
	metaCollector *metaCollector
	configName    string
}

// Init called for init some system resources, like socket, mutex...
// return interval(ms) and error flag, if interval is 0, use default interval
func (s *ServiceK8sMeta) Init(context pipeline.Context) (int, error) {
	s.metaManager = k8smeta.GetMetaManagerInstance()
	s.configName = context.GetConfigName()

	return 0, nil
}

// Description returns a one-sentence description on the Input
func (s *ServiceK8sMeta) Description() string {
	return ""
}

// Stop stops the services and closes any necessary channels and connections
func (s *ServiceK8sMeta) Stop() error {
	return s.metaCollector.Stop()
}

func (s *ServiceK8sMeta) Start(collector pipeline.Collector) error {
	s.collector = collector
	s.metaCollector = &metaCollector{
		serviceK8sMeta:   s,
		processors:       make(map[string][]ProcessFunc),
		collector:        collector,
		entityTypes:      []string{},
		entityBuffer:     make(chan models.PipelineEvent, 100),
		entityLinkBuffer: make(chan models.PipelineEvent, 100),
		stopCh:           make(chan struct{}),
	}
	return s.metaCollector.Start()
}

func init() {
	pipeline.ServiceInputs["service_kubernetes_meta"] = func() pipeline.ServiceInput {
		return &ServiceK8sMeta{
			Interval: 30,
		}
	}
}
