package protocol

import (
	"fmt"

	"github.com/gogo/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/config"
)

func CloneLog(log *Log) *Log {
	cloneLog := &Log{
		Time:     log.Time,
		TimeNs:   log.TimeNs,
		Contents: make([]*Log_Content, len(log.Contents), cap(log.Contents)),
	}
	for i, content := range log.Contents {
		cloneLog.Contents[i] = &Log_Content{
			Key:   content.Key,
			Value: content.Value,
		}
	}
	return cloneLog
}

func SetLogTime(log *Log, second uint32, nanosecond uint32) {
	log.Time = second
	if config.LogtailGlobalConfig.EnableTimestampNanosecond {
		log.TimeNs = &nanosecond
	} else {
		log.TimeNs = nil
	}
}

type Codec struct{}

func (Codec) Marshal(v interface{}) ([]byte, error) {
	vv, ok := v.(proto.Message)
	if !ok {
		return nil, fmt.Errorf("failed to marshal, message is %T, want proto.Message", v)
	}
	return proto.Marshal(vv)
}

func (Codec) Unmarshal(data []byte, v interface{}) error {
	vv, ok := v.(proto.Message)
	if !ok {
		return fmt.Errorf("failed to unmarshal, message is %T, want proto.Message", v)
	}
	return proto.Unmarshal(data, vv)
}

func (Codec) Name() string {
	return "proto"
}
