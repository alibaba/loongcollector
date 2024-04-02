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

package pkg

import (
	"context"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/util"
)

const LogTailMeta LogtailMetaKey = "LogtailContextMeta"

// LogtailContextMeta is used to store metadata in Logtail context and would be
// propagated within context.Context.
type LogtailContextMeta struct {
	project      string
	logstore     string
	configName   string
	loggerHeader string
	alarm        *util.Alarm
}

type LogtailMetaKey string

// NewLogtailContextMeta create a LogtailContextMeta instance.
func NewLogtailContextMeta(project, logstore, configName string) (context.Context, *LogtailContextMeta) {
	meta := &LogtailContextMeta{
		project:    project,
		logstore:   logstore,
		configName: config.GetRealConfigName(configName),
		alarm:      new(util.Alarm),
	}
	if len(logstore) == 0 {
		meta.loggerHeader = "[" + configName + "]\t"
	} else {
		meta.loggerHeader = "[" + configName + "," + logstore + "]\t"
	}
	meta.alarm.Init(project, logstore)
	ctx := context.WithValue(context.Background(), LogTailMeta, meta)
	return ctx, meta
}

// NewLogtailContextMetaWithoutAlarm create a LogtailContextMeta without alarm instance.
func NewLogtailContextMetaWithoutAlarm(project, logstore, configName string) (context.Context, *LogtailContextMeta) {
	meta := &LogtailContextMeta{
		project:    project,
		logstore:   logstore,
		configName: configName,
	}
	if len(logstore) == 0 {
		meta.loggerHeader = "[" + configName + "]\t"
	} else {
		meta.loggerHeader = "[" + configName + "," + logstore + "]\t"
	}
	ctx := context.WithValue(context.Background(), LogTailMeta, meta)
	return ctx, meta
}

func (c *LogtailContextMeta) LoggerHeader() string {
	return c.loggerHeader
}

func (c *LogtailContextMeta) GetProject() string {
	return c.project
}

func (c *LogtailContextMeta) GetLogStore() string {
	return c.logstore
}
func (c *LogtailContextMeta) GetConfigName() string {
	return c.configName
}

func (c *LogtailContextMeta) GetAlarm() *util.Alarm {
	return c.alarm
}

func (c *LogtailContextMeta) RecordAlarm(alarmType, msg string) {
	if c.alarm == nil {
		return
	}
	c.alarm.Record(alarmType, msg)
}
