package baseagg

import (
	"crypto/rand"
	"encoding/hex"
	"strings"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

var (
	shortLog  = "This is short log. This log comes from source "
	mediumLog = strings.Repeat("This is medium log. ", 20) + "This log comes from source "
	longLog   = strings.Repeat("This is long log. ", 200) + "This log comes from source "
)

type SliceQueue struct {
	logGroups []*protocol.LogGroup
}

func NewSliceQueue() *SliceQueue {
	return &SliceQueue{
		logGroups: make([]*protocol.LogGroup, 0, 10),
	}
}

func (q *SliceQueue) Add(logGroup *protocol.LogGroup) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func (q *SliceQueue) AddWithWait(logGroup *protocol.LogGroup, duration time.Duration) error {
	q.logGroups = append(q.logGroups, logGroup)
	return nil
}

func (q *SliceQueue) PopAll() []*protocol.LogGroup {
	return q.logGroups
}

func newAggregatorBase() (*AggregatorBase, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	que := NewSliceQueue()
	agg := NewAggregatorBase()
	_, err := agg.Init(ctx, que)
	return agg, err
}

// common situation: 10 log sources, 10 logs per second per file, medium log
func BenchmarkAdd(b *testing.B) {
	agg, _ := newAggregatorBase()
	log := &protocol.Log{
		Time:     uint32(time.Now().Unix()),
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 300; j++ {
			agg.Add(log, ctx[j%10])
		}
	}
}

// 1 log source
func BenchmarkLogSource1(b *testing.B) {
	benchmarkLogSource(b, 1)
}

// 100 log sources
func BenchmarkLogSource100(b *testing.B) {
	benchmarkLogSource(b, 100)
}

func benchmarkLogSource(b *testing.B, num int) {
	agg, _ := newAggregatorBase()
	log := &protocol.Log{
		Time:     uint32(time.Now().Unix()),
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	ctx := make([]map[string]interface{}, num)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < num; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 30*num; j++ {
			agg.Add(log, ctx[j%num])
		}
		_ = agg.Flush()
	}
}

// 1 log per second per file
func BenchmarkLogProducinfPace1(b *testing.B) {
	benchmarkLogProducingPace(b, 1)
}

// 1000 logs per second per file
func BenchmarkLogProducinfPace1000(b *testing.B) {
	benchmarkLogProducingPace(b, 1000)
}

func benchmarkLogProducingPace(b *testing.B, num int) {
	agg, _ := newAggregatorBase()
	log := &protocol.Log{
		Time:     uint32(time.Now().Unix()),
		Contents: []*protocol.Log_Content{{Key: "content", Value: mediumLog}},
	}
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 3*num*10; j++ {
			agg.Add(log, ctx[j%10])
		}
		_ = agg.Flush()
	}
}

func BenchmarkLogLengthShort(b *testing.B) {
	benchmarkLogLength(b, "short")
}

func BenchmarkLogLengthLong(b *testing.B) {
	benchmarkLogLength(b, "long")
}

func benchmarkLogLength(b *testing.B, len string) {
	agg, _ := newAggregatorBase()
	var value string
	switch len {
	case "short":
		value = shortLog
	case "long":
		value = longLog
	default:
		value = mediumLog
	}
	log := &protocol.Log{
		Time:     uint32(time.Now().Unix()),
		Contents: []*protocol.Log_Content{{Key: "content", Value: value}},
	}
	ctx := make([]map[string]interface{}, 10)
	packIDPrefix := make([]byte, 8)
	for i := 0; i < 10; i++ {
		rand.Read(packIDPrefix)
		ctx[i] = map[string]interface{}{"source": hex.EncodeToString(packIDPrefix) + "-", "topic": "file"}
	}
	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for j := 0; j < 300; j++ {
			agg.Add(log, ctx[j%10])
		}
		_ = agg.Flush()
	}
}
