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
	"fmt"
	"regexp"
	"strconv"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const alarmLogSystemValidatorName = "sys_alarm_checker"

type alarmLogSystemValidator struct {
	Project            string `mapstructure:"project" comment:"the alarm project" `
	Logstore           string `mapstructure:"logstore" comment:"the alarm logstore"`
	AlarmType          string `mapstructure:"alarm_type" comment:"the alarm type"`
	AlarmMsgRegexp     string `mapstructure:"alarm_msg_regexp" comment:"the alarm msg regexp"`
	ExpectMinimumCount int    `mapstructure:"expect_minimum_count" comment:"the alarm minimum count"`

	reg *regexp.Regexp
}

func (c *alarmLogSystemValidator) Description() string {
	return "this is a alarm log validator to check received alarm log from subscriber"
}

func (c *alarmLogSystemValidator) Name() string {
	return counterSysValidatorName
}

func (c *alarmLogSystemValidator) Valid(group *protocol.LogGroup) {
}

func (c *alarmLogSystemValidator) Start() error {
	reg, err := regexp.Compile(c.AlarmMsgRegexp)
	if err != nil {
		return err
	}
	c.reg = reg
	return nil
}

func (c *alarmLogSystemValidator) FetchResult() (res []*Report) {
	key := fmt.Sprintf("project_%s:logstore_%s", c.Project, c.Logstore)

	if _, ok := AlarmLogs[key]; !ok && c.ExpectMinimumCount > 0 {
		logger.Debugf(context.Background(), "want %s has alarm logs, but not found", key)
		res = append(res, &Report{Validator: alarmLogSystemValidatorName, Name: "alarm_log_project_logstore", Want: key, NotFound: key})
		return
	}

	if _, ok := AlarmLogs[key][c.AlarmType]; !ok && c.ExpectMinimumCount > 0 {
		logger.Debugf(context.Background(), "want %s has %s alarm logs, but not found", key, c.AlarmType)
		res = append(res, &Report{Validator: alarmLogSystemValidatorName, Name: "alarm_log_type", Want: c.AlarmType, NotFound: c.AlarmType})
		return
	}

	for log, count := range AlarmLogs[key][c.AlarmType] {
		if c.reg.MatchString(log) && c.ExpectMinimumCount > count {
			logger.Debugf(context.Background(), "want %s has %d %s alarm logs, but got %d", key, c.ExpectMinimumCount, c.AlarmType, count)
			res = append(res, &Report{Validator: alarmLogSystemValidatorName, Name: "alarm_log_num", Want: strconv.Itoa(c.ExpectMinimumCount), Got: strconv.Itoa(count)})
			return
		}
	}
	return
}

func init() {
	RegisterSystemValidatorCreator(alarmLogSystemValidatorName, func(spec map[string]interface{}) (SystemValidator, error) {
		f := new(alarmLogSystemValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		return f, nil
	})
	doc.Register("sys_validator", alarmLogSystemValidatorName, new(alarmLogSystemValidator))
}
