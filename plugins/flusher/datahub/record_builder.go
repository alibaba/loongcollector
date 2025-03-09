package datahub

import (
	"encoding/json"
	"fmt"
	"net"
	"os"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/shopspring/decimal"

	"github.com/aliyun/aliyun-datahub-sdk-go/datahub"
)

const (
	schemaFreshInterval = 3 * time.Minute
	blobContentColumn   = "content"
)

const (
	HostIpKey      = "host_ip"
	CollectPathKey = "path"
	HostnameKey    = "hostname"
	CollectTimeKey = "collect_time"
	FlushTimeKey   = "flush_time"
)

const (
	LogtailPath     = "__path__"
	LogtailHostname = "__hostname__"
	LogtailPackId   = "__pack_id__"
	LogtailTopic    = "__topic__"
)

type RecordBuilder interface {
	Init() error
	Log2Record(logGroup *protocol.LogGroup, log *protocol.Log) (datahub.IRecord, error)
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

	// 额外信息
	hostIp   string
	hostname string
}

func (rb *RecordBuilderImpl) Init() error {
	rb.hostIp = getLocalIp()
	rb.hostname, _ = os.Hostname()

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
		// blob的topic更新时间设置一个很大的值
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

func (rb *RecordBuilderImpl) Log2Record(logGroup *protocol.LogGroup, log *protocol.Log) (datahub.IRecord, error) {
	rb.freshRecordSchema()
	var record datahub.IRecord
	var err error
	if rb.schema == nil {
		record, err = rb.log2BlobRecord(log)
	} else {
		record, err = rb.log2TupleRecord(log)
	}

	if err != nil {
		return nil, err
	}

	rb.addExtraInfo(logGroup, log, record)
	return record, nil
}

func findLogTag(logTags []*protocol.LogTag, key string) (string, bool) {
	if logTags == nil {
		return "", false
	}

	for _, tag := range logTags {
		if tag.Key == key {
			return tag.Value, true
		}
	}

	return "", false
}

func (rb *RecordBuilderImpl) addExtraInfo(logGroup *protocol.LogGroup, log *protocol.Log, record datahub.IRecord) {
	rb.addLevelExtraInfo(logGroup, log, record, rb.extraLevel)
}

func (rb *RecordBuilderImpl) addLevelExtraInfo(logGroup *protocol.LogGroup, log *protocol.Log, record datahub.IRecord, level int) {
	if level <= 0 {
		return
	} else if level == 1 {
		record.SetAttribute(HostIpKey, rb.hostIp)
		if val, ok := findLogTag(logGroup.LogTags, LogtailPath); ok {
			record.SetAttribute(CollectPathKey, val)
		}

		if len(logGroup.Topic) > 0 {
			record.SetAttribute(LogtailTopic, logGroup.Topic)
		}
	} else if level == 2 {
		rb.addLevelExtraInfo(logGroup, log, record, level-1)
		record.SetAttribute(HostnameKey, rb.hostname)
		record.SetAttribute(CollectTimeKey, strconv.FormatUint(uint64(log.Time), 10))
		record.SetAttribute(FlushTimeKey, strconv.FormatInt(time.Now().UnixMilli(), 10))
	}
}

func (rb *RecordBuilderImpl) log2TupleRecord(log *protocol.Log) (datahub.IRecord, error) {
	record := datahub.NewTupleRecord(rb.schema, 0)
	for _, c := range log.Contents {
		filed, err := rb.schema.GetFieldByName(c.Key)
		if err != nil {
			return nil, err
		}

		if c.Value == "" {
			if !filed.AllowNull {
				return nil, fmt.Errorf("field %s is not nullable", c.Key)
			}
			continue
		}

		val, err := validateFieldValue(filed.Type, c.Value)
		if err != nil {
			return nil, err
		}
		record.SetValueByName(c.Key, val)
	}
	return record, nil
}

func (rb *RecordBuilderImpl) log2BlobRecord(log *protocol.Log) (datahub.IRecord, error) {
	if len(log.Contents) == 0 {
		return nil, fmt.Errorf("log is empty")
	}

	if len(log.Contents) == 1 && log.Contents[0].Key == blobContentColumn {
		return datahub.NewBlobRecord([]byte(log.Contents[0].Value), 0), nil
	}

	m := make(map[string]string)
	for _, c := range log.Contents {
		m[c.Key] = c.Value
	}

	buf, _ := json.Marshal(m)
	record := datahub.NewBlobRecord([]byte(buf), 0)
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
		if tmp, err := strconv.ParseFloat(value, 32); err != nil {
			return nil, err
		} else {
			return float32(tmp), nil
		}
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

func getLocalIp() string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return ""
	}
	for _, address := range addrs {
		if ipnet, ok := address.(*net.IPNet); ok && !ipnet.IP.IsLoopback() {
			if ipnet.IP.To4() != nil {
				return ipnet.IP.String()
			}
		}
	}
	return ""
}
