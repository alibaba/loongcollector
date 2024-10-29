package kubernetesmetav2

import (
	"context"
	"encoding/json"

	// #nosec G501
	"crypto/md5"
	"fmt"
	"strconv"
	"strings"
	"time"

	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type metaCollector struct {
	serviceK8sMeta *ServiceK8sMeta
	collector      pipeline.Collector

	entityBuffer     chan models.PipelineEvent
	entityLinkBuffer chan models.PipelineEvent

	stopCh          chan struct{}
	entityProcessor map[string]ProcessFunc
}

func (m *metaCollector) Start() error {
	m.entityProcessor = map[string]ProcessFunc{
		k8smeta.POD:                      m.processPodEntity,
		k8smeta.NODE:                     m.processNodeEntity,
		k8smeta.SERVICE:                  m.processServiceEntity,
		k8smeta.DEPLOYMENT:               m.processDeploymentEntity,
		k8smeta.REPLICASET:               m.processReplicaSetEntity,
		k8smeta.DAEMONSET:                m.processDaemonSetEntity,
		k8smeta.STATEFULSET:              m.processStatefulSetEntity,
		k8smeta.CONFIGMAP:                m.processConfigMapEntity,
		k8smeta.JOB:                      m.processJobEntity,
		k8smeta.CRONJOB:                  m.processCronJobEntity,
		k8smeta.NAMESPACE:                m.processNamespaceEntity,
		k8smeta.PERSISTENTVOLUME:         m.processPersistentVolumeEntity,
		k8smeta.PERSISTENTVOLUMECLAIM:    m.processPersistentVolumeClaimEntity,
		k8smeta.STORAGECLASS:             m.processStorageClassEntity,
		k8smeta.INGRESS:                  m.processIngressEntity,
		k8smeta.POD_NODE:                 m.processPodNodeLink,
		k8smeta.REPLICASET_DEPLOYMENT:    m.processReplicaSetDeploymentLink,
		k8smeta.POD_REPLICASET:           m.processPodReplicaSetLink,
		k8smeta.POD_STATEFULSET:          m.processPodStatefulSetLink,
		k8smeta.POD_DAEMONSET:            m.processPodDaemonSetLink,
		k8smeta.JOB_CRONJOB:              m.processJobCronJobLink,
		k8smeta.POD_JOB:                  m.processPodJobLink,
		k8smeta.POD_PERSISENTVOLUMECLAIN: m.processPodPVCLink,
		k8smeta.POD_CONFIGMAP:            m.processPodConfigMapLink,
		k8smeta.POD_SERVICE:              m.processPodServiceLink,
		k8smeta.POD_CONTAINER:            m.processPodContainerLink,
	}

	if m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Node {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.NODE, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Service {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.SERVICE, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Deployment {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.DEPLOYMENT, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.ReplicaSet {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.REPLICASET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.DaemonSet {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.DAEMONSET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.StatefulSet {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.STATEFULSET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Configmap {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.CONFIGMAP, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Job {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.JOB, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.CronJob {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.CRONJOB, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Namespace {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.NAMESPACE, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.PersistentVolume {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.PERSISTENTVOLUME, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.PersistentVolumeClaim {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.PERSISTENTVOLUMECLAIM, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.StorageClass {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.STORAGECLASS, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Ingress {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.INGRESS, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Pod && m.serviceK8sMeta.Node {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_NODE, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Deployment && m.serviceK8sMeta.ReplicaSet {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.REPLICASET_DEPLOYMENT, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.ReplicaSet && m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_REPLICASET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.StatefulSet && m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_STATEFULSET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.DaemonSet && m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_DAEMONSET, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.CronJob && m.serviceK8sMeta.Job {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.JOB_CRONJOB, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Job && m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_JOB, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Pod && m.serviceK8sMeta.PersistentVolumeClaim {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_PERSISENTVOLUMECLAIN, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Pod && m.serviceK8sMeta.Configmap {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_CONFIGMAP, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Service && m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_SERVICE, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	if m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.RegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD_CONTAINER, m.handleEvent, m.serviceK8sMeta.Interval)
	}
	go m.sendInBackground()
	return nil
}

func (m *metaCollector) Stop() error {
	if m.serviceK8sMeta.Pod {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.POD)
	}
	if m.serviceK8sMeta.Node {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.NODE)
	}
	if m.serviceK8sMeta.Service {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.SERVICE)
	}
	if m.serviceK8sMeta.Deployment {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.DEPLOYMENT)
	}
	if m.serviceK8sMeta.ReplicaSet {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.REPLICASET)
	}
	if m.serviceK8sMeta.DaemonSet {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.DAEMONSET)
	}
	if m.serviceK8sMeta.StatefulSet {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.STATEFULSET)
	}
	if m.serviceK8sMeta.Configmap {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.CONFIGMAP)
	}
	if m.serviceK8sMeta.Job {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.JOB)
	}
	if m.serviceK8sMeta.CronJob {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.CRONJOB)
	}
	if m.serviceK8sMeta.Namespace {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.NAMESPACE)
	}
	if m.serviceK8sMeta.PersistentVolume {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.PERSISTENTVOLUME)
	}
	if m.serviceK8sMeta.PersistentVolumeClaim {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.PERSISTENTVOLUMECLAIM)
	}
	if m.serviceK8sMeta.StorageClass {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.STORAGECLASS)
	}
	if m.serviceK8sMeta.Ingress {
		m.serviceK8sMeta.metaManager.UnRegisterSendFunc(m.serviceK8sMeta.context.GetProject(), m.serviceK8sMeta.configName, k8smeta.INGRESS)
	}
	close(m.stopCh)
	return nil
}

func (m *metaCollector) handleEvent(event []*k8smeta.K8sMetaEvent) {
	if len(event) == 0 {
		return
	}
	switch event[0].EventType {
	case k8smeta.EventTypeAdd, k8smeta.EventTypeUpdate:
		for _, e := range event {
			m.handleAddOrUpdate(e)
		}
	case k8smeta.EventTypeDelete:
		for _, e := range event {
			m.handleDelete(e)
		}
	default:
		logger.Error(context.Background(), "UNKNOWN_EVENT_TYPE", "unknown event type", event[0].EventType)
	}
}

func (m *metaCollector) handleAddOrUpdate(event *k8smeta.K8sMetaEvent) {
	if processor, ok := m.entityProcessor[event.Object.ResourceType]; ok {
		logs := processor(event.Object, "Update")
		for _, log := range logs {
			m.send(log, isEntity(event.Object.ResourceType))
			if isEntity(event.Object.ResourceType) {
				link := m.generateEntityClusterLink(log)
				m.send(link, true)
			}
		}
	}
}

func (m *metaCollector) handleDelete(event *k8smeta.K8sMetaEvent) {
	if processor, ok := m.entityProcessor[event.Object.ResourceType]; ok {
		logs := processor(event.Object, "Expire")
		for _, log := range logs {
			m.send(log, isEntity(event.Object.ResourceType))
			if isEntity(event.Object.ResourceType) {
				link := m.generateEntityClusterLink(log)
				m.send(link, true)
			}
		}
	}
}

func (m *metaCollector) processEntityCommonPart(logContents models.LogContents, kind, namespace, name, method string, firstObservedTime, lastObservedTime int64, creationTime v1.Time) {
	// entity reserved fields
	logContents.Add(entityDomainFieldName, m.serviceK8sMeta.domain)
	logContents.Add(entityTypeFieldName, m.genEntityTypeKey(kind))
	logContents.Add(entityIDFieldName, m.genKey(kind, namespace, name))
	logContents.Add(entityMethodFieldName, method)

	logContents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(firstObservedTime, 10))
	logContents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(lastObservedTime, 10))
	logContents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval*2), 10))
	logContents.Add(entityCategoryFieldName, defaultEntityCategory)

	// common custom fields
	logContents.Add(entityClusterIDFieldName, m.serviceK8sMeta.clusterID)
	logContents.Add(entityKindFieldName, kind)
	logContents.Add(entityNameFieldName, name)
	logContents.Add(entityCreationTimeFieldName, creationTime.Format(time.RFC3339))
}

