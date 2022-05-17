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

package telegraf

import (
	"context"
	"os"
	"path"
	"sync"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

type LogCollector struct {
	readers   []*helper.LogFileReader
	startChan chan struct{}
	stopChan  chan struct{}
	started   bool
	once      sync.Once
}

const (
	runninglogPath    = "telegraf.log"
	startLogPath      = "start_telegraf.log"
	TelegrafAlarmType = "TELEGRAF_ALARM"
)

// NewLogCollector create a log collector to read telegraf log.
func NewLogCollector(agentDirPath string) *LogCollector {
	c := &LogCollector{
		startChan: make(chan struct{}),
		stopChan:  make(chan struct{}),
	}
	if reader := newTelegrafLogReader(agentDirPath, runninglogPath, true); reader != nil {
		c.readers = append(c.readers, reader)
	}
	if reader := newTelegrafLogReader(agentDirPath, startLogPath, false); reader != nil {
		c.readers = append(c.readers, reader)
	}
	return c
}

func newTelegrafLogReader(agentDirPath string, logPath string, drop bool) *helper.LogFileReader {
	checkPoint := helper.LogFileReaderCheckPoint{
		Path: path.Join(agentDirPath, logPath),
	}
	if drop {
		info, err := os.Stat(checkPoint.Path)
		if err != nil {
			logger.Warning(context.Background(), TelegrafAlarmType, "stat log file error, path", checkPoint.Path, "error", err.Error())
		} else {
			checkPoint.Offset = info.Size()
			checkPoint.State = helper.GetOSState(info)
		}
	}

	reader, err := helper.NewLogFileReader(checkPoint, helper.DefaultLogFileReaderConfig, new(Processor), context.Background())
	if err != nil {
		logger.Error(context.Background(), TelegrafAlarmType, "path", checkPoint.Path, "create telegraf log reader error", err)
		return nil
	}
	return reader
}

func (l *LogCollector) Run() {
	for {
		logger.Debug(context.Background(), "run telegraf collector")
		select {
		case <-l.startChan:
			if !l.started {
				for _, reader := range l.readers {
					reader.Start()
				}
				l.started = true
				logger.Info(context.Background(), "telegraf log collector started, count", len(l.readers))
			}
		case <-l.stopChan:
			if l.started {
				for _, reader := range l.readers {
					reader.Stop()
				}
				logger.Info(context.Background(), "telegraf log collector stopped, count", len(l.readers))
				l.started = false
			}
		}
	}
}

func (l *LogCollector) TelegrafStop() {
	logger.Debug(context.Background(), "trigger collector stop")
	l.stopChan <- struct{}{}
}

func (l *LogCollector) TelegrafStart() {
	logger.Debug(context.Background(), "trigger collector start")
	l.startChan <- struct{}{}
}
