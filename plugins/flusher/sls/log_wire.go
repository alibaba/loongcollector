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
	"slices"

	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/util"
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

// Small wire messages generally use two bytes for a log length and three
// for a group length. Backfill the canonical prefix once the size is known.
func beginWireMessage(out []byte, field protowire.Number, reserved int) ([]byte, int) {
	out = protowire.AppendTag(out, field, protowire.BytesType)
	var padding [10]byte
	return append(out, padding[:reserved]...), len(out)
}

func finishWireMessage(out []byte, lengthAt, reserved int) []byte {
	bodyAt := lengthAt + reserved
	size := len(out) - bodyAt
	prefixSize := protowire.SizeVarint(uint64(size))
	if prefixSize != reserved {
		oldEnd := len(out)
		if prefixSize > reserved {
			var padding [10]byte
			out = append(out, padding[:prefixSize-reserved]...)
		}
		copy(out[lengthAt+prefixSize:], out[bodyAt:oldEnd])
		out = out[:lengthAt+prefixSize+size]
	}
	protowire.AppendVarint(out[:lengthAt], uint64(size))
	return out
}

func appendLogWire(out []byte, log *models.Log) ([]byte, error) {
	out = appendOptionalUint64(out, timestampField, log.Timestamp)
	for key, field := range models.LogContentEntries(log.GetIndices()) {
		var bytes []byte
		switch value := field.(type) {
		case string:
			bytes = util.ZeroCopyStringToBytes(value)
		case []byte:
			bytes = value
		default:
			return out, fmt.Errorf("unsupported log content type for key %s: %T", key, field)
		}
		size := optionalBytesSize(contentKeyField, len(key)) + optionalBytesSize(contentValField, len(bytes))
		out = appendWireLength(out, contentField, size)
		out = appendOptionalString(out, contentKeyField, key)
		if len(bytes) > 0 {
			out = protowire.AppendTag(out, contentValField, protowire.BytesType)
			out = protowire.AppendBytes(out, bytes)
		}
	}
	out = appendOptionalString(out, levelField, log.Level)
	out = appendOptionalUint64(out, offsetField, log.Offset)
	return appendOptionalUint64(out, rawSizeField, log.RawSize), nil
}

type logWireBuffer struct {
	bytes []byte
}

// At most four completed buffers can be retained across all Go SLS
// flushers. The normal batch size is up to 3 MiB plus group headers.
var logWirePool = make(chan *logWireBuffer, 4)

func acquireLogWireBuffer() *logWireBuffer {
	select {
	case buffer := <-logWirePool:
		return buffer
	default:
		return new(logWireBuffer)
	}
}

func releaseLogWireBuffer(buffer *logWireBuffer) {
	// Native serialization and compression have finished when the synchronous
	// callback returns. Avoid retaining an unusually large group in the pool.
	const maxRetainedWireBytes = 4 << 20
	if cap(buffer.bytes) > maxRetainedWireBytes {
		return
	}
	select {
	case logWirePool <- buffer:
	default:
	}
}

// marshalLogGroup writes logs in a single pass. Group attributes still use
// the generated marshaler.
func (buffer *logWireBuffer) marshalLogGroup(group *protocol.PipelineEventGroup, events []models.PipelineEvent) ([]byte, int, error) {
	size := group.Size()
	wire := slices.Grow(buffer.bytes[:0], size)[:size]
	if _, err := group.MarshalToSizedBuffer(wire); err != nil {
		return nil, 0, fmt.Errorf("marshal pipeline event group: %w", err)
	}

	const groupLengthBytes, logLengthBytes = 3, 2
	wire, logsLengthAt := beginWireMessage(wire, groupLogsField, groupLengthBytes)
	count := 0
	for _, event := range events {
		log, ok := event.(*models.Log)
		if !ok || log == nil {
			return nil, 0, fmt.Errorf("invalid log event %T", event)
		}
		// SLS requires at least one content field in each wire log.
		if log.GetIndices().Len() == 0 {
			continue
		}
		var lengthAt int
		wire, lengthAt = beginWireMessage(wire, logEventField, logLengthBytes)
		var err error
		wire, err = appendLogWire(wire, log)
		if err != nil {
			return nil, 0, fmt.Errorf("convert log event: %w", err)
		}
		wire = finishWireMessage(wire, lengthAt, logLengthBytes)
		count++
	}
	buffer.bytes = finishWireMessage(wire, logsLengthAt, groupLengthBytes)
	if count == 0 {
		return nil, 0, nil
	}
	return buffer.bytes, count, nil
}
