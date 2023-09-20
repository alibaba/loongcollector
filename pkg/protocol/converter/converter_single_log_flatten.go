// Copyright 2023 iLogtail Authors
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

package protocol

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func (c *Converter) ConvertToSingleProtocolLogsFlatten(logGroup *protocol.LogGroup, targetFields []string) ([]map[string]interface{}, []map[string]string, error) {
	convertedLogs, desiredValues := make([]map[string]interface{}, len(logGroup.Logs)), make([]map[string]string, len(logGroup.Logs))
	for i, log := range logGroup.Logs {
		contents, tags := convertLogToMap(log, logGroup.LogTags, logGroup.Source, logGroup.Topic, c.TagKeyRenameMap)

		desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
		if err != nil {
			return nil, nil, err
		}
		desiredValues[i] = desiredValue

		logLength := len(contents) + len(tags) + 1
		customSingleLog := make(map[string]interface{}, logLength)
		// merge contents to final logs
		for k, v := range contents {
			customSingleLog[k] = v
		}
		// merge tags to final logs
		for k, v := range tags {
			customSingleLog[k] = v
		}

		if newKey, ok := c.ProtocolKeyRenameMap[protocolKeyTime]; ok {
			customSingleLog[newKey] = log.Time
		} else {
			customSingleLog[protocolKeyTime] = log.Time
		}

		convertedLogs[i] = customSingleLog
	}
	return convertedLogs, desiredValues, nil
}

func (c *Converter) ConvertToSingleProtocolStreamFlatten(logGroup *protocol.LogGroup, targetFields []string) ([][]byte, []map[string]string, error) {
	convertedLogs, desiredValues, err := c.ConvertToSingleProtocolLogsFlatten(logGroup, targetFields)
	if err != nil {
		return nil, nil, err
	}
	marshaledLogs := make([][]byte, len(logGroup.Logs))
	for i, log := range convertedLogs {
		switch c.Encoding {
		case EncodingJSON:
			b, err := marshalWithoutHTMLEscaped(log)
			if err != nil {
				return nil, nil, fmt.Errorf("unable to marshal log: %v", log)
			}
			marshaledLogs[i] = b
		default:
			return nil, nil, fmt.Errorf("unsupported encoding format: %s", c.Encoding)
		}
	}
	return marshaledLogs, desiredValues, nil
}
