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

	"github.com/alibaba/ilogtail/helper/profile"
	"github.com/alibaba/ilogtail/plugins/test"
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
	unitType    = "count"
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
	err = r.extractLogs(context.Background(), te, p, meta, cb)
	require.NoError(t, err)
	logs := r.logs
	require.Equal(t, len(logs), 6)
	picks := test.PickLogs(logs, "stackID", stackID)
	require.Equal(t, len(picks), 1)
	log := picks[0]
	require.Equal(t, test.ReadLogVal(log, "name"), name)
	require.Equal(t, test.ReadLogVal(log, "stack"), stack)
	require.Equal(t, test.ReadLogVal(log, "language"), lanuage)
	require.Equal(t, test.ReadLogVal(log, "type"), profileType)
	require.Equal(t, test.ReadLogVal(log, "units"), unitType)
	require.Equal(t, test.ReadLogVal(log, "valueTypes"), valType)
	require.Equal(t, test.ReadLogVal(log, "aggTypes"), aggType)
	require.Equal(t, test.ReadLogVal(log, "dataType"), dataType)
	require.Equal(t, test.ReadLogVal(log, "durationNs"), strconv.Itoa(endTime-startTime))
	require.Equal(t, test.ReadLogVal(log, "labels"), "{\"_app_name_\":\"12\",\"cluster\":\"cluster2\"}")
	require.Equal(t, test.ReadLogVal(log, "val"), "25.00")
}
