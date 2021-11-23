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

package logger

import (
	"github.com/cihub/seelog"

	"strconv"
)

var defaultProductionOptions = []ConfigOption{
	OptionOffConsole,
	OptionAsyncLogger,
	OptionInfoLevel,
	OptionRetainConfig,
	OptionOffMemoryReceiver,
}

var defaultTestOptions = []ConfigOption{
	OptionOpenConsole,
	OptionSyncLogger,
	OptionInfoLevel,
	OptionOverrideConfig,
	OptionOffMemoryReceiver,
}

type ConfigOption func()

func OptionOffConsole() {
	changeConsoleFlag(false)

}
func OptionOpenConsole() {
	changeConsoleFlag(true)
}
func OptionOffMemoryReceiver() {
	memoryReceiverFlag = false
}
func OptionOpenMemoryReceiver() {
	memoryReceiverFlag = true
}
func OptionSyncLogger() {
	changeSyncFlag(true)
}
func OptionAsyncLogger() {
	changeSyncFlag(false)
}
func OptionDebugLevel() {
	changeLevelFlag(seelog.DebugStr)
}
func OptionInfoLevel() {
	changeLevelFlag(seelog.InfoStr)
}
func OptionWarnLevel() {
	changeLevelFlag(seelog.WarnStr)
}
func OptionErrorLevel() {
	changeLevelFlag(seelog.ErrorStr)
}

func OptionOverrideConfig() {
	changeReatainFlag(false)
}

func OptionRetainConfig() {
	changeReatainFlag(true)
}

func changeSyncFlag(sync bool) {
	if sync {
		template = syncPattern
	} else {
		template = asyncPattern
	}
}

func changeConsoleFlag(console bool) {
	consoleFlag = console
	if *loggerConsole != "" {
		if c, err := strconv.ParseBool(*loggerConsole); err == nil {
			consoleFlag = c
		}
	}
}

func changeLevelFlag(level string) {
	levelFlag = level
	if *loggerLevel != "" {
		if _, found := seelog.LogLevelFromString(*loggerLevel); found {
			levelFlag = *loggerLevel
		}
	}
}

func changeReatainFlag(retain bool) {
	retainFlag = retain
	if *loggerRetain != "" {
		if c, err := strconv.ParseBool(*loggerRetain); err == nil {
			retainFlag = c
		}
	}
}
