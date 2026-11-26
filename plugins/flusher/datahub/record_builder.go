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
	"encoding/json"
	"fmt"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/shopspring/decimal"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

const (
	schemaFreshInterval = 3 * time.Minute
	blobContentColumn   = "content"
)

const (
	HostIPKey      = "host_ip"
	CollectPathKey = "path"
	HostnameKey    = "hostname"
	CollectTimeKey = "collect_time"
	FlushTimeKey   = "flush_time"
)

const (
	LogtailPath  = "__path__"
	LogtailTopic = "__topic__"
)

type RecordBuilder interface {
	Init() error
	Build(fields map[string]string) (datahub.IRecord, error)
}

func NewRecordBuilder(projectName string, topicName string, extraLevel int, client datahub.DataHubApi, ctx pipeline.Context) RecordBuilder {
	return &RecordBuilderImpl{
		projectName:   projectName,
		topicName:     topicName,
		extraLevel:    extraLevel,
		client:        client,
		context:       ctx,
		nextFreshTime: time.Now(),
	}
}

type RecordBuilderImpl struct {
	projectName   string
	topicName     string
	extraLevel    int
	client        datahub.DataHubApi
	schema        *datahub.RecordSchema
	context       pipeline.Context
	nextFreshTime time.Time

	hostIP   string
	hostname string
}

func (rb *RecordBuilderImpl) Init() error {
	rb.hostIP = config.LoongcollectorGlobalConfig.HostIP
	rb.hostname = config.LoongcollectorGlobalConfig.Hostname

	return rb.doFreshRecordSchema()
}

func (rb *RecordBuilderImpl) doFreshRecordSchema() error {
	gt, err := rb.client.GetTopic(rb.projectName, rb.topicName)
	if err != nil {
		return err
	}

	if gt.RecordType == datahub.TUPLE {
		rb.schema = gt.RecordSchema
		rb.nextFreshTime = time.Now().Add(schemaFreshInterval)
	} else {
		// Set a large value for BLOB topic
		rb.nextFreshTime = time.Now().Add(1e6 * time.Hour)
	}

	return nil
}

func (rb *RecordBuilderImpl) freshRecordSchema() error {
	if time.Now().Before(rb.nextFreshTime) {
		return nil
	}

	return rb.doFreshRecordSchema()
}

func (rb *RecordBuilderImpl) Build(fields map[string]string) (datahub.IRecord, error) {
	if err := rb.freshRecordSchema(); err != nil {
		return nil, err
	}

	var record datahub.IRecord
	var err error
	if rb.schema == nil {
		record, err = rb.log2BlobRecord(fields)
	} else {
		record, err = rb.log2TupleRecord(fields)
	}

	if err != nil {
		return nil, err
	}

	return record, nil
}

func (rb *RecordBuilderImpl) log2TupleRecord(fields map[string]string) (datahub.IRecord, error) {
	record := datahub.NewTupleRecord(rb.schema, 0)
	for key, value := range fields {
		filed, err := rb.schema.GetFieldByName(key)
		if err != nil {
			return nil, err
		}

		if value == "" {
			if !filed.AllowNull {
				return nil, fmt.Errorf("field %s is not nullable", key)
			}
			continue
		}

		val, err := validateFieldValue(filed.Type, value)
		if err != nil {
			return nil, err
		}
		record.SetValueByName(key, val)
	}
	return record, nil
}

func (rb *RecordBuilderImpl) log2BlobRecord(fields map[string]string) (datahub.IRecord, error) {
	if len(fields) == 0 {
		return nil, fmt.Errorf("log is empty")
	}
	if value, ok := fields[blobContentColumn]; len(fields) == 1 && ok {
		return datahub.NewBlobRecord([]byte(value), 0), nil
	}
	buf, err := json.Marshal(fields)
	if err != nil {
		return nil, fmt.Errorf("marshal DataHub record: %w", err)
	}
	record := datahub.NewBlobRecord(buf, 0)
	return record, nil
}

func validateFieldValue(fieldType datahub.FieldType, value string) (interface{}, error) {
	switch fieldType {
	case datahub.STRING:
		return value, nil
	case datahub.BIGINT:
		return strconv.ParseInt(value, 10, 64)
	case datahub.INTEGER:
		return strconv.ParseInt(value, 10, 32)
	case datahub.SMALLINT:
		return strconv.ParseInt(value, 10, 16)
	case datahub.TINYINT:
		return strconv.ParseInt(value, 10, 8)
	case datahub.FLOAT:
		tmp, err := strconv.ParseFloat(value, 32)
		if err != nil {
			return nil, err
		}
		return float32(tmp), nil
	case datahub.DOUBLE:
		return strconv.ParseFloat(value, 64)
	case datahub.TIMESTAMP:
		return strconv.ParseUint(value, 10, 64)
	case datahub.BOOLEAN:
		return strconv.ParseBool(value)
	case datahub.DECIMAL:
		return decimal.NewFromString(value)
	default:
		return nil, fmt.Errorf("unsupported field type %s", fieldType)
	}
}
