package datahub

import (
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/stretchr/testify/assert"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

func TestValidateFieldValue(t *testing.T) {
	val, err := validateFieldValue(datahub.STRING, "test")
	assert.Nil(t, err)
	assert.Equal(t, "test", val)

	val, err = validateFieldValue(datahub.STRING, "1")
	assert.Nil(t, err)
	assert.Equal(t, "1", val)

	val, err = validateFieldValue(datahub.INTEGER, "1")
	assert.Nil(t, err)
	assert.Equal(t, int64(1), val)

	_, err = validateFieldValue(datahub.INTEGER, "test")
	assert.Error(t, err)

	_, err = validateFieldValue(datahub.FLOAT, "1.1")
	assert.Nil(t, err)

	_, err = validateFieldValue(datahub.FLOAT, "test")
	assert.Error(t, err)

	val, err = validateFieldValue(datahub.BOOLEAN, "true")
	assert.Nil(t, err)
	assert.Equal(t, true, val)

	_, err = validateFieldValue(datahub.BOOLEAN, "test")
	assert.Error(t, err)

	val, err = validateFieldValue(datahub.TIMESTAMP, "1")
	assert.Nil(t, err)
	assert.Equal(t, uint64(1), val)

	_, err = validateFieldValue(datahub.TIMESTAMP, "test")
	assert.Error(t, err)

	_, err = validateFieldValue("unknown", "test")
	assert.Error(t, err)
}

func TestGenBlobRecord(t *testing.T) {
	builder := RecordBuilderImpl{}

	log := &protocol.Log{}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "content",
			Value: "value",
		},
	}
	record, err := builder.log2BlobRecord(log)
	assert.Nil(t, err)
	bRecord, ok := record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "value", string(bRecord.RawData))

	log.Contents = []*protocol.Log_Content{
		{
			Key:   "key1",
			Value: "val1",
		},
		{
			Key:   "key2",
			Value: "val2",
		},
		{
			Key:   "content",
			Value: "val3",
		},
	}
	record, err = builder.log2BlobRecord(log)
	assert.Nil(t, err)
	bRecord, ok = record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "{\"content\":\"val3\",\"key1\":\"val1\",\"key2\":\"val2\"}", string(bRecord.RawData))

	log.Contents = []*protocol.Log_Content{
		{
			Key:   "key1",
			Value: "val1",
		},
	}
	record, err = builder.log2BlobRecord(log)
	assert.Nil(t, err)
	bRecord, ok = record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "{\"key1\":\"val1\"}", string(bRecord.RawData))

	log.Contents = []*protocol.Log_Content{}
	_, err = builder.log2BlobRecord(log)
	assert.NotNil(t, err)
}

func TestGenTupleRecord(t *testing.T) {
	builder := RecordBuilderImpl{
		schema: datahub.NewRecordSchema(),
	}

	builder.schema.AddField(datahub.Field{Name: "f1", Type: datahub.BOOLEAN})
	builder.schema.AddField(datahub.Field{Name: "f2", Type: datahub.TINYINT})
	builder.schema.AddField(datahub.Field{Name: "f3", Type: datahub.SMALLINT})
	builder.schema.AddField(datahub.Field{Name: "f4", Type: datahub.INTEGER})
	builder.schema.AddField(datahub.Field{Name: "f5", Type: datahub.BIGINT})
	builder.schema.AddField(datahub.Field{Name: "f6", Type: datahub.TIMESTAMP})
	builder.schema.AddField(datahub.Field{Name: "f7", Type: datahub.FLOAT})
	builder.schema.AddField(datahub.Field{Name: "f8", Type: datahub.DOUBLE})
	builder.schema.AddField(datahub.Field{Name: "f9", Type: datahub.DECIMAL})
	builder.schema.AddField(datahub.Field{Name: "f10", Type: datahub.STRING})

	log := &protocol.Log{}
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
			Value: "1733205626",
		},
		{
			Key:   "f7",
			Value: "3.14",
		},
		{
			Key:   "f8",
			Value: "123.456",
		},
		{
			Key:   "f9",
			Value: "1234.56789",
		},
		{
			Key:   "f10",
			Value: "aaaabbbbcccc",
		},
	}

	record, err := builder.log2TupleRecord(log)
	assert.Nil(t, err)
	tRecord, ok := record.(*datahub.TupleRecord)
	assert.True(t, ok)
	val := tRecord.GetValueByName("f1")
	bVal, ok := val.(datahub.Boolean)
	assert.True(t, ok)
	assert.True(t, bool(bVal))
	val = tRecord.GetValueByName("f2")
	tVal, ok := val.(datahub.Tinyint)
	assert.True(t, ok)
	assert.Equal(t, int8(1), int8(tVal))
	val = tRecord.GetValueByName("f3")
	sVal, ok := val.(datahub.Smallint)
	assert.True(t, ok)
	assert.Equal(t, int16(123), int16(sVal))
	val = tRecord.GetValueByName("f4")
	iVal, ok := val.(datahub.Integer)
	assert.True(t, ok)
	assert.Equal(t, 123456, int(iVal))
	val = tRecord.GetValueByName("f5")
	gVal, ok := val.(datahub.Bigint)
	assert.True(t, ok)
	assert.Equal(t, int64(123456789), int64(gVal))
	val = tRecord.GetValueByName("f6")
	pVal, ok := val.(datahub.Timestamp)
	assert.True(t, ok)
	assert.Equal(t, uint64(pVal), uint64(pVal))
	val = tRecord.GetValueByName("f7")
	fVal, ok := val.(datahub.Float)
	assert.True(t, ok)
	assert.Equal(t, float32(3.14), float32(fVal))
	val = tRecord.GetValueByName("f8")
	dVal, ok := val.(datahub.Double)
	assert.True(t, ok)
	assert.Equal(t, 123.456, float64(dVal))
	val = tRecord.GetValueByName("f9")
	cVal, ok := val.(datahub.Decimal)
	assert.True(t, ok)
	assert.Equal(t, "1234.56789", cVal.String())
	val = tRecord.GetValueByName("f10")
	rVal, ok := val.(datahub.String)
	assert.True(t, ok)
	assert.Equal(t, "aaaabbbbcccc", string(rVal))
}

