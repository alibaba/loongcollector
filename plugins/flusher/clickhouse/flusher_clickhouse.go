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

package clickhouse

import (
	"context"
	"fmt"
	"net"
	"time"

	"github.com/ClickHouse/clickhouse-go/v2"
	"github.com/ClickHouse/clickhouse-go/v2/lib/driver"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

type FlusherClickHouse struct {
	// Convert ilogtail data convert config
	Convert convertConfig
	// Addresses clickhouse addresses
	Addresses []string
	// Authentication using PLAIN
	Authentication Authentication
	// Cluster ClickHouse cluster name
	Cluster string
	// Table Target of data writing
	Table string
	// Compression Data compression strategy
	Compression string
	// MaxExecutionTime Timeout duration of a single request, unit: second , the default is  60
	MaxExecutionTime int
	// DialTimeout Dial timeout, the default is 10s
	DialTimeout time.Duration
	// MaxOpenConns Maximum number of connections, including long and short connections, the default is 5
	MaxOpenConns int
	// MaxIdleConns The maximum number of idle connections, that is the size of the connection pool, the default is 5
	MaxIdleConns int
	// ConnMaxLifetime Connection maximum lifetime, the default is 10m
	ConnMaxLifetime time.Duration
	// BlockBufferSize, size of block buffer, the default is 10
	BlockBufferSize uint8

	// ClickHouse Buffer engine configuration
	// The number of buffer, recommended as 16
	// Data is flushed to disk when all min conditions are met, or when one max condition is met
	BufferNumLayers int
	BufferMinTime   int
	BufferMaxTime   int
	BufferMinRows   int
	BufferMaxRows   int
	BufferMinBytes  int
	BufferMaxBytes  int

	context   pipeline.Context
	converter *converter.Converter
	conn      driver.Conn
	flusher   FlusherFunc
}

type convertConfig struct {
	// Rename one or more fields from tags.
	TagFieldsRename map[string]string
	// Rename one or more fields, The protocol field options can only be: contents, tags, time
	ProtocolFieldsRename map[string]string
	// Convert protocol, default value: custom_single
	Protocol string
	// Convert encoding, default value:json
	// The options are: 'json'
	Encoding string
}

type FlusherFunc func(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

func NewFlusherClickHouse() *FlusherClickHouse {
	return &FlusherClickHouse{
		Addresses: []string{},
		Authentication: Authentication{
			PlainText: &PlainTextConfig{
				Username: "",
				Password: "",
				Database: "",
			},
		},
		Cluster:          "",
		Table:            "",
		Compression:      "lz4",
		MaxExecutionTime: 60,
		DialTimeout:      time.Duration(10) * time.Second,
		MaxOpenConns:     5,
		MaxIdleConns:     5,
		ConnMaxLifetime:  time.Duration(10) * time.Minute,
		BlockBufferSize:  10,
		BufferNumLayers:  16,
		BufferMinTime:    10,
		BufferMaxTime:    100,
		BufferMinRows:    10000,
		BufferMaxRows:    1000000,
		BufferMinBytes:   10000000,
		BufferMaxBytes:   100000000,
		Convert: convertConfig{
			Protocol: converter.ProtocolCustomSingle,
			Encoding: converter.EncodingJSON,
		},
	}
}

func (f *FlusherClickHouse) Init(context pipeline.Context) error {
	f.context = context
	// Validate config of flusher
	if err := f.Validate(); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher fail, error", err)
		return err
	}
	// Set default value while not set
	if f.Convert.Encoding == "" {
		f.converter.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	// Init converter
	convert, err := f.getConverter()
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher converter fail, error", err)
		return err
	}
	f.converter = convert
	conn, err := newConn(f)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return err
	}
	f.conn = conn
	if err = createNullBufferTable(f); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return err
	}
	return nil
}

func (f *FlusherClickHouse) Description() string {
	return "ClickHouse flusher for logtail"
}

func (f *FlusherClickHouse) Validate() error {
	if f.Addresses == nil || len(f.Addresses) == 0 {
		var err = fmt.Errorf("clickhouse addrs is nil")
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return err
	}
	if f.Table == "" {
		var err = fmt.Errorf("clickhouse table is nil")
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return err
	}
	return nil
}

func (f *FlusherClickHouse) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	return converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

func (f *FlusherClickHouse) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	return f.flusher(projectName, logstoreName, configName, logGroupList)
}

func (f *FlusherClickHouse) BufferFlush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	ctx := context.Background()
	for _, logGroup := range logGroupList {
		logger.Debug(f.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)
		// Merge topicKeys and HashKeys,Only one convert after merge
		serializedLogs, err := f.converter.ToByteStream(logGroup)
		if err != nil {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush clickhouse convert log fail, error", err)
		}
		for _, log := range serializedLogs.([][]byte) {
			sql := fmt.Sprintf("INSERT INTO %s.ilogtail_%s_buffer (_timestamp, _log) VALUES (%d, '%s')", f.Authentication.PlainText.Database, f.Table, time.Now().Unix(), string(log))
			err = f.conn.AsyncInsert(ctx, sql, false)
			if err != nil {
				logger.Error(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush clickhouse AsyncInsert fail, error", err)
			}
		}
		logger.Debug(f.context.GetRuntimeContext(), "ClickHouse success send events: messageID")
	}
	return nil
}

