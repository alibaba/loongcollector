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
	"regexp"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const fieldsKVLogValidatorName = "log_content_kv_regex"

type fieldsKVLogValidator struct {
	ExpectKeyValues map[string]string `mapstructure:"expect_key_value_regex" comment:"expected got key values"`

	kvRegexps map[string]*regexp.Regexp
}

func (f *fieldsKVLogValidator) Description() string {
	return "this is a log content key value validator to check received log from subscriber"
}

func (f *fieldsKVLogValidator) Valid(group *protocol.LogGroup) (reports []*Report) {
	for _, log := range group.Logs {
		for k, reg := range f.kvRegexps {
			for _, content := range log.Contents {
				if content.Key == k {
					if !reg.MatchString(content.Value) {
						logger.Debugf(context.Background(), "want contains KV %s:%s, but got %s:%s", k, reg.String(), content.Key, content.Value)
						reports = append(reports, &Report{Validator: fieldsKVLogValidatorName, Name: "kv_equal", Want: k + ":" + reg.String(),
							Got: content.Key + ":" + content.Value})
					}
					goto find
				}

			}
			logger.Debugf(context.Background(), "want contains KV %s:%s, but not found")
			reports = append(reports, &Report{Validator: fieldsKVLogValidatorName, Name: "kv_equal", Want: k + ":" + reg.String(),
				NotFound: k + ":" + reg.String()})
		find:
		}
	}
	return
}

func (f *fieldsKVLogValidator) Name() string {
	return fieldsKVLogValidatorName
}

func init() {
	RegisterLogValidatorCreator(fieldsKVLogValidatorName, func(spec map[string]interface{}) (LogValidator, error) {
		f := new(fieldsKVLogValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		f.kvRegexps = make(map[string]*regexp.Regexp)
		for k, v := range f.ExpectKeyValues {
			reg, err := regexp.Compile(v)
			if err != nil {
				return nil, err
			}
			f.kvRegexps[k] = reg
		}
		return f, nil
	})

	doc.Register("log_validator", fieldsKVLogValidatorName, new(fieldsKVLogValidator))
}
