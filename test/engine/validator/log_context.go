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
	"strconv"
	"strings"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const contextLogValidatorName = "log_context"

type contextInfo struct {
	log     string
	packSeq int
	logSeq  int
}

type contextLogValidator struct {
	contextInfoMap map[string]*contextInfo
}

func (c *contextLogValidator) Name() string {
	return fieldsLogValidatorName
}

func (c *contextLogValidator) Description() string {
	return "this is a log context validator to check if all logs in a single logGroup have the same origin"
}

func (c *contextLogValidator) Valid(group *protocol.LogGroup) (res []*Report) {
	if len(group.Logs) == 0 {
		logger.Debugf(context.Background(), "no log in log group")
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_num", Want: "> 0", Got: "0"})
		return
	}

	var packID string
	found := false
	for _, tag := range group.LogTags {
		if tag.GetKey() == "__pack_id__" {
			packID = tag.GetValue()
			found = true
			break
		}
	}
	if !found {
		logger.Debugf(context.Background(), "key: __pack_id__ not found in log tags")
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_group_tag", Want: "__pack_id__", Got: ""})
		return
	}

	packIDComponents := strings.Split(packID, "-")
	if len(packIDComponents) != 2 {
		logger.Debugf(context.Background(), "pack id not valid: %s", packID)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "pack_id", Want: "<prefix>-<no>", Got: packID})
		return
	}

	ctxInfo, ok := c.contextInfoMap[packIDComponents[0]]
	if !ok {
		ctxInfo = &contextInfo{
			packSeq: 0,
			logSeq:  0,
		}

		found := false
		for _, logContent := range group.Logs[0].Contents {
			if logContent.Key == "content" {
				ctxInfo.log = logContent.Value
				found = true
				break
			}
		}
		if !found {
			logger.Debugf(context.Background(), "key: content not found in log contents")
			res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_content_key", Want: "content", Got: ""})
			return
		}
	}

	packIDNo, err := strconv.ParseInt(packIDComponents[1], 16, 0)
	if err != nil {
		logger.Debugf(context.Background(), "packIdNo is not valid: %s", packIDNo)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "pack_id_no", Want: "number", Got: packIDComponents[1]})
		return
	}

	if int(packIDNo) != ctxInfo.packSeq+1 {
		logger.Debugf(context.Background(), "expect the %dth pack for pack id prefix %s, got %d instead", ctxInfo.packSeq+1, packIDComponents[0], packIDNo)
		res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "pack_id_no", Want: fmt.Sprintf("%d", ctxInfo.packSeq+1), Got: packIDComponents[1]})
		return
	}

	for _, log := range group.Logs {
		contentFound, seqFound := false, false
		for _, logContent := range log.Contents {
			if logContent.Key == "content" {
				contentFound = true
				if logContent.Value != ctxInfo.log {
					logger.Debugf(context.Background(), "expect content: %s, got: %s", ctxInfo.log, logContent.Value)
					res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_content", Want: ctxInfo.log, Got: logContent.Value})
					return
				}
			} else if logContent.Key == "no" {
				seqFound = true
				no, err := strconv.Atoi(logContent.Value)
				if err != nil {
					logger.Debugf(context.Background(), "logSeqNo is not valid: %s", logContent.Value)
					res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "pack_id_no", Want: "number", Got: logContent.Value})
					return
				}
				if no != ctxInfo.logSeq+1 {
					logger.Debugf(context.Background(), "expect the %dth log for pack id prefix %s, got %d instead", ctxInfo.logSeq+1, packIDComponents[0], no)
					res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_no", Want: fmt.Sprintf("%d", ctxInfo.logSeq+1), Got: logContent.Value})
					return
				}
			}
		}
		if !contentFound {
			logger.Debugf(context.Background(), "key: content not found in log contents")
			res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_content_key", Want: "content", Got: ""})
			return
		}
		if !seqFound {
			logger.Debugf(context.Background(), "key: no not found in log contents")
			res = append(res, &Report{Validator: fieldsLogValidatorName, Name: "log_content_key", Want: "no", Got: ""})
			return
		}
		ctxInfo.logSeq++
	}
	ctxInfo.packSeq++
	c.contextInfoMap[packIDComponents[0]] = ctxInfo

	return
}

func init() {
	RegisterLogValidatorCreator(contextLogValidatorName, func(spec map[string]interface{}) (LogValidator, error) {
		f := new(contextLogValidator)
		err := mapstructure.Decode(spec, f)
		if err != nil {
			return nil, err
		}
		f.contextInfoMap = make(map[string]*contextInfo)
		return f, nil
	})
	doc.Register("log_validator", contextLogValidatorName, new(contextLogValidator))
}