func (m *metaCollector) processEntityLinkCommonPart(logContents models.LogContents, srcKind, srcNamespace, srcName, destKind, destNamespace, destName, method string, firstObservedTime, lastObservedTime int64) {
	logContents.Add(entityLinkSrcDomainFieldName, m.serviceK8sMeta.domain)
	logContents.Add(entityLinkSrcEntityTypeFieldName, m.genEntityTypeKey(srcKind))
	logContents.Add(entityLinkSrcEntityIDFieldName, m.genKey(srcKind, srcNamespace, srcName))

	logContents.Add(entityLinkDestDomainFieldName, m.serviceK8sMeta.domain)
	logContents.Add(entityLinkDestEntityTypeFieldName, m.genEntityTypeKey(destKind))
	logContents.Add(entityLinkDestEntityIDFieldName, m.genKey(destKind, destNamespace, destName))

	logContents.Add(entityMethodFieldName, method)

	logContents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(firstObservedTime, 10))
	logContents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(lastObservedTime, 10))
	logContents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval*2), 10))
	logContents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
}

func (m *metaCollector) processEntityJSONObject(obj map[string]string) string {
	if obj == nil {
		return "{}"
	}
	objStr, err := json.Marshal(obj)
	if err != nil {
		logger.Error(context.Background(), "PROCESS_ENTITY_JSON_OBJECT_FAIL", "process entity json object fail", err)
		return "{}"
	}
	return string(objStr)
}

