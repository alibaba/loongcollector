// Copyright 2022 iLogtail Authors
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
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func (c *Converter) ConvertToSingleLogs(logGroup *protocol.LogGroup, targetFields []string) ([][]byte, [][]string, error) {
	marshaledLogs, desiredValues := make([][]byte, len(logGroup.Logs)), make([][]string, len(logGroup.Logs))
	for i, log := range logGroup.Logs {
		contents, tags := convertLogToMap(log, logGroup.LogTags, logGroup.Source, logGroup.Topic, c.TagKeyRenameMap)

		desiredValue, err := findTargetValues(targetFields, contents, tags, c.TagKeyRenameMap)
		if err != nil {
			return nil, nil, err
		}
		desiredValues[i] = desiredValue

		singleLog := make(map[string]interface{}, 3)
		if newKey, ok := c.ProtocolKeyRenameMap["time"]; ok {
			singleLog[newKey] = log.Time
		} else {
			singleLog["time"] = log.Time
		}
		if newKey, ok := c.ProtocolKeyRenameMap["contents"]; ok {
			singleLog[newKey] = contents
		} else {
			singleLog["contents"] = contents
		}
		if newKey, ok := c.ProtocolKeyRenameMap["tags"]; ok {
			singleLog[newKey] = tags
		} else {
			singleLog["tags"] = tags
		}

		switch c.Encoding {
		case encodingJSON:
			b, err := json.Marshal(singleLog)
			if err != nil {
				return nil, nil, fmt.Errorf("unable to marshal log: %v", singleLog)
			}
			marshaledLogs[i] = b
		default:
			return nil, nil, fmt.Errorf("unsupported encoding format: %s", c.Encoding)
		}
	}
	return marshaledLogs, desiredValues, nil
}
