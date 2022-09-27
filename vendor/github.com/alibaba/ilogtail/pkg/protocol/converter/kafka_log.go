package protocol

import (
	"encoding/json"
	"fmt"
	"strings"

	"github.com/alibaba/ilogtail/pkg/fmtstr"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func (c *Converter) ConvertToKafkaSingleLog(logGroup *protocol.LogGroup, log *protocol.Log, topic string) ([]byte, *string, error) {
	var marshaledLogs []byte
	var targets *string
	contents, tags := convertLogToMap(log, logGroup.LogTags, logGroup.Source, logGroup.Topic, c.TagKeyRenameMap)
	target, err := formatTopic(contents, tags, c.TagKeyRenameMap, topic)
	if err != nil {
		return nil, nil, err
	}
	targets = target
	customSingleLog := make(map[string]interface{}, numProtocolKeys)
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
		customSingleLog[newKey] = log.Time
	} else {
		customSingleLog[protocolKeyTime] = log.Time
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyContent]; ok {
		customSingleLog[newKey] = contents
	} else {
		customSingleLog[protocolKeyContent] = contents
	}
	if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTag]; ok {
		customSingleLog[newKey] = tags
	} else {
		customSingleLog[protocolKeyTag] = tags
	}
	switch c.Encoding {
	case EncodingJSON:
		b, err := json.Marshal(customSingleLog)
		if err != nil {
			return nil, nil, fmt.Errorf("unable to marshal log: %v", customSingleLog)
		}
		marshaledLogs = b
	default:
		return nil, nil, fmt.Errorf("unsupported encoding format: %s", c.Encoding)
	}

	return marshaledLogs, targets, nil
}

// formatTopic return topic dynamically by using a format string
func formatTopic(contents, tags, tagKeyRenameMap map[string]string, topicPattern string) (*string, error) {
	sf, err := fmtstr.Compile(topicPattern, func(field string, ops []fmtstr.VariableOp) (fmtstr.FormatEvaler, error) {
		switch {
		case strings.HasPrefix(field, targetContentPrefix):
			if value, ok := contents[field[len(targetContentPrefix):]]; ok {
				return fmtstr.StringElement{S: value}, nil
			}
		case strings.HasPrefix(field, targetTagPrefix):
			if value, ok := tags[field[len(targetTagPrefix):]]; ok {
				return fmtstr.StringElement{S: value}, nil
			} else if value, ok := tagKeyRenameMap[field[len(targetTagPrefix):]]; ok {
				return fmtstr.StringElement{S: tags[value]}, nil
			}
		default:
			return fmtstr.StringElement{S: field}, nil
		}
		return fmtstr.StringElement{S: field}, nil
	})
	if err != nil {
		return nil, err
	}
	topic, err := sf.Run(nil)
	if err != nil {
		return nil, err
	}
	return &topic, nil
}