func (m *metaCollector) processEntityJSONArray(obj []map[string]string) string {
	if obj == nil {
		return "[]"
	}
	objStr, err := json.Marshal(obj)
	if err != nil {
		logger.Error(context.Background(), "PROCESS_ENTITY_JSON_ARRAY_FAIL", "process entity json array fail", err)
		return "[]"
	}
	return string(objStr)
}

func (m *metaCollector) send(event models.PipelineEvent, entity bool) {
	var buffer chan models.PipelineEvent
	if entity {
		buffer = m.entityBuffer
	} else {
		buffer = m.entityLinkBuffer
	}
	buffer <- event
}

func (m *metaCollector) sendInBackground() {
	entityGroup := &models.PipelineGroupEvents{}
	linkGroup := &models.PipelineGroupEvents{}
	sendFunc := func(group *models.PipelineGroupEvents) {
		for _, e := range group.Events {
			// TODO: temporary convert from event group back to log, will fix after pipeline support Go input to C++ processor
			log := convertPipelineEvent2Log(e)
			m.collector.AddRawLog(log)
		}
		group.Events = group.Events[:0]
	}
	lastSendClusterTime := time.Now()
	for {
		select {
		case e := <-m.entityBuffer:
			entityGroup.Events = append(entityGroup.Events, e)
			if len(entityGroup.Events) >= 100 {
				m.serviceK8sMeta.entityCount.Add(int64(len(entityGroup.Events)))
				sendFunc(entityGroup)
			}
		case e := <-m.entityLinkBuffer:
			linkGroup.Events = append(linkGroup.Events, e)
			if len(linkGroup.Events) >= 100 {
				m.serviceK8sMeta.linkCount.Add(int64(len(linkGroup.Events)))
				sendFunc(linkGroup)
			}
		case <-time.After(3 * time.Second):
			if len(entityGroup.Events) > 0 {
				m.serviceK8sMeta.entityCount.Add(int64(len(entityGroup.Events)))
				sendFunc(entityGroup)
			}
			if len(linkGroup.Events) > 0 {
				m.serviceK8sMeta.linkCount.Add(int64(len(linkGroup.Events)))
				sendFunc(linkGroup)
			}
		case <-m.stopCh:
			return
		}
		if time.Since(lastSendClusterTime) > time.Duration(m.serviceK8sMeta.Interval)*time.Second {
			// send cluster entity if in infra domain
			if m.serviceK8sMeta.domain == infraDomain {
				clusterEntity := m.generateClusterEntity()
				m.collector.AddRawLog(convertPipelineEvent2Log(clusterEntity))
				lastSendClusterTime = time.Now()
			}
		}
	}
}

