// Copyright 2026 iLogtail Authors
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

package helper

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"
)

// NewInputLog creates an event for a row of string fields and independent tags.
func NewInputLog(tags, fields map[string]string, timestamps ...time.Time) *models.Log {
	log := newInputLog(tags, models.NewLogContentsWithCapacity(len(fields)), timestamps)
	for key, value := range fields {
		setInputLogField(log, key, value)
	}
	return log
}

// NewInputLogRow keeps the input column order for processors and routers.
func NewInputLogRow(tags map[string]string, keys, values []string, timestamps ...time.Time) (*models.Log, error) {
	if len(keys) != len(values) {
		return nil, fmt.Errorf("mismatched log row: %d columns and %d values", len(keys), len(values))
	}
	log := newInputLog(tags, models.NewOrderedLogContents(len(keys)), timestamps)
	for i, key := range keys {
		setInputLogField(log, key, values[i])
	}
	return log, nil
}

func newInputLog(tags map[string]string, contents models.LogContents, timestamps []time.Time) *models.Log {
	var timestamp time.Time
	if len(timestamps) == 0 {
		timestamp = time.Now()
	} else {
		timestamp = timestamps[0]
	}
	target := models.NewTags()
	target.AddAll(tags)
	return &models.Log{Contents: contents, Tags: target, Timestamp: uint64(timestamp.UnixNano())}
}

func setInputLogField(log *models.Log, key, value string) {
	if key == models.BodyKey {
		log.SetBody([]byte(value))
		return
	}
	log.Contents.Add(key, value)
}
