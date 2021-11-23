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

package validator

import (
	"context"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/mitchellh/mapstructure"
)

const metricsLabelValidator = "metric_field"

type metricLabelLogValidator struct {
	ExpectLabelNum   int      `mapstructure:"expect_label_num" comment:"the expected metric label num"`
	ExpectLabelNames []string `mapstructure:"expect_label_names" comment:"the expected metric labels names"`

	expectKeys string
}

func (m *metricLabelLogValidator) Description() string {
	return "this is a metric label validator to check received log from subscriber"
}

func (m *metricLabelLogValidator) Valid(group *protocol.LogGroup) (reports []*Report) {
	for _, log := range group.Logs {
		for _, content := range log.Contents {
			if content.Key == "__labels__" {
				reports = m.validate(content.Value)
			}
		}
	}
	return
}

func (m *metricLabelLogValidator) Name() string {
	return metricsLabelValidator
}

func (m *metricLabelLogValidator) validate(content string) (reports []*Report) {
	parts := strings.Split(content, "|")
	if m.ExpectLabelNum > 0 && m.ExpectLabelNum != len(parts) {
		logger.Debugf(context.Background(), "want label num %d, bug got %d", m.ExpectLabelNum, len(parts))
		reports = append(reports, &Report{Validator: metricsLabelValidator, Name: "label_num", Want: strconv.Itoa(m.ExpectLabelNum), Got: strconv.Itoa(len(parts))})
	}
	if len(m.ExpectLabelNames) > 0 {
		var existKeys []string
		for _, part := range parts {
			kv := strings.Split(part, "#$#")
			if len(kv) != 2 {
				logger.Debugf(context.Background(), "want metric pattern key#$#value, bug got %s", part)
				reports = append(reports, &Report{Validator: metricsLabelValidator, Name: "label_pattern", Want: "key#$#value", Got: part})
				continue
			}
			existKeys = append(existKeys, kv[0])
		}
		var notFoundKeys []string
		for _, name := range m.ExpectLabelNames {
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
			logger.Debugf(context.Background(), "want metric label keys: %s, but not found: %s", m.expectKeys, notFoundKeys)
			reports = append(reports, &Report{Validator: metricsLabelValidator, Name: "label_field", Want: m.expectKeys, NotFound: notFoundKeys})
		}
	}
	return reports
}

func init() {
	RegisterLogValidatorCreator(metricsLabelValidator, func(spec map[string]interface{}) (LogValidator, error) {
		f := new(metricLabelLogValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		if len(f.ExpectLabelNames) > 0 {
			f.expectKeys = strings.Join(f.ExpectLabelNames, ",")
		}
		return f, nil
	})
	doc.Register("log_validator", metricsLabelValidator, new(metricLabelLogValidator))
}