func (m *metaCollector) genKey(kind, namespace, name string) string {
	key := m.serviceK8sMeta.clusterID + kind + namespace + name
	// #nosec G401
	return fmt.Sprintf("%x", md5.Sum([]byte(key)))
}

func (m *metaCollector) generateClusterEntity() models.PipelineEvent {
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	log.Timestamp = uint64(time.Now().Unix())
	log.Contents.Add(entityDomainFieldName, m.serviceK8sMeta.domain)
	log.Contents.Add(entityTypeFieldName, m.genEntityTypeKey(clusterTypeName))
	log.Contents.Add(entityIDFieldName, m.genKey("", "", ""))
	log.Contents.Add(entityMethodFieldName, "Update")
	log.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(time.Now().Unix(), 10))
	log.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(time.Now().Unix(), 10))
	log.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval*2), 10))
	log.Contents.Add(entityCategoryFieldName, defaultEntityCategory)
	log.Contents.Add(entityClusterIDFieldName, m.serviceK8sMeta.clusterID)
	return log
}

func (m *metaCollector) generateEntityClusterLink(entityEvent models.PipelineEvent) models.PipelineEvent {
	content := entityEvent.(*models.Log).Contents
	log := &models.Log{}
	log.Contents = models.NewLogContents()
	log.Contents.Add(entityLinkSrcDomainFieldName, m.serviceK8sMeta.domain)
	log.Contents.Add(entityLinkSrcEntityTypeFieldName, content.Get(entityTypeFieldName))
	log.Contents.Add(entityLinkSrcEntityIDFieldName, content.Get(entityIDFieldName))
	log.Contents.Add(entityLinkDestDomainFieldName, m.serviceK8sMeta.domain)
	log.Contents.Add(entityLinkDestEntityTypeFieldName, m.genEntityTypeKey(clusterTypeName))
	log.Contents.Add(entityLinkDestEntityIDFieldName, m.genKey("", "", ""))

	log.Contents.Add(entityLinkRelationTypeFieldName, "runs")
	log.Contents.Add(entityMethodFieldName, content.Get(entityMethodFieldName))

	log.Contents.Add(entityFirstObservedTimeFieldName, content.Get(entityFirstObservedTimeFieldName))
	log.Contents.Add(entityLastObservedTimeFieldName, content.Get(entityLastObservedTimeFieldName))
	log.Contents.Add(entityKeepAliveSecondsFieldName, m.serviceK8sMeta.Interval*2)
	log.Contents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
	log.Timestamp = uint64(time.Now().Unix())
	return log
}

func (m *metaCollector) genEntityTypeKey(kind string) string {
	// assert domain is initialized
	if kind == "" {
		return m.serviceK8sMeta.domain + ".k8s"
	}
	if kind == clusterTypeName && m.serviceK8sMeta.domain == acsDomain {
		return m.serviceK8sMeta.domain + "." + *flags.ClusterType + "." + clusterTypeName
	}
	return m.serviceK8sMeta.domain + ".k8s." + strings.ToLower(kind)
}

func convertPipelineEvent2Log(event models.PipelineEvent) *protocol.Log {
	if modelLog, ok := event.(*models.Log); ok {
		log := &protocol.Log{}
		log.Contents = make([]*protocol.Log_Content, 0)
		for k, v := range modelLog.Contents.Iterator() {
			if _, ok := v.(string); !ok {
				if intValue, ok := v.(int); !ok {
					logger.Error(context.Background(), "COVERT_EVENT_TO_LOG_FAIL", "convert event to log fail, value is not string", v, "key", k)
					continue
				} else {
					v = strconv.Itoa(intValue)
				}
			}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: v.(string)})
		}
		protocol.SetLogTime(log, uint32(modelLog.GetTimestamp()))
		return log
	}
	return nil
}

func isEntity(resourceType string) bool {
	return !strings.Contains(resourceType, k8smeta.LINK_SPLIT_CHARACTER)
}
