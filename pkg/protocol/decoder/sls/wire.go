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

package sls

import (
	"fmt"
	"time"

	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
)

func decodeLog(data []byte) (*models.Log, error) {
	const (
		timeField    = 1
		contentField = 2
		valuesField  = 3
		nanosField   = 4
	)
	log := models.NewLogWithOrderedContents(4)
	var seconds, nanos uint32
	var hasTime bool
	for len(data) > 0 {
		field, kind, consumed := protowire.ConsumeTag(data)
		if consumed < 0 {
			return nil, protowire.ParseError(consumed)
		}
		if !field.IsValid() {
			return nil, fmt.Errorf("invalid SLS log field number %d", field)
		}
		data = data[consumed:]
		switch field {
		case timeField:
			if kind != protowire.VarintType {
				return nil, fmt.Errorf("invalid wire type %d for SLS log time", kind)
			}
			var value uint64
			value, consumed = protowire.ConsumeVarint(data)
			seconds = uint32(value)
			hasTime = true
		case contentField, valuesField:
			if kind != protowire.BytesType {
				return nil, fmt.Errorf("invalid wire type %d for SLS log field %d", kind, field)
			}
			var value []byte
			value, consumed = protowire.ConsumeBytes(data)
			if consumed >= 0 && field == contentField {
				key, value, err := decodeKeyValue(value)
				if err != nil {
					return nil, fmt.Errorf("read SLS log content: %w", err)
				}
				log.Contents.Add(key, value)
			}
		case nanosField:
			if kind != protowire.Fixed32Type {
				return nil, fmt.Errorf("invalid wire type %d for SLS log time_ns", kind)
			}
			nanos, consumed = protowire.ConsumeFixed32(data)
		default:
			consumed = protowire.ConsumeFieldValue(field, kind, data)
		}
		if consumed < 0 {
			return nil, fmt.Errorf("read SLS log field %d: %w", field, protowire.ParseError(consumed))
		}
		data = data[consumed:]
	}
	if !hasTime {
		return nil, fmt.Errorf("missing required SLS log time")
	}
	log.Timestamp = uint64(seconds)*uint64(time.Second) + uint64(nanos)
	return log, nil
}

// Content and LogTag use the same proto2 wire layout with two required strings.
func decodeKeyValue(data []byte) (string, string, error) {
	const (
		keyField   = 1
		valueField = 2
	)
	var key, value []byte
	for len(data) > 0 {
		field, kind, consumed := protowire.ConsumeTag(data)
		if consumed < 0 {
			return "", "", protowire.ParseError(consumed)
		}
		if !field.IsValid() {
			return "", "", fmt.Errorf("invalid SLS key/value field number %d", field)
		}
		data = data[consumed:]
		switch field {
		case keyField, valueField:
			if kind != protowire.BytesType {
				return "", "", fmt.Errorf("invalid wire type %d for SLS key/value field %d", kind, field)
			}
			if field == keyField {
				key, consumed = protowire.ConsumeBytes(data)
			} else {
				value, consumed = protowire.ConsumeBytes(data)
			}
		default:
			consumed = protowire.ConsumeFieldValue(field, kind, data)
		}
		if consumed < 0 {
			return "", "", fmt.Errorf("read SLS key/value field %d: %w", field, protowire.ParseError(consumed))
		}
		data = data[consumed:]
	}
	// ConsumeBytes returns a non-nil slice for a present empty string.
	if key == nil || value == nil {
		return "", "", fmt.Errorf("missing required SLS key or value")
	}
	return string(key), string(value), nil
}