func (f *FlusherClickHouse) SetUrgent(flag bool) {}

func (f *FlusherClickHouse) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.conn != nil
}

func (f *FlusherClickHouse) Stop() error {
	return f.conn.Close()
}

func init() {
	pipeline.Flushers["flusher_clickhouse"] = func() pipeline.Flusher {
		f := NewFlusherClickHouse()
		f.flusher = f.BufferFlush
		return f
	}
}

func newConn(f *FlusherClickHouse) (driver.Conn, error) {
	compression, err := compressionMethod(f.Compression)
	if err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return nil, err
	}
	opt := &clickhouse.Options{
		Addr: f.Addresses,
		DialContext: func(ctx context.Context, addr string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, "tcp", addr)
		},
		Settings: clickhouse.Settings{
			"max_execution_time": f.MaxExecutionTime,
		},
		Compression: &clickhouse.Compression{
			Method: compression,
		},
		DialTimeout:      f.DialTimeout,
		MaxOpenConns:     f.MaxOpenConns,
		MaxIdleConns:     f.MaxIdleConns,
		ConnMaxLifetime:  f.ConnMaxLifetime,
		ConnOpenStrategy: clickhouse.ConnOpenInOrder,
		BlockBufferSize:  f.BlockBufferSize,
	}
	if err = f.Authentication.ConfigureAuthentication(opt); err != nil {
		err = fmt.Errorf("configure authenticationfailed, err: %w", err)
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return nil, err
	}
	conn, err := clickhouse.Open(opt)
	if err != nil {
		err = fmt.Errorf("sql open failed, err: %w", err)
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init clickhouse flusher error", err)
		return nil, err
	}
	if err = conn.Ping(context.Background()); err != nil {
		if exception, ok := err.(*clickhouse.Exception); ok {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "code", exception.Code, "msg", exception.Message, "trace", exception.StackTrace)
		} else {
			logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "error", err)
		}
		return nil, err
	}
	return conn, nil
}

func createNullBufferTable(f *FlusherClickHouse) error {
	sqlNullTableName := fmt.Sprintf("`%s`.`ilogtail_%s`", f.Authentication.PlainText.Database, f.Table)
	sqlBufferTableName := fmt.Sprintf("`%s`.`ilogtail_%s_buffer`", f.Authentication.PlainText.Database, f.Table)
	if f.Cluster != "" {
		sqlNullTableName = fmt.Sprintf("%s on cluster '%s'", sqlNullTableName, f.Cluster)
		sqlBufferTableName = fmt.Sprintf("%s on cluster '%s'", sqlBufferTableName, f.Cluster)
	}
	sqlNull := fmt.Sprintf("CREATE TABLE IF NOT EXISTS %s (`_timestamp` Int64,`_log` String) ENGINE = Null", sqlNullTableName)
	if err := f.conn.Exec(context.Background(), sqlNull); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "sql", sqlNull, "error", err)
		return err
	}
	sqlBuffer := fmt.Sprintf("CREATE TABLE IF NOT EXISTS %s AS `%s`.`ilogtail_%s` ENGINE = Buffer(%s, ilogtail_%s, %d, %d, %d, %d, %d, %d, %d)",
		sqlBufferTableName,
		f.Authentication.PlainText.Database, f.Table,
		f.Authentication.PlainText.Database, f.Table,
		f.BufferNumLayers, f.BufferMinTime, f.BufferMaxTime, f.BufferMinRows, f.BufferMaxRows, f.BufferMinBytes, f.BufferMaxBytes,
	)
	if err := f.conn.Exec(context.Background(), sqlBuffer); err != nil {
		logger.Error(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "sql", sqlBuffer, "error", err)
		return err
	}
	return nil
}

func compressionMethod(compression string) (clickhouse.CompressionMethod, error) {
	switch compression {
	case "none":
		return clickhouse.CompressionNone, nil
	case "gzip":
		return clickhouse.CompressionGZIP, nil
	case "lz4":
		return clickhouse.CompressionLZ4, nil
	case "zstd":
		return clickhouse.CompressionZSTD, nil
	case "deflate":
		return clickhouse.CompressionDeflate, nil
	case "br":
		return clickhouse.CompressionBrotli, nil
	default:
		return clickhouse.CompressionNone, fmt.Errorf("producer.compression should be one of 'none', 'gzip', 'deflate', 'lz4', 'br' or 'zstd'. configured value %v", compression)
	}
}
