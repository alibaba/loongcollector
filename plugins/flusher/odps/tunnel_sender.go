package odps

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tunnel"
)

type TunnelSender interface {
	Init() error
	Send(logGroup *protocol.LogGroup, log *protocol.Log) error
	Flush() error
}

func NewTunnelSender(context pipeline.Context, project, schema, table, partitionConfig string, timeRange, extraLevel int, tunnel *tunnel.Tunnel) TunnelSender {
	cache := NewSessionCache(context, project, schema, table, tunnel)
	return &TunnelSenderImpl{
		context:         context,
		projectName:     project,
		schemaName:      schema,
		tableName:       table,
		partitionConfig: partitionConfig,
		timeRange:       timeRange,
		tunnelIns:       tunnel,
		partition:       NewPartitionHelper(),
		sessionCache:    cache,
		recordBuilder:   NewRecordBuilder(extraLevel),
	}
}

type TunnelSenderImpl struct {
	context         pipeline.Context
	projectName     string
	schemaName      string
	tableName       string
	partitionConfig string
	timeRange       int
	tunnelIns       *tunnel.Tunnel
	partition       *PartitionHelper
	sessionCache    *SessionCache
	recordBuilder   RecordBuilder
}

func (ts *TunnelSenderImpl) Init() error {
	ts.recordBuilder.Init()

	if err := ts.partition.Init(ts.partitionConfig, ts.timeRange); err != nil {
		return err
	}

	logger.Infof(ts.context.GetRuntimeContext(), "Init odps tunnel sender success, config:[%s] columns:%v", ts.partitionConfig, ts.partition.columns)

	return nil
}
func (ts *TunnelSenderImpl) Send(logGroup *protocol.LogGroup, log *protocol.Log) error {
	partitionStr := ts.partition.GenPartition(log)
	session, writer, err := ts.sessionCache.GetWriter(partitionStr)
	if err != nil {
		return err
	}

	record, err := ts.recordBuilder.Log2Record(logGroup, log, session.Schema())
	if err != nil {
		return fmt.Errorf("log to record failed, err: %w", err)
	}

	err = writer.Append(record)
	if err != nil {
		return fmt.Errorf("append record failed, err: %w", err)
	}

	return nil
}

func (ts *TunnelSenderImpl) Flush() error {
	partitions, writers := ts.sessionCache.GetAllWriter()
	for idx, partition := range partitions {
		writer := writers[idx]
		if writer.RecordCount() > 0 {
			start := time.Now()
			traceId, recordCount, bytesSend, err := writer.Flush()
			cost := time.Since(start)
			if err != nil {
				return fmt.Errorf("Flush data failed, partition:%s traceId: %s, err: %v", partition, traceId, err)
			} else {
				logger.Infof(ts.context.GetRuntimeContext(),
					"Flush odps(%s/%s/%s) success, partition:%s traceId: %s, recordCount: %d, bytesSend: %d, cost:%v",
					ts.projectName, ts.schemaName, ts.tableName, partition, traceId, recordCount, bytesSend, cost)
			}
		}
	}

	return nil
}
