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

package fmtstr

import "time"

// FormatIndex name Elasticsearch index base on time like Logstash and Beats
func FormatIndex(targetValues map[string]string, indexPattern string, indexTimestamp uint32) (*string, error) {
	sf, err := Compile(indexPattern, func(key string, ops []VariableOp) (FormatEvaler, error) {
		// with timestamp expression, like %{+yyyyMM}
		if key[0] == '+' {
			indexTime := time.Unix(int64(indexTimestamp), 0)
			index := FormatTimestamp(&indexTime, key[1:])
			return StringElement{S: index}, nil
		}
		if value, ok := targetValues[key]; ok {
			return StringElement{S: value}, nil
		}
		return StringElement{S: key}, nil
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
