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

//go:build linux || windows
// +build linux windows

package sls

import (
	"fmt"

	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// Log event and group field numbers from protobuf_public/models.
const (
	groupLogsField  = 3
	logEventField   = 1
	timestampField  = 1
	contentField    = 2
	levelField      = 3
	offsetField     = 4
	rawSizeField    = 5
	contentKeyField = 1
	contentValField = 2
)

func wireBytesSize(field protowire.Number, length int) int {
	return protowire.SizeTag(field) + protowire.SizeBytes(length)
}

func optionalBytesSize(field protowire.Number, length int) int {
	if length == 0 {
		return 0
	}
	return wireBytesSize(field, length)
}

func optionalUint64Size(field protowire.Number, value uint64) int {
	if value == 0 {
		return 0
	}
	return protowire.SizeTag(field) + protowire.SizeVarint(value)
}

func appendWireLength(out []byte, field protowire.Number, length int) []byte {
	out = protowire.AppendTag(out, field, protowire.BytesType)
	return protowire.AppendVarint(out, uint64(length))
}

func appendOptionalString(out []byte, field protowire.Number, text string) []byte {
	if text == "" {
		return out
	}
	out = protowire.AppendTag(out, field, protowire.BytesType)
	return protowire.AppendString(out, text)
}

func appendOptionalUint64(out []byte, field protowire.Number, value uint64) []byte {
	if value == 0 {
		return out
	}
	out = protowire.AppendTag(out, field, protowire.VarintType)
	return protowire.AppendVarint(out, value)
}

func logWireSize(log *models.Log) (int, error) {
	size := optionalUint64Size(timestampField, log.Timestamp) + optionalBytesSize(levelField, len(log.Level)) +
		optionalUint64Size(offsetField, log.Offset) + optionalUint64Size(rawSizeField, log.RawSize)
	for key, field := range models.LogContentEntries(log.GetIndices()) {
		var length int
		switch value := field.(type) {
		case string:
			length = len(value)
		case []byte:
			length = len(value)
		default:
			return 0, fmt.Errorf("unsupported log content type for key %s: %T", key, field)
		}
		content := optionalBytesSize(contentKeyField, len(key)) + optionalBytesSize(contentValField, length)
		size += wireBytesSize(contentField, content)
	}
	return size, nil
}

func appendLogWire(out []byte, log *models.Log) []byte {
	out = appendOptionalUint64(out, timestampField, log.Timestamp)
	for key, field := range models.LogContentEntries(log.GetIndices()) {
		keySize := optionalBytesSize(contentKeyField, len(key))
		switch value := field.(type) {
		case string:
			out = appendWireLength(out, contentField, keySize+optionalBytesSize(contentValField, len(value)))
			out = appendOptionalString(out, contentKeyField, key)
			out = appendOptionalString(out, contentValField, value)
		case []byte:
			out = appendWireLength(out, contentField, keySize+optionalBytesSize(contentValField, len(value)))
			out = appendOptionalString(out, contentKeyField, key)
			if len(value) > 0 {
				out = protowire.AppendTag(out, contentValField, protowire.BytesType)
				out = protowire.AppendBytes(out, value)
			}
		}
	}
	out = appendOptionalString(out, levelField, log.Level)
	out = appendOptionalUint64(out, offsetField, log.Offset)
	return appendOptionalUint64(out, rawSizeField, log.RawSize)
}

// marshalLogGroup sizes the log messages before writing them into the final
// PEG buffer. Group attributes still use the generated marshaler.
func marshalLogGroup(group *protocol.PipelineEventGroup, events []models.PipelineEvent) ([]byte, int, error) {
	sizes := make([]int, len(events))
	logsSize, count := 0, 0
	for i, event := range events {
		log, ok := event.(*models.Log)
		if !ok || log == nil {
			return nil, 0, fmt.Errorf("invalid log event %T", event)
		}
		// SLS requires at least one content field in each wire log.
		if log.GetIndices().Len() == 0 {
			continue
		}
		size, err := logWireSize(log)
		if err != nil {
			return nil, 0, fmt.Errorf("convert log event: %w", err)
		}
		sizes[i] = size + 1
		logsSize += wireBytesSize(logEventField, size)
		count++
	}
	if count == 0 {
		return nil, 0, nil
	}

	headerSize := group.Size()
	wire := make([]byte, headerSize, headerSize+wireBytesSize(groupLogsField, logsSize))
	if _, err := group.MarshalToSizedBuffer(wire); err != nil {
		return nil, 0, fmt.Errorf("marshal pipeline event group: %w", err)
	}
	wire = appendWireLength(wire, groupLogsField, logsSize)
	for i, size := range sizes {
		if size == 0 {
			continue
		}
		wire = appendWireLength(wire, logEventField, size-1)
		wire = appendLogWire(wire, events[i].(*models.Log))
	}
	return wire, count, nil
}
