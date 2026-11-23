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
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func TestDecodeGroupWire(t *testing.T) {
	nanos := uint32(123)
	log, err := (&protocol.Log{
		Time: 7, TimeNs: &nanos,
		Contents: []*protocol.Log_Content{{Key: "z", Value: "first"}, {Key: "a", Value: "last"}},
	}).Marshal()
	require.NoError(t, err)
	second, err := (&protocol.Log{Time: 8}).Marshal()
	require.NoError(t, err)
	topicTag, err := (&protocol.LogTag{Key: pipeline.TopicTagKey, Value: "tag-topic"}).Marshal()
	require.NoError(t, err)
	machineTag, err := (&protocol.LogTag{Key: "__machine_uuid__", Value: "tag-machine"}).Marshal()
	require.NoError(t, err)

	data := appendGroupField(nil, 1, log)
	data = appendGroupField(data, 2, []byte("previous-category"))
	data = appendGroupField(data, 2, []byte("original-logstore"))
	data = appendGroupField(data, 3, []byte("topic"))
	data = appendGroupField(data, 4, []byte("removed-source"))
	data = appendGroupField(data, 4, nil)
	data = appendGroupField(data, 5, []byte("machine"))
	data = appendGroupField(data, 6, topicTag)
	data = appendGroupField(data, 6, machineTag)
	data = protowire.AppendVarint(protowire.AppendTag(data, 7, protowire.VarintType), 20)
	data = appendGroupField(data, 8, []byte("unknown"))
	data = protowire.AppendTag(data, 9, protowire.StartGroupType)
	data = protowire.AppendFixed32(protowire.AppendTag(data, 10, protowire.Fixed32Type), 99)
	data = protowire.AppendTag(data, 9, protowire.EndGroupType)
	data = appendGroupField(data, 1, second)

	groups, err := (&Decoder{}).Decode(data, nil)
	require.NoError(t, err)
	require.Len(t, groups, 1)
	result := groups[0]
	require.Len(t, result.Events, 2)
	require.Equal(t, map[string]string{"sls.category": "original-logstore"}, result.Group.GetMetadata().Iterator())
	require.Empty(t, result.Group.GetDestinationLogstore())
	require.Equal(t, map[string]string{pipeline.TopicTagKey: "topic", "__machine_uuid__": "machine"}, result.Group.GetTags().Iterator())

	first := result.Events[0].(*models.Log)
	require.Equal(t, uint64(7000000123), first.GetTimestamp())
	require.Equal(t, map[string]any{"z": "first", "a": "last"}, first.GetIndices().Iterator())
	firstKey, ok := models.FirstLogContentKey(first.GetIndices())
	require.True(t, ok)
	require.Equal(t, "z", firstKey)
	require.Equal(t, uint64(8000000000), result.Events[1].GetTimestamp())
}

func TestDecodeMalformedGroupWire(t *testing.T) {
	for _, tc := range []struct {
		name string
		data []byte
	}{
		{"truncated tag", []byte{0x80}},
		{"field zero", []byte{0, 0}},
		{"wrong log type", protowire.AppendVarint(protowire.AppendTag(nil, 1, protowire.VarintType), 7)},
		{"truncated category", []byte{0x12, 0x02, 'x'}},
		{"log missing time", appendGroupField(nil, 1, nil)},
		{"tag missing value", appendGroupField(nil, 6, appendGroupField(nil, 1, []byte("key")))},
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
	groups, err := (&Decoder{}).Decode(appendGroupField(nil, 2, []byte("original-logstore")), nil)
	require.NoError(t, err)
	require.Empty(t, groups)
}

func appendGroupField(data []byte, field protowire.Number, value []byte) []byte {
	return protowire.AppendBytes(protowire.AppendTag(data, field, protowire.BytesType), value)
}
