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
	"context"
	"fmt"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func RegexSingle(ctx context.Context) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}
	fields := []string{"mark", "file", "logno", "ip", "time", "method", "url", "http", "status", "size", "useragent", "msg"}
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	err = retry.Do(
		func() error {
			groups, err = subscriber.TestSubscriber.GetData(from)
			if err != nil {
				return err
			}
			for _, group := range groups {
				for _, log := range group.Logs {
					for _, field := range fields {
						found := false
						for _, content := range log.Contents {
							if content.Key == field {
								found = true
								break
							}
						}
						if !found {
							return fmt.Errorf("field %s not found", field)
						}
					}
				}
			}
			return err
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}
