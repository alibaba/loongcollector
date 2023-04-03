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

package pprof

import (
	"context"
	"os"
	"strconv"
	"testing"
	"time"

	"github.com/pyroscope-io/pyroscope/pkg/convert/pprof"
	"github.com/pyroscope-io/pyroscope/pkg/storage/tree"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

func readPprofFixture(path string) (*tree.Profile, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer func() {
		_ = f.Close()
	}()

	var p tree.Profile
	if err = pprof.Decode(f, &p); err != nil {
		return nil, err
	}
	return &p, nil
}

const (
	name        = "runtime.kevent /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/sys_darwin.go"
	stack       = "runtime.netpoll /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/netpoll_kqueue.go\nruntime.findrunnable /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/proc.go\nruntime.schedule /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/proc.go\nruntime.park_m /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/proc.go\nruntime.mcall /opt/homebrew/Cellar/go/1.16.1/libexec/src/runtime/asm_arm64.s"
	stackID     = "40fb694aa9506d0b"
	startTime   = 1619321948265140000
	endTime     = 1619321949365317167
	lanuage     = "go"
	profileType = "profile_cpu"
	dataType    = "CallStack"
	val         = 250000000
	valType     = "cpu"
	unitType    = "nanoseconds"
	aggType     = "sum"
)

var (
	tags = map[string]string{
		"_app_name_": "12",
	}
)

func TestRawProfile_Parse(t *testing.T) {
	te, err := readPprofFixture("testdata/cpu.pb.gz")
	require.NoError(t, err)
	p := Parser{
		stackFrameFormatter: Formatter{},
		sampleTypesFilter:   filterKnownSamples(DefaultSampleTypeMapping),
		sampleTypes:         DefaultSampleTypeMapping,
	}
	r := new(RawProfile)
	meta := &profile.Meta{
		Tags:            tags,
		SpyName:         "go",
		StartTime:       time.Now(),
		EndTime:         time.Now(),
		SampleRate:      99,
		Units:           profile.NanosecondsUnit,
		AggregationType: profile.SumAggType,
	}
	cb := r.extractProfileV1(meta, map[string]string{"cluster": "cluster2"})
	r.parser = &p
	err = r.extractLogs(context.Background(), te, meta, cb)
	require.NoError(t, err)
	logs := r.logs
	require.Equal(t, len(logs), 6)
	picks := PickLogs(logs, "stackID", stackID)
	require.Equal(t, len(picks), 1)
	log := picks[0]
	require.Equal(t, ReadLogVal(log, "name"), name)
	require.Equal(t, ReadLogVal(log, "stack"), stack)
	require.Equal(t, ReadLogVal(log, "language"), lanuage)
	require.Equal(t, ReadLogVal(log, "type"), profileType)
	require.Equal(t, ReadLogVal(log, "units"), unitType)
	require.Equal(t, ReadLogVal(log, "valueTypes"), valType)
	require.Equal(t, ReadLogVal(log, "aggTypes"), aggType)
	require.Equal(t, ReadLogVal(log, "dataType"), dataType)
	require.Equal(t, ReadLogVal(log, "durationNs"), strconv.Itoa(endTime-startTime))
	require.Equal(t, ReadLogVal(log, "labels"), "{\"_app_name_\":\"12\",\"cluster\":\"cluster2\"}")
	require.Equal(t, ReadLogVal(log, "val"), "250000000.00")
}

// ReadLogVal returns the log content value for the input key, and returns empty string when not found.
func ReadLogVal(log *protocol.Log, key string) string {
	for _, content := range log.Contents {
		if content.Key == key {
			return content.Value
		}
	}
	return ""
}

// PickLogs select some of original logs to new res logs by the specific pickKey and pickVal.
func PickLogs(logs []*protocol.Log, pickKey string, pickVal string) (res []*protocol.Log) {
	for _, log := range logs {
		if ReadLogVal(log, pickKey) == pickVal {
			res = append(res, log)
		}
	}
	return res
}
