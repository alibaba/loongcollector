// Copyright 2025 iLogtail Authors
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

package datahub

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

const (
	compressTypeDefault = datahub.ZSTD
	extraLevelDefault   = 1
)

type FlusherDatahub struct {
	AccessKeyID     string
	AccessKeySecret string
	SecurityToken   string
	Endpoint        string
	ProjectName     string
	TopicName       string
	recordBuilder   RecordBuilder
	producer        Producer
	context         pipeline.Context
}

func NewFlusherDatahub() *FlusherDatahub {
	return &FlusherDatahub{
		AccessKeyID:     "",
		AccessKeySecret: "",
		SecurityToken:   "",
		Endpoint:        "",
		ProjectName:     "",
		TopicName:       "",
	}
}

func (d *FlusherDatahub) Init(context pipeline.Context) error {
	d.context = context
	logger.Debugf(d.context.GetRuntimeContext(), "Init datahub flusher:%v", *d)

	var account datahub.Account
	if len(d.SecurityToken) > 0 {
		account = datahub.NewStsCredential(d.AccessKeyID, d.AccessKeySecret, d.SecurityToken)
	} else {
		account = datahub.NewAliyunAccount(d.AccessKeyID, d.AccessKeySecret)
	}

	config := &datahub.Config{
		UserAgent:            fmt.Sprintf("loongcollector/%s-%s", config.BaseVersion, config.LoongcollectorGlobalConfig.HostIP),
		CompressorType:       compressTypeDefault,
		EnableBinary:         true,
		EnableSchemaRegistry: false,
		HttpClient:           datahub.DefaultHttpClient(),
	}

	client := datahub.NewClientWithConfig(d.Endpoint, config, account)

	d.recordBuilder = NewRecordBuilder(d.ProjectName, d.TopicName, extraLevelDefault, client, d.context)
	err := d.recordBuilder.Init()
	if err != nil {
		logger.Errorf(d.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Init datahub(%s/%s) record builder failed, error:%v", d.ProjectName, d.TopicName, err)
		return err
	}
	logger.Debugf(d.context.GetRuntimeContext(), "Init datahub(%s/%s) record builder success", d.ProjectName, d.TopicName)

	d.producer = NewProducer(d.ProjectName, d.TopicName, client, d.context)
	err = d.producer.Init()
	if err != nil {
		logger.Errorf(d.context.GetRuntimeContext(), "DATAHUB_FLUSHER_ALARM", "Init datahub(%s/%s) producer failed, error:%v", d.ProjectName, d.TopicName, err)
		return err
	}
	logger.Infof(d.context.GetRuntimeContext(), "Init datahub(%s/%s) producer success", d.ProjectName, d.TopicName)

	return nil
}

func (d *FlusherDatahub) Description() string {
	return "datahub flusher"
}

func (d *FlusherDatahub) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		records := make([]datahub.IRecord, len(logGroup.Logs))
		for id, log := range logGroup.Logs {
			record, err := d.recordBuilder.Log2Record(logGroup, log)
			if err != nil {
				return err
			}
			records[id] = record
		}
		err := d.producer.Send(records)
		if err != nil {
			return err
		}
	}
	return nil
}

func (d *FlusherDatahub) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return d.producer != nil && d.recordBuilder != nil
}

func (d *FlusherDatahub) SetUrgent(flag bool) {
}

func (d *FlusherDatahub) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_aliyun_datahub"] = func() pipeline.Flusher {
		f := NewFlusherDatahub()
		return f
	}
}
