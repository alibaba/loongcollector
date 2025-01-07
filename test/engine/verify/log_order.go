// Copyright 2024 iLogtail Authors
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

package verify

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func LogOrder(ctx context.Context) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}

	// Get logs
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var err error
	var groups []*protocol.LogGroup
	err = retry.Do(
		func() error {
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			return err
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return ctx, err
	}

	// Check log order
	currentLogNo := 0
	for i := 0; i < len(groups); i++ {
		for j := 0; j < len(groups[i].Logs); j++ {
			if j == 0 {
				currentLogNo, _ = getLogNoFromLog(groups[i].Logs[j])
				continue
			}
			if groups[i].Logs[j].Time > groups[i].Logs[j-1].Time {
				if nextLogNo, ok := getLogNoFromLog(groups[i].Logs[j]); ok {
					if nextLogNo != currentLogNo+1 {
						return ctx, fmt.Errorf("log order is not correct, current logNo: %d, next logNo: %d", currentLogNo, nextLogNo)
					}
					currentLogNo = nextLogNo
				}
				continue
			}
		}
	}
	return ctx, nil
}

func getLogNoFromLog(log *protocol.Log) (int, bool) {
	for _, content := range log.Contents {
		if content.Key == "logNo" {
			logNo, err := strconv.Atoi(content.Value)
			if err != nil {
				return 0, false
			}
			return logNo, true
		}
	}
	return 0, false
}
