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
	"github.com/alibaba/ilogtail/pkg/models"
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
	var events []models.PipelineEvent
	cb := r.profileLogCallback(meta, map[string]string{"cluster": "cluster2"}, &events)
	r.parser = &p
	err = r.extractLogs(context.Background(), te, meta, cb)
	require.NoError(t, err)
	require.Len(t, events, 6)
	var picks []*models.Log
	for _, event := range events {
		log := event.(*models.Log)
		if log.Contents.Get("stackID") == stackID {
			picks = append(picks, log)
		}
	}
	require.Len(t, picks, 1)
	log := picks[0]
	require.Equal(t, uint64(startTime), log.GetTimestamp())
	require.Equal(t, log.Contents.Get("name"), name)
	require.Equal(t, log.Contents.Get("stack"), stack)
	require.Equal(t, log.Contents.Get("language"), lanuage)
	require.Equal(t, log.Contents.Get("type"), profileType)
	require.Equal(t, log.Contents.Get("units"), unitType)
	require.Equal(t, log.Contents.Get("valueTypes"), valType)
	require.Equal(t, log.Contents.Get("aggTypes"), aggType)
	require.Equal(t, log.Contents.Get("dataType"), dataType)
	require.Equal(t, log.Contents.Get("durationNs"), strconv.Itoa(endTime-startTime))
	require.Equal(t, log.Contents.Get("labels"), "{\"_app_name_\":\"12\",\"cluster\":\"cluster2\"}")
	require.Equal(t, log.Contents.Get("val"), "250000000.00")
}

func TestProfileLogTimestampAtEpoch(t *testing.T) {
	const timestamp = 123
	push := NewRawProfile(nil, "")
	pull := NewRawProfileByPull(nil, nil, nil)
	meta := &profile.Meta{Tags: map[string]string{"profile_id": "test"}}
	for _, tc := range []struct {
		name  string
		input *RawProfile
	}{
		{name: "push", input: push},
		{name: "pull", input: pull},
	} {
		t.Run(tc.name, func(t *testing.T) {
			before := uint64(time.Now().UnixNano())
			var events []models.PipelineEvent
			cb := tc.input.profileLogCallback(meta, nil, &events)
			cb(1, &profile.Stack{Name: "test"}, []uint64{1}, []string{"cpu"}, []string{"samples"}, []string{"sum"}, timestamp, timestamp+1, nil)
			require.Len(t, events, 1)
			got := events[0].GetTimestamp()
			if tc.input == push {
				require.Equal(t, uint64(timestamp), got)
			} else {
				require.GreaterOrEqual(t, got, before)
				require.LessOrEqual(t, got, uint64(time.Now().UnixNano()))
			}
		})
	}
}