func TestGenTupleRecordWithNull(t *testing.T) {
	builder := RecordBuilderImpl{
		schema: datahub.NewRecordSchema(),
	}

	builder.schema.AddField(datahub.Field{Name: "f1", Type: datahub.BOOLEAN, AllowNull: true})
	builder.schema.AddField(datahub.Field{Name: "f2", Type: datahub.TINYINT, AllowNull: true})

	log := &protocol.Log{}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "f1",
			Value: "true",
		},
		{
			Key:   "f2",
			Value: "",
		},
	}

	record, err := builder.log2TupleRecord(log)
	assert.Nil(t, err)
	tRecord, ok := record.(*datahub.TupleRecord)
	assert.True(t, ok)
	val := tRecord.GetValueByName("f1")
	bVal, ok := val.(datahub.Boolean)
	assert.True(t, ok)
	assert.True(t, bool(bVal))
	val = tRecord.GetValueByName("f2")
	assert.Nil(t, val)

	builder.schema = datahub.NewRecordSchema()
	builder.schema.AddField(datahub.Field{Name: "f1", Type: datahub.BOOLEAN, AllowNull: true})
	builder.schema.AddField(datahub.Field{Name: "f2", Type: datahub.TINYINT, AllowNull: false})

	_, err = builder.log2TupleRecord(log)
	assert.NotNil(t, err)
}

func TestGenTupleRecordWithTypeMissMatch(t *testing.T) {
	builder := RecordBuilderImpl{
		schema: datahub.NewRecordSchema(),
	}

	builder.schema.AddField(datahub.Field{Name: "f1", Type: datahub.BOOLEAN, AllowNull: true})
	builder.schema.AddField(datahub.Field{Name: "f2", Type: datahub.TINYINT, AllowNull: true})

	log := &protocol.Log{}
	log.Contents = []*protocol.Log_Content{
		{
			Key:   "f1",
			Value: "123",
		},
		{
			Key:   "f2",
			Value: "2",
		},
	}

	_, err := builder.log2TupleRecord(log)
	assert.NotNil(t, err)
}

// Test for ExtraInfoColumn
func TestGenRecord(t *testing.T) {
	builder := RecordBuilderImpl{
		nextFreshTime: time.Now().Add(time.Hour),
		hostIp:        "127.0.0.1",
		hostname:      "hostname",
	}

	logGroup := &protocol.LogGroup{
		LogTags: []*protocol.LogTag{
			{
				Key:   "__path__",
				Value: "/tmp/test.log",
			},
		},
		Topic: "test_topic",
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

	builder.extraLevel = 0
	record, err := builder.Log2Record(logGroup, log)
	assert.Nil(t, err)
	bRecord, ok := record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "value", string(bRecord.RawData))
	attr := record.GetAttributes()
	assert.Equal(t, 0, len(attr))

	// level 1
	builder.extraLevel = 1
	record, err = builder.Log2Record(logGroup, log)
	assert.Nil(t, err)
	bRecord, ok = record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "value", string(bRecord.RawData))
	attr = record.GetAttributes()
	assert.Equal(t, 3, len(attr))
	path, ok := attr["path"]
	assert.True(t, ok)
	pathStr, ok := path.(string)
	assert.True(t, ok)
	assert.Equal(t, "/tmp/test.log", pathStr)
	ip, ok := attr["host_ip"]
	assert.True(t, ok)
	ipStr, ok := ip.(string)
	assert.True(t, ok)
	assert.Equal(t, "127.0.0.1", ipStr)
	topic, ok := attr["__topic__"]
	assert.True(t, ok)
	topicStr, ok := topic.(string)
	assert.True(t, ok)
	assert.Equal(t, "test_topic", topicStr)

	// level 2
	builder.extraLevel = 2
	record, err = builder.Log2Record(logGroup, log)
	assert.Nil(t, err)
	bRecord, ok = record.(*datahub.BlobRecord)
	assert.True(t, ok)
	assert.Equal(t, "value", string(bRecord.RawData))
	attr = record.GetAttributes()
	assert.Equal(t, 6, len(attr))
	path, ok = attr["path"]
	assert.True(t, ok)
	pathStr, ok = path.(string)
	assert.True(t, ok)
	assert.Equal(t, "/tmp/test.log", pathStr)
	ip, ok = attr["host_ip"]
	assert.True(t, ok)
	ipStr, ok = ip.(string)
	assert.True(t, ok)
	assert.Equal(t, "127.0.0.1", ipStr)
	topic, ok = attr["__topic__"]
	assert.True(t, ok)
	topicStr, ok = topic.(string)
	assert.True(t, ok)
	assert.Equal(t, "test_topic", topicStr)
	cTime, ok := attr["collect_time"]
	assert.True(t, ok)
	cTimeStr, ok := cTime.(string)
	assert.True(t, ok)
	assert.Equal(t, "1733205626", cTimeStr)
	hostname, ok := attr["hostname"]
	assert.True(t, ok)
	hostnameStr, ok := hostname.(string)
	assert.True(t, ok)
	assert.Equal(t, "hostname", hostnameStr)
	fTime, ok := attr["flush_time"]
	assert.True(t, ok)
	_, ok = fTime.(string)
	assert.True(t, ok)
}
