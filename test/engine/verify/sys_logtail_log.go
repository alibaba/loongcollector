// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package verify

import (
	"bufio"
	"bytes"
	"context"
	"fmt"
	"io"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

func LogtailPluginLog(ctx context.Context, expectCount int, expectStr string) (context.Context, error) {
	dockercompose.CopyCoreLogs()
	logtailPluginLog := config.LogDir + "/logtail_plugin.LOG"
	count, err := lineCounter(logtailPluginLog)
	logger.Infof(context.Background(), "find %d lines of the logtail plugin log", count)
	if err != nil {
		return ctx, fmt.Errorf("read log file %s failed: %v", logtailPluginLog, err)
	}
	// #nosec G304
	f, _ := os.Open(logtailPluginLog)

	workerNum := count/200000 + 1

	stop := make(chan struct{})
	resCh := make(chan string, workerNum)
	var chanArr []chan string
	var swg sync.WaitGroup
	var cwg sync.WaitGroup
	cwg.Add(1)
	actualCount := 0
	go func() {
		defer cwg.Done()
		for range resCh {
			actualCount++
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
					if strings.Contains(line, expectStr) {
						resCh <- expectStr
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
	if actualCount != expectCount {
		return ctx, fmt.Errorf("expect %d lines of logtail plugin log, but got %d", expectCount, actualCount)
	}
	return ctx, nil
}

func lineCounter(logtailPluginLog string) (int, error) {
	// #nosec G304
	r, err := os.Open(logtailPluginLog)
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
