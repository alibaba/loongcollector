// Copyright 2025 iLogtail Authors
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
	"encoding/json"
	"fmt"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func KafkaPartitionsAtLeast(ctx context.Context, expectedStr string) (context.Context, error) {
	return verifyPartitions(ctx, expectedStr, func(observed, expected int) error {
		if observed < expected {
			return fmt.Errorf("observed partitions %d < expected at least %d", observed, expected)
		}
		return nil
	})
}

func KafkaPartitionsEqual(ctx context.Context, expectedStr string) (context.Context, error) {
	return verifyPartitions(ctx, expectedStr, func(observed, expected int) error {
		if observed != expected {
			return fmt.Errorf("observed partitions %d != expected %d", observed, expected)
		}
		return nil
	})
}

func verifyPartitions(ctx context.Context, expectedStr string, cmp func(int, int) error) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}

	expected, err := strconv.Atoi(expectedStr)
	if err != nil {
		return ctx, fmt.Errorf("invalid expected number: %v", err)
	}

	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	err = retry.Do(
		func() error {
			var err2 error
			groups, err2 = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			return err2
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return ctx, err
	}

	parts := make(map[string]struct{})
	for _, group := range groups {
		for _, lg := range group.Logs {
			for _, c := range lg.Contents {
				if c.Key == "partition" && c.Value != "" {
					parts[c.Value] = struct{}{}
				}
			}
		}
	}

	if err := cmp(len(parts), expected); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func KafkaPartitionsConsistentByField(ctx context.Context, field string) (context.Context, error) {
	groups, err := fetchLogs(ctx)
	if err != nil {
		return ctx, err
	}

	m := make(map[string]map[string]struct{}) // key -> set(partitions)
	total := 0
	for _, g := range groups {
		for _, lg := range g.Logs {
			var content string
			var partition string
			var keyField string
			for _, c := range lg.Contents {
				switch c.Key {
				case "content":
					content = c.Value
				case "partition":
					partition = c.Value
				case field: // prefer direct field if subscriber extracted it
					keyField = c.Value
				}
			}
			if partition == "" {
				continue
			}
			var key string
			if keyField != "" {
				key = keyField
			} else {
				var ok bool
				key, ok = extractJSONField(content, field)
				if !ok || key == "" {
					continue
				}
			}
			if _, ok := m[key]; !ok {
				m[key] = make(map[string]struct{})
			}
			m[key][partition] = struct{}{}
			total++
		}
	}
	for k, parts := range m {
		if len(parts) != 1 {
			return ctx, fmt.Errorf("key %s appears in multiple partitions: %v", k, setToSortedSlice(parts))
		}
	}
	logger.Infof(context.Background(), "KAFKA_PARTITION_VERIFY checked %d records, %d keys, all deterministic by %s", total, len(m), field)
	return ctx, nil
}

func KafkaPrintPartitionMappingByField(ctx context.Context, field string) (context.Context, error) {
	groups, err := fetchLogs(ctx)
	if err != nil {
		return ctx, err
	}
	m := make(map[string]map[string]struct{})
	for _, g := range groups {
		for _, lg := range g.Logs {
			var content string
			var partition string
			for _, c := range lg.Contents {
				switch c.Key {
				case "content":
					content = c.Value
				case "partition":
					partition = c.Value
				}
			}
			if content == "" || partition == "" {
				continue
			}
			key, ok := extractJSONField(content, field)
			if !ok || key == "" {
				continue
			}
			if _, ok := m[key]; !ok {
				m[key] = make(map[string]struct{})
			}
			m[key][partition] = struct{}{}
		}
	}
	keys := make([]string, 0, len(m))
	for k := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	for _, k := range keys {
		parts := setToSortedSlice(m[k])
		logger.Infof(context.Background(), "KAFKA_PARTITION_MAPPING key=%s partitions=%s", k, strings.Join(parts, ","))
	}
	return ctx, nil
}

func fetchLogs(ctx context.Context) ([]*protocol.LogGroup, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return nil, fmt.Errorf("no start time")
	}

	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var groups []*protocol.LogGroup
	err := retry.Do(
		func() error {
			var err2 error
			groups, err2 = subscriber.TestSubscriber.GetData(control.GetQuery(ctx), from)
			return err2
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return nil, err
	}
	return groups, nil
}

func extractJSONField(raw string, field string) (string, bool) {
	var m map[string]interface{}
	if err := json.Unmarshal([]byte(raw), &m); err == nil {
		if v, ok := m[field]; ok {
			if s, ok2 := v.(string); ok2 {
				return s, true
			}
		}
		return "", false
	}
	needle := fmt.Sprintf("\"%s\":\"", field)
	idx := strings.Index(raw, needle)
	if idx == -1 {
		return "", false
	}
	start := idx + len(needle)
	end := strings.Index(raw[start:], "\"")
	if end == -1 {
		return "", false
	}
	return raw[start : start+end], true
}

func setToSortedSlice(m map[string]struct{}) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}
