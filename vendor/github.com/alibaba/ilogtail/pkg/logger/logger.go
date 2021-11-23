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
	"context"
	"flag"
	"fmt"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/cihub/seelog"
)

// seelog template
const (
	asyncPattern = `
<seelog type="asynctimer" asyncinterval="500000" minlevel="%s" >
 <outputs formatid="common">
	 <rollingfile type="size" filename="%slogtail_plugin.LOG" maxsize="2097152" maxrolls="10"/>
	 %s
     %s
 </outputs>
 <formats>
	 <format id="common" format="%%Date %%Time [%%LEV] [%%File:%%Line] [%%FuncShort] %%Msg%%n" />
 </formats>
</seelog>
`
	syncPattern = `
<seelog type="sync" minlevel="%s" >
 <outputs formatid="common">
	 <rollingfile type="size" filename="%slogtail_plugin.LOG" maxsize="2097152" maxrolls="10"/>
	 %s
	 %s
 </outputs>
 <formats>
	 <format id="common" format="%%Date %%Time [%%LEV] [%%File:%%Line] [%%FuncShort] %%Msg%%n" />
 </formats>
</seelog>
`
)

const (
	FlagLevelName   = "logger-level"
	FlagConsoleName = "logger-console"
	FlagRetainName  = "logger-retain"
)

// logtailLogger is a global logger instance, which is shared with the whole plugins of LogtailPlugin.
// When having LogtailContextMeta of LogtailPlugin in the context.Context, the meta header would be appended to
// the log. The reason why we don't use formatter is we want to use on logger instance to control the memory
// cost of the logging operation and avoid adding a lock. Also, when the log level is greater than or equal
// to warn, these logs will be sent to the back-end service for self-telemetry.
var logtailLogger = seelog.Disabled

// Flags is only used in testing because LogtailPlugin is trigger by C rather than pure Go.
var (
	loggerLevel   = flag.String(FlagLevelName, "", "debug flag")
	loggerConsole = flag.String(FlagConsoleName, "", "debug flag")
	loggerRetain  = flag.String(FlagRetainName, "", "debug flag")
)

var (
	memoryReceiverFlag bool
	consoleFlag        bool
	remoteFlag         bool
	DebugFlag          bool
	retainFlag         bool
	levelFlag          string

	template string
	once     sync.Once
)

func Init() {
	once.Do(func() {
		initNormalLogger()
	})
}

func InitTestLogger(options ...ConfigOption) {
	once.Do(func() {
		initTestLogger(options...)
	})
}

// initNormalLogger extracted from Init method for unit test.
func initNormalLogger() {
	remoteFlag = true
	for _, option := range defaultProductionOptions {
		option()
	}
	setLogConf(util.GetCurrentBinaryPath() + "plugin_logger.xml")
}

// initTestLogger extracted from Init method for unit test.
func initTestLogger(options ...ConfigOption) {
	remoteFlag = false
	for _, option := range defaultTestOptions {
		option()
	}
	for _, option := range options {
		option()
	}
	setLogConf(util.GetCurrentBinaryPath() + "plugin_logger.xml")
}

func Debug(ctx context.Context, kvPairs ...interface{}) {
	if !DebugFlag {
		return
	}
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		logtailLogger.Debug(ltCtx.LoggerHeader(), generateLog(kvPairs...))
	} else {
		logtailLogger.Debug(generateLog(kvPairs...))
	}
}

func Debugf(ctx context.Context, format string, params ...interface{}) {
	if !DebugFlag {
		return
	}
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		logtailLogger.Debugf(ltCtx.LoggerHeader()+format, params...)
	} else {
		logtailLogger.Debugf(format, params...)
	}
}

func Info(ctx context.Context, kvPairs ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		logtailLogger.Info(ltCtx.LoggerHeader(), generateLog(kvPairs...))
	} else {
		logtailLogger.Info(generateLog(kvPairs...))
	}
}

func Infof(ctx context.Context, format string, params ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		logtailLogger.Infof(ltCtx.LoggerHeader()+format, params...)
	} else {
		logtailLogger.Infof(format, params...)
	}
}

func Warning(ctx context.Context, alarmType string, kvPairs ...interface{}) {
	msg := generateLog(kvPairs...)
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		_ = logtailLogger.Warn(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, msg)
		}
	} else {
		_ = logtailLogger.Warn("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			util.GlobalAlarm.Record(alarmType, msg)
		}
	}
}

func Warningf(ctx context.Context, alarmType string, format string, params ...interface{}) {
	msg := fmt.Sprintf(format, params...)
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		_ = logtailLogger.Warn(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, msg)
		}
	} else {
		_ = logtailLogger.Warn("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			util.GlobalAlarm.Record(alarmType, msg)
		}
	}
}

func Error(ctx context.Context, alarmType string, kvPairs ...interface{}) {
	msg := generateLog(kvPairs...)
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		_ = logtailLogger.Error(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, msg)
		}
	} else {
		_ = logtailLogger.Error("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			util.GlobalAlarm.Record(alarmType, msg)
		}
	}
}

func Errorf(ctx context.Context, alarmType string, format string, params ...interface{}) {
	msg := fmt.Sprintf(format, params...)
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		_ = logtailLogger.Error(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, msg)
		}
	} else {
		_ = logtailLogger.Error("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			util.GlobalAlarm.Record(alarmType, msg)
		}
	}
}

// Flush logs to the output when using async logger.
func Flush() {
	logtailLogger.Flush()
}

func setLogConf(logConfig string) {
	if !retainFlag {
		_ = os.Remove(util.GetCurrentBinaryPath() + "plugin_logger.xml")
	}
	DebugFlag = false
	logtailLogger = seelog.Disabled
	path := filepath.Clean(logConfig)
	if _, err := os.Stat(path); err != nil {
		logConfigContent := generateDefaultConfig()
		_ = ioutil.WriteFile(path, []byte(logConfigContent), os.ModePerm)
	}
	fmt.Printf("load log config %s \n", path)
	logger, err := seelog.LoggerFromConfigAsFile(path)
	if err != nil {
		fmt.Println("init logger error", err)
		return
	}
	if err := logger.SetAdditionalStackDepth(1); err != nil {
		fmt.Printf("cannot set logger stack depth: %v\n", err)
		return
	}
	logtailLogger = logger
	dat, _ := ioutil.ReadFile(path)
	DebugFlag = strings.Contains(string(dat), "minlevel=\"debug\"")
}

func generateLog(kvPairs ...interface{}) string {
	var logString = ""
	pairLen := len(kvPairs) / 2
	for i := 0; i < pairLen; i++ {
		logString += fmt.Sprintf("%v:%v\t", kvPairs[i<<1], kvPairs[i<<1+1])
	}
	if len(kvPairs)&0x01 != 0 {
		logString += fmt.Sprintf("%v:\t", kvPairs[len(kvPairs)-1])
	}
	return logString
}

func generateDefaultConfig() string {
	consoleStr := ""
	if consoleFlag {
		consoleStr = "<console/>"
	}
	memoryReceiverFlagStr := ""
	if memoryReceiverFlag {
		memoryReceiverFlagStr = "<custom name=\"memory\" />"
	}
	return fmt.Sprintf(template, levelFlag, util.GetCurrentBinaryPath(), consoleStr, memoryReceiverFlagStr)
}
