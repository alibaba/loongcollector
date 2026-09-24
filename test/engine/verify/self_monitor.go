// Copyright 2026 iLogtail Authors
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
	"context"
	"encoding/json"
	"fmt"
	"os"
	"regexp"
	"time"

	"github.com/avast/retry-go/v4"
	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/test/config"
)

const selfMonitorRetryTimeout = 90 * time.Second

// containerPluginSourceLabelKeys are written only when the reader has container metadata.
var containerPluginSourceLabelKeys = []string{
	"_namespace_",
	"_pod_name_",
	"_pod_uid_",
	"_container_name_",
	"_container_ip_",
	"_image_name_",
}

type selfMonitorMetricLine struct {
	Name   string            `json:"__name__"`
	Labels map[string]string `json:"__labels__"`
}

func SelfMonitorPluginSourceLabelsMatchKV(ctx context.Context, expectKeyValuesStr string) (context.Context, error) {
	expectKeyValues := make(map[string]string)
	if err := yaml.Unmarshal([]byte(expectKeyValuesStr), expectKeyValues); err != nil {
		return ctx, err
	}
	kvRegexps := make(map[string]*regexp.Regexp, len(expectKeyValues))
	for k, v := range expectKeyValues {
		reg, err := regexp.Compile(v)
		if err != nil {
			return ctx, err
		}
		kvRegexps[k] = reg
	}

	timeoutCtx, cancel := context.WithTimeout(context.TODO(), selfMonitorRetryTimeout)
	defer cancel()
	err := retry.Do(
		func() error {
			metrics, readErr := readSelfMonitorMetrics("plugin_source")
			if readErr != nil {
				return readErr
			}
			return matchPluginSourceLabels(metrics, kvRegexps)
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

func SelfMonitorPluginSourceLabelsOmitContainerKeys(ctx context.Context) (context.Context, error) {
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), selfMonitorRetryTimeout)
	defer cancel()
	err := retry.Do(
		func() error {
			metrics, readErr := readSelfMonitorMetrics("plugin_source")
			if readErr != nil {
				return readErr
			}
			return pluginSourceLabelsOmitKeys(metrics, containerPluginSourceLabelKeys)
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

func readSelfMonitorMetrics(category string) ([]selfMonitorMetricLine, error) {
	if config.SelfMonitorFile == "" {
		return nil, fmt.Errorf("self-monitor file path is empty")
	}
	f, err := os.Open(config.SelfMonitorFile)
	if err != nil {
		return nil, fmt.Errorf("open self-monitor file %s: %w", config.SelfMonitorFile, err)
	}
	defer f.Close()

	var metrics []selfMonitorMetricLine
	scanner := bufio.NewScanner(f)
	scanner.Buffer(make([]byte, 0, 64*1024), 4*1024*1024)
	for scanner.Scan() {
		line := scanner.Bytes()
		if len(line) == 0 {
			continue
		}
		var metric selfMonitorMetricLine
		if err := json.Unmarshal(line, &metric); err != nil {
			continue
		}
		if metric.Name == category {
			metrics = append(metrics, metric)
		}
	}
	if err := scanner.Err(); err != nil {
		return nil, err
	}
	if len(metrics) == 0 {
		return nil, fmt.Errorf("no %s metrics in %s", category, config.SelfMonitorFile)
	}
	return metrics, nil
}

func matchPluginSourceLabels(metrics []selfMonitorMetricLine, kvRegexps map[string]*regexp.Regexp) error {
	var lastErr error
	for _, metric := range metrics {
		err := matchLabels(metric.Labels, kvRegexps)
		if err == nil {
			return nil
		}
		lastErr = err
	}
	if lastErr == nil {
		return fmt.Errorf("no plugin_source metrics to match")
	}
	return lastErr
}

func pluginSourceLabelsOmitKeys(metrics []selfMonitorMetricLine, keys []string) error {
	for _, metric := range metrics {
		for _, key := range keys {
			if _, ok := metric.Labels[key]; ok {
				return fmt.Errorf("plugin_source label %s should be absent, labels=%v", key, metric.Labels)
			}
		}
	}
	return nil
}

func matchLabels(labels map[string]string, kvRegexps map[string]*regexp.Regexp) error {
	for key, reg := range kvRegexps {
		value, ok := labels[key]
		if !ok {
			return fmt.Errorf("want plugin_source label %s:%s, but not found, labels=%v", key, reg.String(), labels)
		}
		if !reg.MatchString(value) {
			return fmt.Errorf("want plugin_source label %s:%s, but got %s", key, reg.String(), value)
		}
	}
	return nil
}
