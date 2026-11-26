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

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

type eventSubscriber interface {
	GetEventData() ([]*models.PipelineGroupEvents, error)
}

func LogContentsExactly(ctx context.Context, expected string) (context.Context, error) {
	return ctx, checkExactLogValues(expected, "fields", func(_ *models.GroupInfo, log *models.Log) map[string]string {
		fields := make(map[string]string, log.GetIndices().Len())
		for key, value := range log.GetIndices().Iterator() {
			fields[key] = pipeline.GetStringValue(value)
		}
		return fields
	})
}

func LogGroupTagsExactly(ctx context.Context, expected string) (context.Context, error) {
	return ctx, checkExactLogValues(expected, "group tags", func(group *models.GroupInfo, _ *models.Log) map[string]string {
		return group.GetTags().Iterator()
	})
}

func LogEventTagsExactly(ctx context.Context, expected string) (context.Context, error) {
	return ctx, checkExactLogValues(expected, "event tags", func(_ *models.GroupInfo, log *models.Log) map[string]string {
		return log.GetTags().Iterator()
	})
}

func LogContentsExcludeKeys(ctx context.Context, expected string) (context.Context, error) {
	var keys []string
	if err := yaml.Unmarshal([]byte(expected), &keys); err != nil {
		return ctx, err
	}
	return ctx, checkReceivedLogs(func(_ *models.GroupInfo, log *models.Log) error {
		for _, key := range keys {
			if log.GetIndices().Contains(key) {
				return fmt.Errorf("log contains forbidden field %q", key)
			}
		}
		return nil
	})
}

func checkExactLogValues(expected string, name string, values func(*models.GroupInfo, *models.Log) map[string]string) error {
	var patterns map[string]string
	if err := yaml.Unmarshal([]byte(expected), &patterns); err != nil {
		return err
	}
	regexps := make(map[string]*regexp.Regexp, len(patterns))
	for key, pattern := range patterns {
		compiled, err := regexp.Compile("\\A(?:" + pattern + ")\\z")
		if err != nil {
			return fmt.Errorf("invalid %s pattern for %q: %w", name, key, err)
		}
		regexps[key] = compiled
	}
	return checkReceivedLogs(func(group *models.GroupInfo, log *models.Log) error {
		actual := values(group, log)
		if len(actual) != len(regexps) {
			return fmt.Errorf("want %d %s, got %d: %v", len(regexps), name, len(actual), actual)
		}
		for key, regexp := range regexps {
			if value, ok := actual[key]; !ok || !regexp.MatchString(value) {
				return fmt.Errorf("want %s %q matching %q, got %v", name, key, patterns[key], actual)
			}
		}
		return nil
	})
}

func checkReceivedLogs(check func(*models.GroupInfo, *models.Log) error) error {
	source, ok := subscriber.TestSubscriber.(eventSubscriber)
	if !ok {
		return fmt.Errorf("the subscriber does not expose native log events")
	}
	groups, err := source.GetEventData()
	if err != nil {
		return err
	}
	checked := 0
	for _, group := range groups {
		for _, event := range group.Events {
			if log, ok := event.(*models.Log); ok {
				if err := check(group.Group, log); err != nil {
					return fmt.Errorf("log %d: %w", checked+1, err)
				}
				checked++
			}
		}
	}
	if checked == 0 {
		return fmt.Errorf("no log events received")
	}
	return nil
}
