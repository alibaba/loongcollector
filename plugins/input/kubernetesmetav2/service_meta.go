package kubernetesmetav2

import (
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type ProcessFunc func(data *k8smeta.ObjectWrapper, method string) []models.PipelineEvent

//revive:disable:exported
type ServiceK8sMeta struct {
	//revive:enable:exported
	Interval int
	// entity switch
	Pod                   bool
	Node                  bool
	Service               bool
	Deployment            bool
	ReplicaSet            bool
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
	Container             bool
	// link switch
	Node2Pod                  string
	Deployment2Pod            string
	ReplicaSet2Pod            string
	Deployment2ReplicaSet     string
	StatefulSet2Pod           string
	DaemonSet2Pod             string
	Service2Pod               string
	Pod2Container             string
	CronJob2Job               string
	Job2Pod                   string
	Ingress2Service           string
	Pod2PersistentVolumeClaim string
	Pod2ConfigMap             string

	// add link for namesapce
	Namespace2Pod                   string
	Namespace2Service               string
	Namespace2Deployment            string
	Namespace2DaemonSet             string
	Namespace2StatefulSet           string
	Namespace2Configmap             string
	Namespace2Job                   string
	Namespace2CronJob               string
	Namespace2PersistentVolume      string
	Namespace2PersistentVolumeClaim string
	Namespace2StorageClass          string
	Namespace2Ingress               string

	// restrict cluster link dest target
	Cluster2Node             string
	Cluster2Namespace        string
	Cluster2PersistentVolume string
	Cluster2StorageClass     string

	// other
	context       pipeline.Context
	metaManager   *k8smeta.MetaManager
	collector     pipeline.Collector
	metaCollector *metaCollector
	configName    string
	clusterID     string
	clusterName   string
	clusterRegion string
	domain        string

	// self metric
	entityCount selfmonitor.CounterMetric
	linkCount   selfmonitor.CounterMetric
}

// Init called for init some system resources, like socket, mutex...
// return interval(ms) and error flag, if interval is 0, use default interval
func (s *ServiceK8sMeta) Init(context pipeline.Context) (int, error) {
	s.context = context
	s.metaManager = k8smeta.GetMetaManagerInstance()
	s.configName = context.GetConfigName()
	s.initDomain()

	metricRecord := s.context.GetMetricRecord()
	s.entityCount = selfmonitor.NewCounterMetricAndRegister(metricRecord, selfmonitor.MetricCollectEntityTotal)
	s.linkCount = selfmonitor.NewCounterMetricAndRegister(metricRecord, selfmonitor.MetricCollectLinkTotal)
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
		collector:        collector,
		entityBuffer:     make(chan models.PipelineEvent, 100),
		entityLinkBuffer: make(chan models.PipelineEvent, 100),
		stopCh:           make(chan struct{}),
		entityProcessor:  make(map[string]ProcessFunc),
	}
	return s.metaCollector.Start()
}

func (s *ServiceK8sMeta) initDomain() {

	if flags.ClusterType != nil && *flags.ClusterType != "" {
		s.domain = *flags.ClusterType
	} else {
		s.domain = k8sDomain
	}

}

func init() {
	pipeline.ServiceInputs["service_kubernetes_meta"] = func() pipeline.ServiceInput {
		return &ServiceK8sMeta{
			Interval:      60,
			clusterID:     *flags.ClusterID,
			clusterName:   *flags.ClusterName,
			clusterRegion: *flags.ClusterRegion,
		}
	}
}
