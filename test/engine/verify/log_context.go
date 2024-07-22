// Copyright 2024 iLogtail Authors
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
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/avast/retry-go/v4"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

type contextInfo struct {
	log     string
	packSeq int
	logSeq  int
}

func LogContext(ctx context.Context) (context.Context, error) {
	var from int32
	value := ctx.Value(config.StartTimeContextKey)
	if value != nil {
		from = value.(int32)
	} else {
		return ctx, fmt.Errorf("no start time")
	}

	// Get logs
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	var err error
	var groups []*protocol.LogGroup
	err = retry.Do(
		func() error {
			groups, err = subscriber.TestSubscriber.GetData(from)
			return err
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
	if err != nil {
		return ctx, err
	}

	if len(groups) == 0 {
		return ctx, fmt.Errorf("no log group")
	}
	if len(groups[0].Logs) == 0 {
		return ctx, fmt.Errorf("no log in log group")
	}

	contextInfoMap := make(map[string]*contextInfo)
	for _, group := range groups {

		var packID string
		found := true
		for _, tag := range group.LogTags {
			if tag.GetKey() == "__pack_id__" {
				packID = tag.GetValue()
				found = true
				break
			}
		}
		if !found {
			return ctx, fmt.Errorf("key: __pack_id__ not found in log tags")
		}

		packIDComponents := strings.Split(packID, "-")
		if len(packIDComponents) != 2 {
			return ctx, fmt.Errorf("pack id not valid: %s", packID)
		}
		ctxInfo, ok := contextInfoMap[packIDComponents[0]]
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
				return ctx, fmt.Errorf("key: content not found in log contents")
			}
		}

		packIDNo, err := strconv.ParseInt(packIDComponents[1], 16, 0)
		if err != nil {
			return ctx, fmt.Errorf("pack_id_no is not valid: %s", packIDComponents[1])
		}

		if int(packIDNo) != ctxInfo.packSeq+1 {
			return ctx, fmt.Errorf("expect the %dth pack for pack id prefix %s, got %d instead", ctxInfo.packSeq+1, packIDComponents[0], packIDNo)
		}

		for _, log := range group.Logs {
			contentFound, seqFound := false, false
			for _, logContent := range log.Contents {
				if logContent.Key == "content" {
					contentFound = true
					if logContent.Value != ctxInfo.log {
						return ctx, fmt.Errorf("expect content: %s, got: %s", ctxInfo.log, logContent.Value)
					}
				} else if logContent.Key == "no" {
					seqFound = true
					no, err := strconv.Atoi(logContent.Value)
					if err != nil {
						return ctx, fmt.Errorf("logSeqNo is not valid: %s", logContent.Value)
					}
					if no != ctxInfo.logSeq+1 {
						return ctx, fmt.Errorf("expect the %dth log for pack id prefix %s, got %d instead", ctxInfo.logSeq+1, packIDComponents[0], no)
					}
				}
			}
			if !contentFound {
				return ctx, fmt.Errorf("key: content not found in log contents")
			}
			if !seqFound {
				return ctx, fmt.Errorf("key: no not found in log contents")
			}
			ctxInfo.logSeq++
		}
		ctxInfo.packSeq++
		contextInfoMap[packIDComponents[0]] = ctxInfo
	}

	return ctx, nil
}
