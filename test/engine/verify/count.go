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
	"strconv"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func LogCount(ctx context.Context, expect int) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	var count int
	err = retry.Do(
		func() error {
			count = 0
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			if err != nil {
				return err
			}
			for _, group := range groups {
				count += len(group.Logs)
			}
			if count != expect {
				return fmt.Errorf("log count not match, expect %d, got %d, from %d", expect, count, from)
			}
			if expect == 0 {
				return fmt.Errorf("log count is 0")
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if expect == 0 && count == expect {
		return ctx, nil
	}
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}

func LogCountLess(ctx context.Context, expect int) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	var count int
	err = retry.Do(
		func() error {
			count = 0
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			if err != nil {
				return err
			}
			for _, group := range groups {
				count += len(group.Logs)
			}
			if count != expect {
				return fmt.Errorf("log count not match, expect %d, got %d, from %d", expect, count, from)
			}
			if expect == 0 {
				return fmt.Errorf("log count is 0")
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if count > 0 && count < expect {
		return ctx, nil
	}
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}

func MetricCheck(ctx context.Context, expect int, duration int64, checker func([]*protocol.LogGroup) error) (context.Context, error) {
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	var count int
	err = retry.Do(
		func() error {
			count = 0
			currTime := time.Now().Unix()
			lastScrapeTime := int32(currTime - duration)
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), lastScrapeTime)
			if err != nil {
				return err
			}
			for _, group := range groups {
				count += len(group.Logs)
			}
			if count < expect {
				return fmt.Errorf("metric count not match, expect %d, got %d, from %d", expect, count, lastScrapeTime)
			}
			if expect == 0 {
				return fmt.Errorf("metric count is 0")
			}
			if err = checker(groups); err != nil {
				return err
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if expect == 0 && count == expect {
		return ctx, nil
	}
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}

func MetricCount(ctx context.Context, expect int, duration int64) (context.Context, error) {
	return MetricCheck(ctx, expect, duration, func(groups []*protocol.LogGroup) error {
		return nil
	})
}

func MetricCountAndValueCompare(ctx context.Context, expect int, duration int64, minValue int64, maxValue int64) (context.Context, error) {
	return MetricCheck(ctx, expect, duration, func(groups []*protocol.LogGroup) error {
		lessCount := 0
		greaterCount := 0
		for _, group := range groups {
			for _, log := range group.Logs {
				for _, content := range log.Contents {
					if content.Key == "value" {
						value, err := strconv.ParseFloat(content.Value, 64)
						if err != nil {
							return fmt.Errorf("parse value failed: %v", err)
						}
						if value < float64(minValue) {
							lessCount++
						}
						if value > float64(maxValue) {
							greaterCount++
						}
					}
				}
			}
		}
		if lessCount > 0 || greaterCount > 0 {
			return fmt.Errorf("metric value not match, lessCount %d, greaterCount %d", lessCount, greaterCount)
		}
		return nil
	})
}

func MetricCountAndValueEqual(ctx context.Context, expect int, duration int64, expectValue int64) (context.Context, error) {
	return MetricCheck(ctx, expect, duration, func(groups []*protocol.LogGroup) error {
		notEqualCount := 0
		for _, group := range groups {
			for _, log := range group.Logs {
				for _, content := range log.Contents {
					if content.Key == "value" {
						value, err := strconv.ParseFloat(content.Value, 64)
						if err != nil {
							return fmt.Errorf("parse value failed: %v", err)
						}
						if value != float64(expectValue) {
							notEqualCount++
						}
					}
				}
			}
		}
		if notEqualCount > 0 {
			return fmt.Errorf("metric value not match, not equal count %d", notEqualCount)
		}
		return nil
	})
}

func LogCountAtLeast(ctx context.Context, expect int) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	var count int
	err = retry.Do(
		func() error {
			count = 0
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			if err != nil {
				return err
			}
			for _, group := range groups {
				count += len(group.Logs)
			}
			if count < expect {
				return fmt.Errorf("log count not match, expect at least %d, got %d", expect, count)
			}
			if expect == 0 {
				return fmt.Errorf("log count is 0")
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if expect == 0 && count == expect {
		return ctx, nil
	}
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}

func LogCountAtLeastWithFilter(ctx context.Context, sql string, expect int, filterKey string, filterValue string) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	var err error
	var count int
	err = retry.Do(
		func() error {
			count = 0
			groups, err = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			if err != nil {
				return err
			}
			count = 0
			for _, group := range groups {
				for _, log := range group.Logs {
					for _, content := range log.Contents {
						if content.Key == filterKey && content.Value == filterValue {
							count++
							break
						}
					}
				}
			}
			if count < expect {
				return fmt.Errorf("log count not match, expect at least %d, got %d", expect, count)
			}
			if expect == 0 {
				return fmt.Errorf("log count is 0")
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if expect == 0 && count == expect {
		return ctx, nil
	}
	if err != nil {
		return ctx, err
	}
	return ctx, nil
}
