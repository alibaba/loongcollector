// Copyright 2024 iLogtail Authors
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

package doris

import (
	"bytes"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
	"github.com/bingquanzhao/go-doris-sdk/pkg/load"
)

// FlusherDoris implements a data flusher that sends logs to Apache Doris via Stream Load.
// It provides efficient buffering and batch processing capabilities to optimize
// the performance of data loading into Doris.
type FlusherDoris struct {
	// Basic connection configuration
	Addresses []string // List of Doris FE addresses in format "host:port"
	Database  string   // Target Doris database name
	// Authentication related configuration
	Authentication Authentication
	// Table name configuration
	Table          string            // Target Doris table name
	LoadProperties map[string]string // Additional Stream Load properties to set in header

	dorisClient *load.DorisLoadClient
	context     pipeline.Context
	converter   *converter.Converter
	Convert     convertConfig
}

type convertConfig struct {
	// Rename one or more fields from tags
	TagFieldsRename map[string]string
	// Rename one or more fields, The protocol field options can only be: contents, tags, time
	ProtocolFieldsRename map[string]string
	// Convert protocol, default value: custom_single
	Protocol string
	// Convert encoding, default value: json
	Encoding string
}

type FlusherFunc func(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error

func NewFlusherDoris() *FlusherDoris {
	return &FlusherDoris{
		Addresses: []string{},
		Authentication: Authentication{
			PlainText: &PlainTextConfig{
				Username: "",
				Password: "",
				Database: "",
			},
		},

		Table: "",
		Convert: convertConfig{
			Protocol: converter.ProtocolCustomSingle,
			Encoding: converter.EncodingJSON,
		},
	}
}

func (f *FlusherDoris) Init(context pipeline.Context) error {
	f.context = context
	// Validate config of flusher
	if err := f.Validate(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher fail, error", err)
		return err
	}
	// Set default value while not set
	if f.Convert.Encoding == "" {
		f.Convert.Encoding = converter.EncodingJSON
	}
	if f.Convert.Protocol == "" {
		f.Convert.Protocol = converter.ProtocolCustomSingle
	}
	// Init converter
	convert, err := f.getConverter()
	if err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher converter fail, error", err)
		return err
	}
	f.converter = convert

	// Init Doris client
	if err := f.initDorisClient(); err != nil {
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris client fail, error", err)
		return err
	}

	return nil
}

func (f *FlusherDoris) getConverter() (*converter.Converter, error) {
	logger.Debug(f.context.GetRuntimeContext(), "[ilogtail data convert config] Protocol", f.Convert.Protocol,
		"Encoding", f.Convert.Encoding, "TagFieldsRename", f.Convert.TagFieldsRename, "ProtocolFieldsRename", f.Convert.ProtocolFieldsRename)
	return converter.NewConverter(f.Convert.Protocol, f.Convert.Encoding, f.Convert.TagFieldsRename, f.Convert.ProtocolFieldsRename, f.context.GetPipelineScopeConfig())
}

func (f *FlusherDoris) Description() string {
	return "Doris flusher for logtail"
}

// initDorisClient initializes the Doris Stream Load client
func (f *FlusherDoris) initDorisClient() error {
	// Get authentication credentials
	username, password, err := f.Authentication.GetUsernamePassword()
	if err != nil {
		return fmt.Errorf("failed to get authentication credentials: %w", err)
	}

	// Create Doris SDK configuration
	config := &load.Config{
		Endpoints:   f.Addresses,
		User:        username,
		Password:    password,
		Database:    f.Database,
		Table:       f.Table,
		Format:      load.DefaultJSONFormat(),
		Retry:       load.DefaultRetry(),
		GroupCommit: load.ASYNC,
		Options:     f.LoadProperties,
	}

	// Create Doris client
	client, err := load.NewLoadClient(config)
	if err != nil {
		return fmt.Errorf("failed to create doris client: %w", err)
	}

	f.dorisClient = client
	logger.Info(f.context.GetRuntimeContext(), "Doris client initialized successfully",
		"endpoints", f.Addresses, "database", f.Database, "table", f.Table)

	return nil
}

func (f *FlusherDoris) Validate() error {
	if f.Addresses == nil || len(f.Addresses) == 0 {
		var err = fmt.Errorf("doris addrs is nil")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher error", err)
		return err
	}
	if f.Table == "" {
		var err = fmt.Errorf("doris table is nil")
		logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_INIT_ALARM", "init doris flusher error", err)
		return err
	}
	return nil
}

func (f *FlusherDoris) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	if f.dorisClient == nil {
		return fmt.Errorf("doris client not initialized")
	}

	for _, logGroup := range logGroupList {
		logger.Debug(f.context.GetRuntimeContext(), "[LogGroup] topic", logGroup.Topic, "logstore", logGroup.Category, "logcount", len(logGroup.Logs), "tags", logGroup.LogTags)

		// Convert log group to byte stream
		serializedLogs, err := f.converter.ToByteStream(logGroup)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush doris convert log fail, error", err)
			continue
		}

		// Combine all logs into a single buffer
		var buffer bytes.Buffer
		logCount := 0
		for _, log := range serializedLogs.([][]byte) {
			buffer.Write(log)
			buffer.WriteByte('\n') // Add newline separator for JSON object line format
			logCount++
		}

		if buffer.Len() == 0 {
			logger.Debug(f.context.GetRuntimeContext(), "No logs to flush")
			continue
		}

		// Load data to Doris using SDK
		logger.Debug(f.context.GetRuntimeContext(), "Loading data to Doris", "logCount", logCount, "dataSize", buffer.Len())
		response, err := f.dorisClient.Load(&buffer)
		if err != nil {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM", "flush doris load fail, error", err)
			return fmt.Errorf("failed to load data to doris: %w", err)
		}

		if response.Status == load.SUCCESS {
			logger.Info(f.context.GetRuntimeContext(), "Doris load success",
				"loadedRows", response.Resp.NumberLoadedRows,
				"loadBytes", response.Resp.LoadBytes,
				"label", response.Resp.Label)
		} else {
			logger.Warning(f.context.GetRuntimeContext(), "FLUSHER_FLUSH_ALARM",
				"doris load failed with status", response.Status,
				"message", response.ErrorMessage)
			return fmt.Errorf("doris load failed: %s", response.ErrorMessage)
		}
	}

	return nil
}

func (f *FlusherDoris) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return f.dorisClient != nil
}

func (f *FlusherDoris) SetUrgent(flag bool) {}

func (f *FlusherDoris) Stop() error {
	return nil
}

// Register the plugin to the Flushers array.
func init() {
	pipeline.Flushers["flusher_doris"] = func() pipeline.Flusher {
		f := NewFlusherDoris()
		return f
	}
}
