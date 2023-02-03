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

//go:build windows
// +build windows

package eventlog

import (
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// EventLogConfig represents the config of EventLog.
type EventLogConfig struct {
	Context     pipeline.Context
	Name        string
	IgnoreOlder time.Duration
	EventID     string
	Level       string
	Provider    []string
}

type creator func(config EventLogConfig) (EventLog, error)

var creators = map[string]creator{}

func Register(name string, c creator) {
	creators[name] = c
}

func NewEventLog(config EventLogConfig) (EventLog, error) {
	if "" == config.Name {
		return nil, fmt.Errorf("Name can not be empty in config")
	}

	if len(creators) == 0 {
		return nil, fmt.Errorf("No Windows event log API available.")
	}

	// Prefer wineventlog to eventlogging.
	var c creator
	if winEventLog, exist := creators[winEventLogAPIName]; exist {
		c = winEventLog
		logger.Infof(config.Context.GetRuntimeContext(), "Use API %v to collect event log", winEventLogAPIName)
	} else {
		c = creators[eventLoggingAPIName]
		logger.Infof(config.Context.GetRuntimeContext(), "Use API %v to collect event log", eventLoggingAPIName)
	}
	return c(config)
}
