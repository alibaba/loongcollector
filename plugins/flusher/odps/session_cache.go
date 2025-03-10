package odps

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tunnel"
)

const DefaultPartition = "default"
const DefaultExpireTime = 5 * time.Minute

type SessionItem struct {
	partition  string
	session    *tunnel.StreamUploadSession
	writer     *tunnel.RecordPackStreamWriter
	accessTime time.Time
}

type SessionCache struct {
	context     pipeline.Context
	projectName string
	schemaName  string
	tableName   string
	tunnelIns   *tunnel.Tunnel
	sessionMap  map[string]*SessionItem
	expireTime  time.Duration
}

func NewSessionCache(context pipeline.Context, project, schema, table string, tunnel *tunnel.Tunnel) *SessionCache {
	return &SessionCache{
		context:     context,
		projectName: project,
		schemaName:  schema,
		tableName:   table,
		tunnelIns:   tunnel,
		expireTime:  DefaultExpireTime,
		sessionMap:  make(map[string]*SessionItem),
	}
}

func (sc *SessionCache) createSession(partition string) (*tunnel.StreamUploadSession, error) {
	opts := make([]tunnel.Option, 0)
	opts = append(opts, tunnel.SessionCfg.WithDefaultDeflateCompressor())

	if partition != DefaultPartition {
		opts = append(opts, tunnel.SessionCfg.WithPartitionKey(partition))
		opts = append(opts, tunnel.SessionCfg.WithCreatePartition())
	}

	if len(sc.schemaName) > 0 {
		opts = append(opts, tunnel.SessionCfg.WithSchemaName(sc.schemaName))
	}

	return sc.tunnelIns.CreateStreamUploadSession(
		sc.projectName,
		sc.tableName,
		opts...,
	)
}

func (sc *SessionCache) createSessionItem(partition string) (*SessionItem, error) {
	newSession, err := sc.createSession(partition)
	if err != nil {
		return nil, err
	}

	return &SessionItem{
		partition:  partition,
		session:    newSession,
		writer:     newSession.OpenRecordPackWriter(),
		accessTime: time.Now(),
	}, nil
}

func (sc *SessionCache) cleanup() {
	now := time.Now()
	for key, val := range sc.sessionMap {
		if now.Sub(val.accessTime) > sc.expireTime {
			delete(sc.sessionMap, key)
			logger.Infof(sc.context.GetRuntimeContext(), "Delete odps(%s/%s/%s) expire session, partition: %s, last access time: %s",
				sc.projectName, sc.schemaName, sc.tableName, val.partition, val.accessTime)
		}
	}
}

func (sc *SessionCache) GetWriter(partition string) (*tunnel.StreamUploadSession, *tunnel.RecordPackStreamWriter, error) {
	item, exists := sc.sessionMap[partition]
	if exists {
		item.accessTime = time.Now()
	} else {
		newSession, err := sc.createSessionItem(partition)
		if err != nil {
			logger.Errorf(sc.context.GetRuntimeContext(), OdpsAlarm, "Create odps(%s/%s/%s) session failed, partition: %s",
				sc.projectName, sc.schemaName, sc.tableName, partition)
			return nil, nil, err
		}
		logger.Infof(sc.context.GetRuntimeContext(), "Create odps(%s/%s/%s) session success, partition: %s",
			sc.projectName, sc.schemaName, sc.tableName, partition)
		item = newSession
		sc.sessionMap[partition] = newSession
		sc.cleanup()
	}

	return item.session, item.writer, nil
}

func (sc *SessionCache) GetAllWriter() ([]string, []*tunnel.RecordPackStreamWriter) {
	partitions := make([]string, 0, len(sc.sessionMap))
	writers := make([]*tunnel.RecordPackStreamWriter, 0, len(sc.sessionMap))
	for _, val := range sc.sessionMap {
		partitions = append(partitions, val.partition)
		writers = append(writers, val.writer)
	}
	return partitions, writers
}
