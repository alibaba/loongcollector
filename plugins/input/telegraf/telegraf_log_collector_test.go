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
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/stretchr/testify/assert"

	"fmt"
	"os"
	"testing"
	"time"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenConsole)

}

var infos = []string{
	"2022-05-17T02:25:13Z D! Loaded inputs: %d debug\n",
	"2022-05-17T02:25:13Z I! Loaded inputs: %d info\n",
	"2022-05-17T02:25:13Z W! Loaded inputs: %d warning\n",
	"2022-05-17T02:25:13Z E! Loaded inputs: %d error\n",
}

func Creator(t *testing.T, stop chan struct{}) {
	_ = os.Remove("telegraf.log")
	file, err := os.OpenFile("telegraf.log", os.O_WRONLY|os.O_APPEND|os.O_CREATE, 0666)
	var count int64
	assert.NoError(t, err)
	defer file.Close()
	ticker := time.NewTicker(time.Second)

	// mock history log
	for i := 0; i < 10; i++ {
		file.WriteString(fmt.Sprintf(infos[i%4], count))
		count++

	}
	// mock new log
	for {
		select {
		case t := <-ticker.C:
			file.WriteString(fmt.Sprintf(infos[t.Second()%4], count))
			count++
		case <-stop:
			return
		}
	}
}

func TestNewLogCollector(t *testing.T) {
	stopChan := make(chan struct{})
	defer func() {
		stopChan <- struct{}{}
		time.Sleep(time.Second)
	}()
	go Creator(t, stopChan)
	time.Sleep(time.Second)
	collector := NewLogCollector(".")
	go collector.Run()
	go func() {
		time.Sleep(2 * time.Second)
	}()

	collector.TelegrafStart()

	time.Sleep(time.Hour)

}
