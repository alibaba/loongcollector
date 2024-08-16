package kubernetesmetav2

import (
	"context"
	// #nosec G501
	"crypto/md5"
	"fmt"
	"strings"
	"time"

	v1 "k8s.io/api/core/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type metaCollector struct {
	serviceK8sMeta *ServiceK8sMeta
	processors     map[string][]ProcessFunc
	collector      pipeline.Collector

	entityTypes      []string
	entityBuffer     chan models.PipelineEvent
	entityLinkBuffer chan models.PipelineEvent

	stopCh chan struct{}
}

func (m *metaCollector) Start() error {
	if m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.configName, k8smeta.POD, m.handleEvent, m.serviceK8sMeta.Interval)
		m.processors[k8smeta.POD] = append(m.processors[k8smeta.POD], m.processPodEntity)
		m.processors[k8smeta.POD] = append(m.processors[k8smeta.POD], m.processPodReplicasetLink)
		m.entityTypes = append(m.entityTypes, k8smeta.POD)
	}
	if m.serviceK8sMeta.Service {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.configName, k8smeta.SERVICE, m.handleEvent, m.serviceK8sMeta.Interval)
		m.processors[k8smeta.SERVICE] = append(m.processors[k8smeta.SERVICE], m.processServiceEntity)
		m.entityTypes = append(m.entityTypes, k8smeta.SERVICE)
	}
	if m.serviceK8sMeta.Pod && m.serviceK8sMeta.Service && m.serviceK8sMeta.PodServiceLink {
		// link data will be collected in pod registry
		m.processors[k8smeta.POD_SERVICE] = append(m.processors[k8smeta.POD_SERVICE], m.processPodServiceLink)
		m.entityTypes = append(m.entityTypes, k8smeta.POD_SERVICE)
	}
	go m.sendInBackground()
	return nil
}

func (m *metaCollector) Stop() error {
	if m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.configName, k8smeta.POD)
	}
	if m.serviceK8sMeta.Service {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.configName, k8smeta.SERVICE)
	}
	if m.serviceK8sMeta.Pod && m.serviceK8sMeta.Service && m.serviceK8sMeta.PodServiceLink {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.configName, k8smeta.POD_SERVICE)
	}
	close(m.stopCh)
	return nil
}

func (m *metaCollector) handleEvent(event *k8smeta.K8sMetaEvent) {
	switch event.EventType {
	case k8smeta.EventTypeAdd:
		m.handleAdd(event)
	case k8smeta.EventTypeUpdate:
		m.handleUpdate(event)
	case k8smeta.EventTypeDelete:
		m.handleDelete(event)
	default:
		logger.Error(context.Background(), "UNKNOWN_EVENT_TYPE", "unknown event type", event.EventType)
	}
}

func (m *metaCollector) handleAdd(event *k8smeta.K8sMetaEvent) {
	if processors, ok := m.processors[event.Object.ResourceType]; ok {
		for _, processor := range processors {
			log := processor(event.Object, "create")
			if log != nil {
				m.send(log, isLink(event.Object.ResourceType))
			}
		}
	}
}

func (m *metaCollector) handleUpdate(event *k8smeta.K8sMetaEvent) {
	if processors, ok := m.processors[event.Object.ResourceType]; ok {
		for _, processor := range processors {
			log := processor(event.Object, "update")
			if log != nil {
				m.send(log, isLink(event.Object.ResourceType))
			}
		}
	}
}

func (m *metaCollector) handleDelete(event *k8smeta.K8sMetaEvent) {
	if processors, ok := m.processors[event.Object.ResourceType]; ok {
		for _, processor := range processors {
			log := processor(event.Object, "delete")
			if log != nil {
				m.send(log, isLink(event.Object.ResourceType))
			}
		}
	}
}

func (m *metaCollector) send(event models.PipelineEvent, entity bool) {
	var buffer chan models.PipelineEvent
	if entity {
		buffer = m.entityBuffer
	} else {
		buffer = m.entityLinkBuffer
	}
	select {
	case buffer <- event:
	case <-time.After(3 * time.Second):
		logger.Warning(context.Background(), "SEND_EVENT_TIMEOUT", "send event timeout", event)
	}
}

func (m *metaCollector) sendInBackground() {
	entityGroup := &models.PipelineGroupEvents{}
	entityLinkGroup := &models.PipelineGroupEvents{}
	sendFunc := func(group *models.PipelineGroupEvents) {
		for _, e := range group.Events {
			// TODO: temporary convert from event group back to log, will fix after pipeline support Go input to C++ processor
			log := convertPipelineEvent2Log(e)
			m.collector.AddRawLog(log)
		}
		group.Events = group.Events[:0]
	}
	for {
		select {
		case e := <-m.entityBuffer:
			entityGroup.Events = append(entityGroup.Events, e)
			if len(entityGroup.Events) >= 100 {
				sendFunc(entityGroup)
			}
		case e := <-m.entityLinkBuffer:
			entityLinkGroup.Events = append(entityLinkGroup.Events, e)
			if len(entityLinkGroup.Events) >= 100 {
				sendFunc(entityLinkGroup)
			}
		case <-time.After(3 * time.Second):
			if len(entityGroup.Events) > 0 {
				sendFunc(entityGroup)
			}
			if len(entityLinkGroup.Events) > 0 {
				sendFunc(entityLinkGroup)
			}
		case <-m.stopCh:
			return
		}
	}
}

func convertPipelineEvent2Log(event models.PipelineEvent) *protocol.Log {
	if modelLog, ok := event.(*models.Log); ok {
		log := &protocol.Log{}
		log.Contents = make([]*protocol.Log_Content, 0)
		for k, v := range modelLog.Contents.Iterator() {
			if _, ok := v.(string); !ok {
				logger.Error(context.Background(), "COVERT_EVENT_TO_LOG_FAIL", "convert event to log fail, value is not string", v)
				continue
			}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: v.(string)})
		}
		protocol.SetLogTime(log, uint32(modelLog.GetTimestamp()))
		return log
	}
	return nil
}

func genKeyByPod(pod *v1.Pod) string {
	return genKey(pod.Namespace, pod.Kind, pod.Name)
}

func genKey(namespace, kind, name string) string {
	key := namespace + kind + name
	// #nosec G401
	return fmt.Sprintf("%x", md5.Sum([]byte(key)))
}

func genKeyByService(service *v1.Service) string {
	return genKey(service.Namespace, service.Kind, service.Name)
}

func isLink(resourceType string) bool {
	return strings.Contains(resourceType, k8smeta.LINK_SPLIT_CHARACTER)
}
