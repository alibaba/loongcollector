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

package odps

import (
	"regexp"
	"testing"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/datatype"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tableschema"
	"github.com/stretchr/testify/assert"
)

func TestGenRecordWithoutParse(t *testing.T) {
	builder := RecordBuilderImpl{
		hostIP:     "127.0.0.1",
		hostname:   "hostname",
		extraLevel: 1,
	}

	logGroup := &protocol.LogGroup{
		LogTags: []*protocol.LogTag{
			{
				Key:   "__path__",
				Value: "/tmp/test.log",
			},
		},
	}

	log := &protocol.Log{
		Time: 1733205626,
	}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "content",
			Value: "value",
		},
	}

	schemaBuilder := tableschema.NewSchemaBuilder()
	schemaBuilder.Column(tableschema.Column{Name: "content", Type: datatype.StringType})
	schema := schemaBuilder.Build()

	record, err := builder.Log2Record(logGroup, log, &schema)
	assert.Nil(t, err)
	assert.Equal(t, 1, record.Len())
	assert.Equal(t, "[value]", record.String())
}

func TestGenRecordWithParse(t *testing.T) {
	builder := RecordBuilderImpl{
		hostIP:   "127.0.0.1",
		hostname: "hostname",
	}

	logGroup := &protocol.LogGroup{
		LogTags: []*protocol.LogTag{
			{
				Key:   "__path__",
				Value: "/tmp/test.log",
			},
		},
	}

	log := &protocol.Log{
		Time: 1733205626,
	}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "f1",
			Value: "true",
		},
		{
			Key:   "f2",
			Value: "1",
		},
		{
			Key:   "f3",
			Value: "123",
		},
		{
			Key:   "f4",
			Value: "123456",
		},
		{
			Key:   "f5",
			Value: "123456789",
		},
		{
			Key:   "f6",
			Value: "3.14",
		},
		{
			Key:   "f7",
			Value: "123.456",
		},
		{
			Key:   "f8",
			Value: "2024-12-04",
		},
		{
			Key:   "f9",
			Value: "2024-12-04 10:00:00",
		},
		{
			Key:   "f10",
			Value: "2024-12-04 10:00:00.000",
		},
		{
			Key:   "f11",
			Value: "2024-12-04 10:00:00.000",
		},
		{
			Key:   "f12",
			Value: "aaaa",
		},
		{
			Key:   "f13",
			Value: "bbbb",
		},
		{
			Key:   "f14",
			Value: "cccc",
		},
		{
			Key:   "f15",
			Value: "dddd",
		},
		{
			Key:   "f16",
			Value: "xxxx",
		},
	}

	schemaBuilder := tableschema.NewSchemaBuilder()
	schemaBuilder.Column(tableschema.Column{Name: "f1", Type: datatype.BooleanType})
	schemaBuilder.Column(tableschema.Column{Name: "f2", Type: datatype.TinyIntType})
	schemaBuilder.Column(tableschema.Column{Name: "f3", Type: datatype.SmallIntType})
	schemaBuilder.Column(tableschema.Column{Name: "f4", Type: datatype.IntType})
	schemaBuilder.Column(tableschema.Column{Name: "f5", Type: datatype.BigIntType})
	schemaBuilder.Column(tableschema.Column{Name: "f6", Type: datatype.FloatType})
	schemaBuilder.Column(tableschema.Column{Name: "f7", Type: datatype.DoubleType})
	schemaBuilder.Column(tableschema.Column{Name: "f8", Type: datatype.DateType})
	schemaBuilder.Column(tableschema.Column{Name: "f9", Type: datatype.DateTimeType})
	schemaBuilder.Column(tableschema.Column{Name: "f10", Type: datatype.TimestampType})
	schemaBuilder.Column(tableschema.Column{Name: "f11", Type: datatype.TimestampNtzType})
	schemaBuilder.Column(tableschema.Column{Name: "f12", Type: datatype.StringType})
	schemaBuilder.Column(tableschema.Column{Name: "f13", Type: datatype.NewCharType(100)})
	schemaBuilder.Column(tableschema.Column{Name: "f14", Type: datatype.NewVarcharType(100)})
	schemaBuilder.Column(tableschema.Column{Name: "f15", Type: datatype.BinaryType})
	schemaBuilder.Column(tableschema.Column{Name: "f16", Type: datatype.NewJsonType()})
	schema := schemaBuilder.Build()

	record, err := builder.Log2Record(logGroup, log, &schema)
	assert.Nil(t, err)
	assert.Equal(t, 16, record.Len())
	expect := "[true, 1, 123, 123456, 123456789, 3.140000E+00, 1.234560E+02, 2024-12-04, 2024-12-04 10:00:00, 2024-12-04 10:00:00.000, 2024-12-04 10:00:00.000, aaaa, bbbb, cccc, unhex('64646464'), \"xxxx\"]"
	assert.Equal(t, expect, record.String())
}

