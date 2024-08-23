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
	"strings"
	"time"

	"github.com/avast/retry-go/v4"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func LogLabel(ctx context.Context, expectLabelsStr string) (context.Context, error) {
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

	expectLabels := make([]string, 0)
	if err = yaml.Unmarshal([]byte(expectLabelsStr), &expectLabels); err != nil {
		return ctx, err
	}

	for _, group := range groups {
		for _, log := range group.Logs {
			for _, content := range log.Contents {
				if content.Key == "__labels__" {
					parts := strings.Split(content.Value, "|")
					if len(expectLabels) != len(parts) {
						return ctx, fmt.Errorf("want label num %d, bug got %d", len(expectLabels), len(parts))
					}
					var existKeys []string
					for _, part := range parts {
						kv := strings.Split(part, "#$#")
						if len(kv) != 2 {
							logger.Debugf(context.Background(), "want metric pattern key#$#value, bug got %s", part)
							return ctx, fmt.Errorf("want metric pattern key#$#value, bug got %s", part)
						}
						existKeys = append(existKeys, kv[0])
					}
					var notFoundKeys []string
					for _, name := range expectLabels {
						found := false
						for _, key := range existKeys {
							if key == name {
								found = true
								break
							}
						}
						if !found {
							notFoundKeys = append(notFoundKeys, name)
						}
					}
					if len(notFoundKeys) > 0 {
						notFoundKeys := strings.Join(notFoundKeys, ",")
						return ctx, fmt.Errorf("want metric label keys: %s, but not found: %s", expectLabels, notFoundKeys)
					}
				}
			}
		}
	}
	return ctx, nil
}
