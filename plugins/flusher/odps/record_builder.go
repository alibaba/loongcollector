package odps

import (
	"fmt"
	"net"
	"os"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/data"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/datatype"
	"github.com/aliyun/aliyun-odps-go-sdk/odps/tableschema"
)

const (
	ExtraInfoColumn = "__dwarf_info__"

	LogtailPath     = "__path__"
	LogtailHostname = "__hostname__"
	LogtailPackId   = "__pack_id__"
)

type RecordBuilder interface {
	Init()
	Log2Record(logGroup *protocol.LogGroup, log *protocol.Log, schema *tableschema.TableSchema) (data.Record, error)
}

type ExtraInfo struct {
	HostIp      string `json:"host_ip,omitempty"`
	CollectPath string `json:"path,omitempty"`
	Hostname    string `json:"hostname,omitempty"`
	CollectTime string `json:"collect_time,omitempty"`
	FlushTime   string `json:"flush_time,omitempty"`
}

func NewRecordBuilder(extraLevel int) RecordBuilder {
	return &RecordBuilderImpl{
		extraLevel: extraLevel,
	}
}

type RecordBuilderImpl struct {
	extraLevel int
	hostIp     string
	hostname   string
}

func (rb *RecordBuilderImpl) Init() {
	rb.hostname, _ = os.Hostname()
	rb.hostIp = getLocalIp()
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

func (rb *RecordBuilderImpl) genExtraInfo(logGroup *protocol.LogGroup, log *protocol.Log) ExtraInfo {
	info := ExtraInfo{}

	if rb.extraLevel >= 1 {
		if path, ok := findLogTag(logGroup.LogTags, LogtailPath); ok {
			info.CollectPath = path
		}
		info.HostIp = rb.hostIp
	}

	if rb.extraLevel >= 2 {
		info.Hostname = rb.hostname
		info.CollectTime = strconv.FormatUint(uint64(log.Time), 10)
		info.FlushTime = strconv.FormatInt(time.Now().UnixMilli(), 10)
	}
	return info
}

func (rb *RecordBuilderImpl) Log2Record(logGroup *protocol.LogGroup, log *protocol.Log, schema *tableschema.TableSchema) (data.Record, error) {
	record := data.NewRecord(len(schema.Columns))

	// 这个双层for循环性能很差，后续可以优化一下
	for idx, column := range schema.Columns {
		if column.Name == ExtraInfoColumn && column.Type.ID() == datatype.JSON {
			val, err := data.NewJson(rb.genExtraInfo(logGroup, log))
			if err != nil {
				return nil, err
			}
			record[idx] = val
			continue
		}

		var value *string = nil
		for _, content := range log.Contents {
			if column.Name == content.Key {
				value = &content.Value
				break
			}
		}

		if value == nil {
			record[idx] = nil
			continue
		}

		if *value == "" &&
			column.Type.ID() != datatype.STRING &&
			column.Type.ID() != datatype.CHAR &&
			column.Type.ID() != datatype.VARCHAR {
			record[idx] = nil
			continue
		}

		switch column.Type.ID() {
		case datatype.NULL:
			continue
		case datatype.BOOLEAN:
			val, err := strconv.ParseBool(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = data.Bool(val)
		case datatype.TINYINT:
			val, err := strconv.Atoi(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = data.TinyInt(val)
		case datatype.SMALLINT:
			val, err := strconv.Atoi(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = data.SmallInt(val)
		case datatype.INT:
			val, err := strconv.Atoi(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = data.Int(val)
		case datatype.BIGINT:
			val, err := strconv.ParseInt(*value, 10, 64)
			if err != nil {
				return nil, err
			}
			record[idx] = data.BigInt(val)
		case datatype.FLOAT:
			fVal, err := strconv.ParseFloat(*value, 32)
			if err != nil {
				return nil, err
			}
			record[idx] = data.Float(fVal)
		case datatype.DOUBLE:
			dVal, err := strconv.ParseFloat(*value, 32)
			if err != nil {
				return nil, err
			}
			record[idx] = data.Double(dVal)
		case datatype.DATE:
			t, err := data.NewDate(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = t
		case datatype.DATETIME:
			t, err := data.NewDateTime(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = t
		case datatype.TIMESTAMP:
			tVal, err := data.NewTimestamp(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = tVal
		case datatype.TIMESTAMP_NTZ:
			tVal, err := data.NewTimestampNtz(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = tVal
		case datatype.STRING:
			record[idx] = data.String(*value)
		case datatype.CHAR:
			val, err := data.NewChar(255, *value)
			if err != nil {
				return nil, err
			}
			record[idx] = val
		case datatype.VARCHAR:
			val, err := data.NewVarChar(65536, *value)
			if err != nil {
				return nil, err
			}
			record[idx] = val
		case datatype.BINARY:
			record[idx] = data.Binary(*value)
		case datatype.JSON:
			val, err := data.NewJson(*value)
			if err != nil {
				return nil, err
			}
			record[idx] = val
		case datatype.VOID, datatype.DECIMAL, datatype.IntervalDayTime, datatype.IntervalYearMonth,
			datatype.ARRAY, datatype.MAP, datatype.STRUCT, datatype.TypeUnknown:
			return nil, fmt.Errorf("unsported type %v, column:%v", column.Type.Name(), column.Name)
		}
	}

	return record, nil
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
