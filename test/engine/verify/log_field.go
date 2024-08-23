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
	"regexp"
	"time"

	"github.com/avast/retry-go/v4"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func LogField(ctx context.Context, expectFieldStr string) (context.Context, error) {
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

	expectFields := make([]string, 0)
	err = yaml.Unmarshal([]byte(expectFieldStr), &expectFields)
	if err != nil {
		return ctx, err
	}
	for _, group := range groups {
		for _, log := range group.Logs {
			for _, field := range expectFields {
				for _, content := range log.Contents {
					if content.Key == field {
						goto find
					}
				}
				return ctx, fmt.Errorf("want contains field %s, but not found, current: %s", field, log.Contents)
			find:
			}
		}
	}
	return ctx, nil
}

func LogFieldKV(ctx context.Context, expectKeyValuesStr string) (context.Context, error) {
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

	kvRegexps := make(map[string]*regexp.Regexp)
	expectKeyValues := make(map[string]string)
	err = yaml.Unmarshal([]byte(expectKeyValuesStr), expectKeyValues)
	for k, v := range expectKeyValues {
		reg, err := regexp.Compile(v)
		if err != nil {
			return ctx, err
		}
		kvRegexps[k] = reg
	}

	for _, group := range groups {
		for _, log := range group.Logs {
			for k, reg := range kvRegexps {
				for _, content := range log.Contents {
					if content.Key == k {
						if !reg.MatchString(content.Value) {
							return ctx, fmt.Errorf("want contains KV %s:%s, but got %s:%s", k, reg.String(), content.Key, content.Value)
						}
						goto find
					}
				}
				return ctx, fmt.Errorf("want contains KV %s:%s, but not found", k, reg.String())
			find:
			}
		}
	}
	return ctx, nil
}
