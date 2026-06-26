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

package flusherparity

import (
	"encoding/json"
	"sort"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	converter "github.com/alibaba/ilogtail/pkg/protocol/converter"
)

const (
	FixedLogTime     = uint32(1700000000)
	FixedTimestampNs = uint64(FixedLogTime) * 1_000_000_000
)

// BuildCustomSingleParityFixtures returns semantically equivalent v1 LogGroups and v2 PipelineGroupEvents
// for custom_single JSON conversion parity tests.
func BuildCustomSingleParityFixtures() ([]*protocol.LogGroup, []*models.PipelineGroupEvents) {
	v1Log := &protocol.Log{
		Time: FixedLogTime,
		Contents: []*protocol.Log_Content{
			{Key: "content", Value: "export parity test"},
		},
	}
	v2Log := models.NewLog("", []byte("export parity test"), "", "", "", models.NewTags(), FixedTimestampNs)

	v1Groups := []*protocol.LogGroup{{
		Logs:   []*protocol.Log{v1Log},
		Source: "",
	}}

	groupTags := models.NewTagsWithKeyValues("host.ip", "")
	v2Groups := []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), groupTags),
		Events: []models.PipelineEvent{v2Log},
	}}

	return v1Groups, v2Groups
}

// NewCustomSingleConverter creates a converter used by extended flushers in parity tests.
func NewCustomSingleConverter() (*converter.Converter, error) {
	return converter.NewConverter(
		converter.ProtocolCustomSingle,
		converter.EncodingJSON,
		nil,
		nil,
		&config.GlobalConfig{},
	)
}

// CollectConverterByteStreams compares v1 ToByteStream vs v2 ToByteStreamV2 output for the same semantic data.
func CollectConverterByteStreams(cvt *converter.Converter, v1Groups []*protocol.LogGroup, v2Groups []*models.PipelineGroupEvents) ([][]byte, [][]byte, error) {
	var v1Out, v2Out [][]byte
	for _, lg := range v1Groups {
		stream, err := cvt.ToByteStream(lg)
		if err != nil {
			return nil, nil, err
		}
		v1Out = append(v1Out, stream.([][]byte)...)
	}
	for _, ge := range v2Groups {
		stream, err := cvt.ToByteStreamV2(ge)
		if err != nil {
			return nil, nil, err
		}
		v2Out = append(v2Out, stream.([][]byte)...)
	}
	return v1Out, v2Out, nil
}

// CollectConverterByteStreamsWithFields compares v1/v2 converter output when selected fields are used.
func CollectConverterByteStreamsWithFields(
	cvt *converter.Converter,
	v1Groups []*protocol.LogGroup,
	v2Groups []*models.PipelineGroupEvents,
	fields []string,
) ([][]byte, [][]byte, error) {
	var v1Out, v2Out [][]byte
	for _, lg := range v1Groups {
		stream, _, err := cvt.ToByteStreamWithSelectedFields(lg, fields)
		if err != nil {
			return nil, nil, err
		}
		v1Out = append(v1Out, stream.([][]byte)...)
	}
	for _, ge := range v2Groups {
		stream, _, err := cvt.ToByteStreamWithSelectedFieldsV2(ge, fields)
		if err != nil {
			return nil, nil, err
		}
		v2Out = append(v2Out, stream.([][]byte)...)
	}
	return v1Out, v2Out, nil
}

// NormalizeJSONLines sorts JSON object lines for stable comparison.
func NormalizeJSONLines(lines [][]byte) []string {
	out := make([]string, 0, len(lines))
	for _, line := range lines {
		var obj map[string]interface{}
		if err := json.Unmarshal(line, &obj); err != nil {
			out = append(out, string(line))
			continue
		}
		normalized, err := json.Marshal(obj)
		if err != nil {
			out = append(out, string(line))
			continue
		}
		out = append(out, string(normalized))
	}
	sort.Strings(out)
	return out
}

// LogsToNormalizedJSON returns normalized JSON strings for legacy logs (used by wrapper flushers).
func LogsToNormalizedJSON(logs []*protocol.Log) ([]string, error) {
	out := make([]string, 0, len(logs))
	for _, log := range logs {
		buf, err := json.Marshal(log)
		if err != nil {
			return nil, err
		}
		var obj map[string]interface{}
		if unmarshalErr := json.Unmarshal(buf, &obj); unmarshalErr != nil {
			return nil, unmarshalErr
		}
		normalized, err := json.Marshal(obj)
		if err != nil {
			return nil, err
		}
		out = append(out, string(normalized))
	}
	sort.Strings(out)
	return out, nil
}