func TestUnsupportType(t *testing.T) {
	builder := RecordBuilderImpl{}

	logGroup := &protocol.LogGroup{}

	log := &protocol.Log{}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "f1",
			Value: "value",
		},
		{
			Key:   "f2",
			Value: "value",
		},
	}

	schemaBuilder := tableschema.NewSchemaBuilder()
	schemaBuilder.Column(tableschema.Column{Name: "f1", Type: datatype.StringType})
	schemaBuilder.Column(tableschema.Column{Name: "f2", Type: datatype.NewArrayType(datatype.StringType)})
	schema := schemaBuilder.Build()

	_, err := builder.Log2Record(logGroup, log, &schema)
	assert.NotNil(t, err)
}

func TestTypeMissMatch(t *testing.T) {
	builder := RecordBuilderImpl{}

	logGroup := &protocol.LogGroup{}

	log := &protocol.Log{}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "f1",
			Value: "value",
		},
		{
			Key:   "f2",
			Value: "value",
		},
	}

	schemaBuilder := tableschema.NewSchemaBuilder()
	schemaBuilder.Column(tableschema.Column{Name: "f1", Type: datatype.StringType})
	schemaBuilder.Column(tableschema.Column{Name: "f2", Type: datatype.BigIntType})
	schema := schemaBuilder.Build()

	_, err := builder.Log2Record(logGroup, log, &schema)
	assert.NotNil(t, err)
}

func TestGenRecordWithExtraInfo(t *testing.T) {
	builder := RecordBuilderImpl{
		hostIP:     "127.0.0.1",
		hostname:   "hostname",
		extraLevel: 1,
	}

	logGroup := &protocol.LogGroup{
		LogTags: []*protocol.LogTag{
			{
				Key:   "__path__",
				Value: "/tmp/test.log",
			},
		},
	}

	log := &protocol.Log{
		Time: 1733205626,
	}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "content",
			Value: "value",
		},
	}

	schemaBuilder := tableschema.NewSchemaBuilder()
	schemaBuilder.Column(tableschema.Column{Name: "content", Type: datatype.StringType})
	schemaBuilder.Column(tableschema.Column{Name: "__dwarf_info__", Type: datatype.NewJsonType()})
	schema := schemaBuilder.Build()

	record, err := builder.Log2Record(logGroup, log, &schema)
	assert.Nil(t, err)
	assert.Equal(t, 2, record.Len())
	assert.Equal(t, "[value, {\"host_ip\":\"127.0.0.1\",\"path\":\"/tmp/test.log\"}]", record.String())

	builder.extraLevel = 2
	record, err = builder.Log2Record(logGroup, log, &schema)
	assert.Nil(t, err)
	assert.Equal(t, 2, record.Len())
	re := regexp.MustCompile(`\[value, \{"host_ip":"127\.0\.0\.1","path":"/tmp/test\.log","hostname":"hostname","collect_time":"1733205626","flush_time":"\d+"\}\]`)
	assert.Truef(t, re.MatchString(record.String()), "regexp match failed, result:%s", record.String())
}
