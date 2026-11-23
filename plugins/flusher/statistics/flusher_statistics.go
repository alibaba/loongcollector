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

package statistics

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/paulbellamy/ratecounter"
)

// FlusherStatistics only does statistics for data to flush instead of flushing really.
// It maintains three rates:
// 1. loggroupRateCounter: the rate of the number of log groups to flush.
// 2. logRateCounter: the rate of the total number of logs in log groups to flush.
// 3. byteRateCounter: the rate of the total (serialized) bytes of logs in log groups to flush.
type FlusherStatistics struct {
	SleepMsPerLogGroup  int
	RateIntervalMs      int
	GeneratePB          bool
	loggroupRateCounter *ratecounter.RateCounter
	logRateCounter      *ratecounter.RateCounter
	byteRateCount       *ratecounter.RateCounter
	lastOutputTime      time.Time
	context             pipeline.Context
}

func (p *FlusherStatistics) Init(context pipeline.Context) error {
	if p.RateIntervalMs == 0 {
		p.RateIntervalMs = 1000
	}
	p.loggroupRateCounter = ratecounter.NewRateCounter((time.Duration)(p.RateIntervalMs) * time.Millisecond)
	p.logRateCounter = ratecounter.NewRateCounter((time.Duration)(p.RateIntervalMs) * time.Millisecond)
	p.byteRateCount = ratecounter.NewRateCounter((time.Duration)(p.RateIntervalMs) * time.Millisecond)
	p.lastOutputTime = time.Now()
	p.context = context
	return nil
}

func (*FlusherStatistics) Description() string {
	return "stdout flusher for logtail"
}

func (*FlusherStatistics) SetUrgent(flag bool) {
}

// Export counts events, groups and their sizes.
func (p *FlusherStatistics) Export(groups []*models.PipelineGroupEvents, _ pipeline.PipelineContext) error {
	for _, groupEvents := range groups {
		if groupEvents == nil {
			continue
		}
		p.loggroupRateCounter.Incr(1)
		p.logRateCounter.Incr((int64)(len(groupEvents.Events)))
		if p.GeneratePB {
			p.byteRateCount.Incr(groupEvents.GetSize())
		}
		time.Sleep(time.Millisecond * time.Duration(p.SleepMsPerLogGroup))
	}
	p.reportRates()
	return nil
}

// reportRates periodically logs the accumulated rates.
func (p *FlusherStatistics) reportRates() {
	nowTime := time.Now()
	if nowTime.Sub(p.lastOutputTime) >= (time.Duration)(p.RateIntervalMs)*time.Millisecond {
		logger.Info(p.context.GetRuntimeContext(), "current rate(MB)", float32(p.byteRateCount.Rate())/1024.0/1024.0, "log tps", p.logRateCounter.Rate(), "loggroup tps", p.loggroupRateCounter.Rate())
		p.lastOutputTime = nowTime
	}
}

// IsReady is ready to flush
func (*FlusherStatistics) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	return true
}

// Stop ...
func (*FlusherStatistics) Stop() error {
	return nil
}

func init() {
	pipeline.Flushers["flusher_statistics"] = func() pipeline.Flusher {
		return &FlusherStatistics{}
	}
}
