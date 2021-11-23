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

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const fieldsLogValidatorName = "log_fields"

type fieldsLogValidator struct {
	ExpectLogFields   []string `mapstructure:"expect_log_fields" comment:"the expected log fields mapping"`
	ExpectLogFieldNum int      `mapstructure:"expect_log_field_num" comment:"the expected log fields number"`
	ExpectLogTags     []string `mapstructure:"expect_log_tags" comment:"the expected log tags mapping"`
	ExpectTagNum      int      `mapstructure:"expect_tag_num" comment:"the expected log tags number"`
	ExpectLogCategory string   `mapstructure:"expect_log_category" comment:"the expected log category"`
	ExpectLogTopic    string   `mapstructure:"expect_log_topic" comment:"the expected log topic"`
	ExpectLogSource   string   `mapstructure:"expect_log_source" comment:"the expected log source"`
}

func (f *fieldsLogValidator) Name() string {
	return fieldsLogValidatorName
}

func (f *fieldsLogValidator) Description() string {
	return "this is a log field validator to check received log from subscriber"
}

func (f *fieldsLogValidator) Valid(group *protocol.LogGroup) (res []*Report) {
	if f.ExpectLogSource != "" && f.ExpectLogSource != group.Source {
		logger.Debugf(context.Background(), "want log source %s, bug got %s", f.ExpectLogSource, group.Source)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_source", Want: f.ExpectLogSource, Got: group.Source})
	}
	if f.ExpectLogCategory != "" && f.ExpectLogCategory != group.Category {
		logger.Debugf(context.Background(), "want log category %s, bug got %s", f.ExpectLogCategory, group.Category)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_category", Want: f.ExpectLogCategory, Got: group.Category})
	}
	if f.ExpectLogTopic != "" && f.ExpectLogTopic != group.Topic {
		logger.Debugf(context.Background(), "want log topic %s, bug got %s", f.ExpectLogTopic, group.Topic)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_topic", Want: f.ExpectLogTopic, Got: group.Topic})
	}
	if f.ExpectTagNum > 0 && len(group.LogTags) != f.ExpectTagNum {
		logger.Debugf(context.Background(), "want log tag num %d, bug got %d, got tags: %+v", f.ExpectTagNum, len(group.LogTags), group.LogTags)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_tag_num", Want: strconv.Itoa(f.ExpectTagNum), Got: strconv.Itoa(len(group.LogTags))})
	}
	if notFoundTags := f.checkTags(group.LogTags); len(notFoundTags) != 0 {
		logger.Debugf(context.Background(), "want log tags contain %+v, but not found", notFoundTags)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_tag", Want: strings.Join(f.ExpectLogTags, ","), NotFound: strings.Join(notFoundTags, ",")})
	}
	if f.ExpectLogFieldNum != 0 && len(group.Logs) == 0 {
		logger.Debugf(context.Background(), "want every log has %d fields, but not found any logs", f.ExpectLogFieldNum)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_field_num", Want: strconv.Itoa(f.ExpectLogFieldNum), Got: "0"})
	}
	for _, l := range group.Logs {
		if !f.checkLogFieldsNum(l) {
			logger.Debugf(context.Background(), "want log fields num %d, but got %d, the log is %s", f.ExpectLogFieldNum, len(l.Contents), l.String())
			res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_field_num", Want: strconv.Itoa(f.ExpectLogFieldNum), Got: strconv.Itoa(len(l.Contents))})
		}
		if notFoundFields := f.checkLogFields(l); len(notFoundFields) != 0 {
			logger.Debugf(context.Background(), "want log fields contain %+v, but not found", notFoundFields)
			res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_field", Want: strings.Join(f.ExpectLogFields, ","), NotFound: strings.Join(notFoundFields, ",")})
		}
	}
	return
}

func (f *fieldsLogValidator) checkTags(tags []*protocol.LogTag) (notFoundTags []string) {
	for _, exp := range f.ExpectLogTags {
		var exist bool
		for _, tag := range tags {
			if tag.Key == exp {
				exist = true
				break
			}
		}
		if !exist {
			notFoundTags = append(notFoundTags, exp)
		}
	}
	return
}

func (f *fieldsLogValidator) checkLogFields(log *protocol.Log) (notFoundFields []string) {
	for _, exp := range f.ExpectLogFields {
		var exist bool
		for _, content := range log.Contents {
			if content.Key == exp {
				exist = true
				break
			}
		}
		if !exist {
			notFoundFields = append(notFoundFields, exp)
		}
	}
	return
}

func (f *fieldsLogValidator) checkLogFieldsNum(log *protocol.Log) bool {
	if f.ExpectLogFieldNum <= 0 {
		return true
	}
	return f.ExpectLogFieldNum == len(log.Contents)
}

func init() {
	RegisterLogValidatorCreator(fieldsLogValidatorName, func(spec map[string]interface{}) (LogValidator, error) {
		f := new(fieldsLogValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		return f, nil
	})
	doc.Register("log_validator", fieldsLogValidatorName, new(fieldsLogValidator))
}
