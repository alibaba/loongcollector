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

package helper

import (
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/go-kit/kit/log"
	"github.com/go-kit/kit/log/level"
)

type goKitLogWrapper struct {
	context   pipeline.Context
	alarmType string
}

// NewGoKitLogWrapper returns a logger that log with context.
func NewGoKitLogWrapper(context pipeline.Context, alarmType string) log.Logger {
	logger := &goKitLogWrapper{
		context:   context,
		alarmType: alarmType,
	}
	return log.With(level.NewFilter(logger, level.AllowAll()), "caller", log.DefaultCaller())
}

func (g *goKitLogWrapper) Log(params ...interface{}) error {
	for i := 0; i < len(params); i += 2 {
		if params[i].(string) == "level" {
			levelStr := fmt.Sprintf("%v", params[i+1])
			if levelStr == "warn" || levelStr == "error" {
				break
			}
			if levelStr == "info" {
				logger.Info(g.context.GetRuntimeContext(), params...)
			} else {
				logger.Debug(g.context.GetRuntimeContext(), params...)
			}
			return nil
		}
	}
	logger.Warning(g.context.GetRuntimeContext(), g.alarmType, params...)
	return nil
}
