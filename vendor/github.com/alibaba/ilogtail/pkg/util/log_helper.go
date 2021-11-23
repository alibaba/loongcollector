// Copyright 2021 iLogtail Authors
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

package util

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func CreateLog(t time.Time, configTag map[string]string, logTags map[string]string, fields map[string]string) (*protocol.Log, error) {
	var slsLog protocol.Log
	slsLog.Contents = make([]*protocol.Log_Content, 0, len(configTag)+len(logTags)+len(fields))
	for key, val := range configTag {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range logTags {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range fields {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	slsLog.Time = uint32(t.Unix())
	return &slsLog, nil
}

func CreateLogByArray(t time.Time, configTag map[string]string, logTags map[string]string, columns []string, values []string) (*protocol.Log, error) {
	var slsLog protocol.Log
	slsLog.Contents = make([]*protocol.Log_Content, 0, len(configTag)+len(logTags)+len(columns))

	for key, val := range configTag {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	for key, val := range logTags {
		cont := &protocol.Log_Content{
			Key:   key,
			Value: val,
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	if len(columns) != len(values) {
		return nil, fmt.Errorf("columns and values not equal")
	}

	for index := range columns {
		cont := &protocol.Log_Content{
			Key:   columns[index],
			Value: values[index],
		}
		slsLog.Contents = append(slsLog.Contents, cont)
	}

	slsLog.Time = uint32(t.Unix())
	return &slsLog, nil
}
