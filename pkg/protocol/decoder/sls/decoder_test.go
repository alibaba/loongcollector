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

package sls

import (
	"testing"

	"github.com/stretchr/testify/require"
	"google.golang.org/protobuf/encoding/protowire"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

func TestDecodeGroupWire(t *testing.T) {
	log := appendVarintField(nil, 1, 9)
	log = appendBytesField(log, 2, keyValueWire("z", "original"))
	log = appendFixed32Field(log, 4, 456)
	log = appendBytesField(log, 2, keyValueWire("a", "last"))
	log = appendBytesField(log, 2, keyValueWire("z", "first"))
	log = appendBytesField(log, 2, keyValueWire("", ""))
	log = appendBytesField(log, 3, []byte("legacy value"))
	log = appendVarintField(log, 1, 7)
	log = appendFixed32Field(log, 4, 123)
	log = appendVarintField(log, 5, 99)
	second := appendVarintField(nil, 1, 8)
	epoch := appendVarintField(nil, 1, 0)
	topicTag := keyValueWire(pipeline.TopicTagKey, "tag-topic")
	machineTag := keyValueWire("__machine_uuid__", "tag-machine")

	data := appendBytesField(nil, 1, log)
	data = appendBytesField(data, 2, []byte("previous-category"))
	data = appendBytesField(data, 2, []byte("original-logstore"))
	data = appendBytesField(data, 3, []byte("topic"))
	data = appendBytesField(data, 4, []byte("removed-source"))
	data = appendBytesField(data, 4, nil)
	data = appendBytesField(data, 5, []byte("machine"))
	data = appendBytesField(data, 6, topicTag)
	data = appendBytesField(data, 6, machineTag)
	data = appendBytesField(data, 6, keyValueWire("node", "overridden"))
	nodeTag := appendBytesField(keyValueWire("unused", "unused"), 1, []byte("node"))
	nodeTag = appendBytesField(nodeTag, 2, []byte("edge"))
	data = appendBytesField(data, 6, nodeTag)
	data = appendBytesField(data, 6, keyValueWire("empty", ""))
	data = appendVarintField(data, 7, 20)
	data = appendBytesField(data, 8, []byte("unknown"))
	data = protowire.AppendTag(data, 9, protowire.StartGroupType)
	data = appendFixed32Field(data, 10, 99)
	data = protowire.AppendTag(data, 9, protowire.EndGroupType)
	data = appendBytesField(data, 1, second)
	data = appendBytesField(data, 1, epoch)

	groups, err := (&Decoder{}).Decode(data, nil)
	require.NoError(t, err)
	require.Len(t, groups, 1)
	result := groups[0]
	require.Len(t, result.Events, 3)
	require.Equal(t, map[string]string{"sls.category": "original-logstore"}, result.Group.GetMetadata().Iterator())
	require.Empty(t, result.Group.GetDestinationLogstore())
	require.Equal(t, map[string]string{pipeline.TopicTagKey: "topic", "__machine_uuid__": "machine", "node": "edge", "empty": ""}, result.Group.GetTags().Iterator())

	first := result.Events[0].(*models.Log)
	require.Equal(t, uint64(7000000123), first.GetTimestamp())
	require.Equal(t, map[string]any{"z": "first", "a": "last", "": ""}, first.GetIndices().Iterator())
	var fieldOrder []string
	for key := range models.LogContentEntries(first.GetIndices()) {
		fieldOrder = append(fieldOrder, key)
	}
	require.Equal(t, []string{"z", "a", ""}, fieldOrder)
	require.Equal(t, uint64(8000000000), result.Events[1].GetTimestamp())
	require.Equal(t, uint64(0), result.Events[2].GetTimestamp())
}

func TestDecodeMalformedGroupWire(t *testing.T) {
	logWithContent := func(content []byte) []byte {
		log := appendBytesField(appendVarintField(nil, 1, 0), 2, content)
		return appendBytesField(nil, 1, log)
	}
	for _, tc := range []struct {
		name string
		data []byte
	}{
		{"truncated tag", []byte{0x80}},
		{"field zero", []byte{0, 0}},
		{"wrong log type", appendVarintField(nil, 1, 7)},
		{"truncated category", []byte{0x12, 0x02, 'x'}},
		{"log missing time", appendBytesField(nil, 1, nil)},
		{"log wrong time type", appendBytesField(nil, 1, appendBytesField(nil, 1, nil))},
		{"log missing content key", logWithContent(appendBytesField(nil, 2, nil))},
		{"log missing content value", logWithContent(appendBytesField(nil, 1, nil))},
		{"log wrong legacy value type", appendBytesField(nil, 1, appendVarintField(appendVarintField(nil, 1, 0), 3, 0))},
		{"log truncated nanos", appendBytesField(nil, 1, protowire.AppendTag(appendVarintField(nil, 1, 0), 4, protowire.Fixed32Type))},
		{"tag missing key", appendBytesField(nil, 6, appendBytesField(nil, 2, nil))},
		{"tag missing value", appendBytesField(nil, 6, appendBytesField(nil, 1, []byte("key")))},
		{"unexpected end group", protowire.AppendTag(nil, 7, protowire.EndGroupType)},
		{"truncated unknown field", []byte{0x3a, 0x02, 'x'}},
	} {
		t.Run(tc.name, func(t *testing.T) {
			groups, err := (&Decoder{}).Decode(tc.data, nil)
			require.Error(t, err)
			require.Empty(t, groups)
		})
	}
}

func TestDecodeEmptyGroupWire(t *testing.T) {
	groups, err := (&Decoder{}).Decode(appendBytesField(nil, 2, []byte("original-logstore")), nil)
	require.NoError(t, err)
	require.Empty(t, groups)
}

func appendBytesField(data []byte, field protowire.Number, value []byte) []byte {
	return protowire.AppendBytes(protowire.AppendTag(data, field, protowire.BytesType), value)
}

func appendVarintField(data []byte, field protowire.Number, value uint64) []byte {
	return protowire.AppendVarint(protowire.AppendTag(data, field, protowire.VarintType), value)
}

func appendFixed32Field(data []byte, field protowire.Number, value uint32) []byte {
	return protowire.AppendFixed32(protowire.AppendTag(data, field, protowire.Fixed32Type), value)
}

func keyValueWire(key, value string) []byte {
	return appendBytesField(appendBytesField(nil, 1, []byte(key)), 2, []byte(value))
}
