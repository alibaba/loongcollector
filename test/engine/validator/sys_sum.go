// Copyright 2022 iLogtail Authors
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

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/mitchellh/mapstructure"
)

const sumSystemValidatorName = "sys_sum_checker"

type sumSystemValidator struct {
	Field                    string `mapstructure:"field"  comment:"group by field"`
	FilterKey                string `mapstructure:"filter_key"  comment:"where filter_key=filter_val"`
	FilterVal                string `mapstructure:"filter_val"  comment:"where  filter_key=filter_val"`
	ExpectReceivedMinimumNum int    `mapstructure:"expect_received_minimum_num"  comment:"the expected minimum received log group number"`

	sum int
}

func (s *sumSystemValidator) Description() string {
	return "system sum validator group by field"
}

func (s *sumSystemValidator) Start() error {
	return nil
}

func (s *sumSystemValidator) Valid(group *protocol.LogGroup) {
	for _, log := range group.Logs {
		var num int
		var err error
		equals := true
		for _, content := range log.Contents {
			if content.Key == s.Field {
				num, err = strconv.Atoi(content.Value)
				if err != nil {
					logger.Error(context.Background(), "ILLEGAL_FIELD", "the field must be number", content.Key)
					continue
				}
			}
			if s.FilterKey != "" && s.FilterKey == content.Key {
				equals = s.FilterVal == content.Value
			}
		}
		if equals {
			s.sum += num
		}

	}
}

func (s *sumSystemValidator) FetchResult() (reports []*Report) {
	logger.Debugf(context.Background(), "want field log sum number %d, got %d", s.ExpectReceivedMinimumNum, s.sum)
	if s.ExpectReceivedMinimumNum > s.sum {
		reports = append(reports, &Report{Validator: sumSystemValidatorName, Name: "log_field_sum_number", Want: strconv.Itoa(s.ExpectReceivedMinimumNum), Got: strconv.Itoa(s.sum)})
	}
	return
}

func (s *sumSystemValidator) Name() string {
	return sumSystemValidatorName
}

func init() {
	RegisterSystemValidatorCreator(sumSystemValidatorName, func(spec map[string]interface{}) (SystemValidator, error) {
		f := new(sumSystemValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		return f, nil
	})
	doc.Register("sys_validator", sumSystemValidatorName, new(sumSystemValidator))
}
