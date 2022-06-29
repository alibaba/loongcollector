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

package validator

import (
	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"

	"bufio"
	"bytes"
	"context"
	"io"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/mitchellh/mapstructure"
)

const logtailLogValidatorName = "sys_logtail_log"

type logtailLogValidator struct {
	ExpectContainsLogTimes map[string]int `mapstructure:"expect_contains_log_times" comment:"the times of the expected logs in LogtailPlugin."`
	Main                   bool           `mapstructure:"main_log" comment:"the C part log would be checked when configured true, otherwise is Go part log."`
	logtailPluginLog       string
	logtailLog             string
}

func (l *logtailLogValidator) Description() string {
	return "this is a LogtailPlugin log validator to check the behavior of LogtailPlugin"
}

func (l *logtailLogValidator) Start() error {
	l.logtailPluginLog = config.LogDir + "/logtail_plugin.LOG"
	l.logtailLog = config.LogDir + "/ilogtail.LOG"
	return nil
}

func (l *logtailLogValidator) Valid(group *protocol.LogGroup) {

}

func (l *logtailLogValidator) FetchResult() (reports []*Report) {
	count, err := l.lineCounter()
	logger.Infof(context.Background(), "find %d lines of the logtail plugin log", count)
	if err != nil {
		logger.Error(context.Background(), "READ_LOG_ALARM", "file", l.GetFile(), "err", err)
		reports = append(reports, &Report{Validator: logtailLogValidatorName, Name: "err", Want: "success", Got: "error"})
		return
	}
	f, _ := os.Open(l.GetFile())

	workerNum := count/200000 + 1

	stop := make(chan struct{})
	resCh := make(chan string, workerNum)
	result := make(map[string]int)
	var chanArr []chan string
	var swg sync.WaitGroup
	var cwg sync.WaitGroup
	cwg.Add(1)
	go func() {
		defer cwg.Done()
		for flag := range resCh {
			result[flag]++
		}
	}()

	for i := 0; i < workerNum; i++ {
		ch := make(chan string, 10)
		chanArr = append(chanArr, ch)
		swg.Add(1)
		go func() {
			defer swg.Done()
			for {
				select {
				case line := <-ch:
					for k := range l.ExpectContainsLogTimes {
						if strings.Contains(line, k) {
							resCh <- k
						}
					}
				case <-stop:
					return
				}
			}
		}()
	}

	scanner := bufio.NewScanner(f)
	cursor := 0
	for {
		if !scanner.Scan() {
			time.Sleep(10 * time.Millisecond)
			break
		}
		line := scanner.Text()
		num := cursor % workerNum
		chanArr[num] <- line
		cursor++
	}
	close(stop)
	swg.Wait()
	for {
		if len(resCh) == 0 {
			close(resCh)
			break
		}
		time.Sleep(time.Microsecond)
	}
	cwg.Wait()
	for flag, times := range l.ExpectContainsLogTimes {
		if result[flag] != times {
			reports = append(reports, &Report{Validator: logtailLogValidatorName, Name: "logtail_log_times_" + flag, Want: strconv.Itoa(times), Got: strconv.Itoa(result[flag])})
		}
	}
	return
}

func (l *logtailLogValidator) Name() string {
	return logtailLogValidatorName
}

func (l *logtailLogValidator) GetFile() string {
	if l.Main {
		return l.logtailLog
	}
	return l.logtailPluginLog

}

func (l *logtailLogValidator) lineCounter() (int, error) {
	r, err := os.Open(l.GetFile())
	if err != nil {
		return -1, err
	}
	defer func(r *os.File) {
		_ = r.Close()
	}(r)
	buf := make([]byte, 32*1024)
	count := 0
	lineSep := []byte{'\n'}
	for {
		c, err := r.Read(buf)
		count += bytes.Count(buf[:c], lineSep)

		switch {
		case err == io.EOF:
			return count, nil

		case err != nil:
			return count, err
		}
	}
}

func init() {
	RegisterSystemValidatorCreator(logtailLogValidatorName, func(spec map[string]interface{}) (SystemValidator, error) {
		v := new(logtailLogValidator)
		err := mapstructure.Decode(spec, v)
		if err != nil {
			return nil, err
		}
		return v, nil
	})
	doc.Register("sys_validator", logtailLogValidatorName, new(logtailLogValidator))
}
