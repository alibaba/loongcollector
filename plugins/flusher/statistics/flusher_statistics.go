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
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/protocol"

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

// Flush flushes @logGroupList but it only do statistics.
// It returns any error it encountered.
func (p *FlusherStatistics) Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error {
	for _, logGroup := range logGroupList {
		p.loggroupRateCounter.Incr(1)
		p.logRateCounter.Incr((int64)(len(logGroup.Logs)))
		if p.GeneratePB {
			buf, err := logGroup.Marshal()
			if err != nil {
				return fmt.Errorf("loggroup marshal err %v", err)
			}
			p.byteRateCount.Incr((int64)(len(buf)))
		}
		time.Sleep(time.Millisecond * time.Duration(p.SleepMsPerLogGroup))
	}

	nowTime := time.Now()
	if nowTime.Sub(p.lastOutputTime) >= (time.Duration)(p.RateIntervalMs)*time.Millisecond {
		logger.Info(p.context.GetRuntimeContext(), "current rate(MB)", float32(p.byteRateCount.Rate())/1024.0/1024.0, "log tps", p.logRateCounter.Rate(), "loggroup tps", p.loggroupRateCounter.Rate())
		p.lastOutputTime = nowTime
	}
	return nil
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
