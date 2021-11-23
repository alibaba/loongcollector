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

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const counterSysValidatorName = "sys_counter"

type counterSystemValidator struct {
	ExpectReceivedMinimumLogNum      int  `mapstructure:"expect_received_minimum_log_num"  comment:"the expected minimum received log number"`
	ExpectReceivedMinimumLogGroupNum int  `mapstructure:"expect_received_minimum_log_group_num"  comment:"the expected minimum received log group number"`
	ExpectMinimumRawLogNum           int  `mapstructure:"expect_minimum_raw_log_num"   comment:"the expected minimum raw log number"`
	ExpectMinimumProcessedLogNum     int  `mapstructure:"expect_minimum_processed_log_num"  comment:"the expected minimum processed log number"`
	ExpectMinimumFlushLogNum         int  `mapstructure:"expect_minimum_flush_log_num"  comment:"the expected minimum flushed log number"`
	ExpectMinimumFlushLogGroupNum    int  `mapstructure:"expect_minimum_flush_log_group_num" comment:"the expected minimum flushed log group number"`
	ExpectEqualRawLog                bool `mapstructure:"expect_equal_raw_log" comment:"whether the received log number equal to the raw log number"`
	ExpectEqualProcessedLog          bool `mapstructure:"expect_equal_processed_log" comment:"whether the received log number equal to the processed log number"`
	ExpectEqualFlushLog              bool `mapstructure:"expect_equal_flush_log" comment:"whether the received log number equal to the flush log number"`

	groupCounter int
	logsCounter  int
}

func (c *counterSystemValidator) Description() string {
	return "this is a log field validator to check received log from subscriber"
}

func (c *counterSystemValidator) Name() string {
	return counterSysValidatorName
}

func (c *counterSystemValidator) Valid(group *protocol.LogGroup) {
	c.groupCounter++
	c.logsCounter += len(group.Logs)
}

func (c *counterSystemValidator) Start() error {
	return nil
}

func (c *counterSystemValidator) FetchResult() (res []*Report) {
	if c.ExpectReceivedMinimumLogGroupNum > c.groupCounter {
		logger.Debugf(context.Background(), "want log group number over %d, bug got %d", c.ExpectReceivedMinimumLogGroupNum, c.groupCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "loggroup_minimum_number", Want: strconv.Itoa(c.ExpectReceivedMinimumLogGroupNum), Got: strconv.Itoa(c.groupCounter)})
	}
	if c.ExpectReceivedMinimumLogNum > c.logsCounter {
		logger.Debugf(context.Background(), "want log number over %d, bug got %d", c.ExpectReceivedMinimumLogNum, c.logsCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "log_minimum_number", Want: strconv.Itoa(c.ExpectReceivedMinimumLogNum), Got: strconv.Itoa(c.logsCounter)})
	}
	if c.ExpectMinimumRawLogNum > RawLogCounter {
		logger.Debugf(context.Background(), "want raw log number over %d, bug got %d", c.ExpectMinimumRawLogNum, RawLogCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "raw_log_minimum_number", Want: strconv.Itoa(c.ExpectMinimumRawLogNum), Got: strconv.Itoa(RawLogCounter)})
	}
	if c.ExpectMinimumProcessedLogNum > ProcessedLogCounter {
		logger.Debugf(context.Background(), "want processed log number over %d, bug got %d", c.ExpectMinimumProcessedLogNum, ProcessedLogCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "processed_log_minimum_number", Want: strconv.Itoa(c.ExpectMinimumProcessedLogNum), Got: strconv.Itoa(ProcessedLogCounter)})
	}
	if c.ExpectMinimumFlushLogNum > FlushLogCounter {
		logger.Debugf(context.Background(), "want flushed log number over %d, bug got %d", c.ExpectMinimumFlushLogNum, FlushLogCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "flushed_log_minimum_number", Want: strconv.Itoa(c.ExpectMinimumFlushLogNum), Got: strconv.Itoa(FlushLogCounter)})
	}
	if c.ExpectMinimumFlushLogGroupNum > FlushLogGroupCounter {
		logger.Debugf(context.Background(), "want flushed log group  number over %d, bug got %d", c.ExpectMinimumFlushLogGroupNum, FlushLogGroupCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "flushed_log_group_minimum_number", Want: strconv.Itoa(c.ExpectMinimumFlushLogGroupNum), Got: strconv.Itoa(FlushLogGroupCounter)})
	}
	if c.ExpectEqualRawLog && c.logsCounter != RawLogCounter {
		logger.Debugf(context.Background(), "want got raw log number %d, bug got %d", RawLogCounter, c.logsCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "equal_raw_log", Want: strconv.Itoa(RawLogCounter), Got: strconv.Itoa(c.logsCounter)})
	}
	if c.ExpectEqualProcessedLog && c.logsCounter != ProcessedLogCounter {
		logger.Debugf(context.Background(), "want got processed log number %d, bug got %d", ProcessedLogCounter, c.logsCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "equal_processed_log", Want: strconv.Itoa(ProcessedLogCounter), Got: strconv.Itoa(c.logsCounter)})
	}
	if c.ExpectEqualFlushLog && c.logsCounter != FlushLogCounter {
		logger.Debugf(context.Background(), "want got flush log number %d, bug got %d", FlushLogCounter, c.logsCounter)
		res = append(res, &Report{Validator: counterSysValidatorName, Name: "equal_flush_log", Want: strconv.Itoa(FlushLogCounter), Got: strconv.Itoa(c.logsCounter)})
	}
	return
}

func init() {
	RegisterSystemValidatorCreator(counterSysValidatorName, func(spec map[string]interface{}) (SystemValidator, error) {
		f := new(counterSystemValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		return f, nil
	})
	doc.Register("sys_validator", counterSysValidatorName, new(counterSystemValidator))
}
