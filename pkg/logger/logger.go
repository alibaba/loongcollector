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
	"bufio"
	"context"
	"flag"
	"fmt"
	"io"
	"path"

	"os"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"

	"github.com/cihub/seelog"
)

// seelog template
const (
	asyncPattern = `
<seelog type="asynctimer" asyncinterval="500000" minlevel="%s" >
 <outputs formatid="common">
	 <rollingfile type="size" filename="%s%s" maxsize="20000000" maxrolls="10"/>
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
	 <rollingfile type="size" filename="%s%s" maxsize="20000000" maxrolls="10"/>
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
	retainFlag         bool
	levelFlag          string
	debugFlag          int32

	template          string
	once              sync.Once
	wait              sync.WaitGroup
	closeChan         chan struct{}
	closedCatchStdout bool
)

func InitLogger() {
	once.Do(func() {
		initNormalLogger()
	})
}

func InitTestLogger(options ...ConfigOption) {
	once.Do(func() {
		config.LoongcollectorGlobalConfig.LoongCollectorLogDir = "./"
		config.LoongcollectorGlobalConfig.LoongCollectorConfDir = "./"
		config.LoongcollectorGlobalConfig.LoongCollectorLogConfDir = "./"
		initTestLogger(options...)
		catchStandardOutput()
	})
}

// initNormalLogger extracted from Init method for unit test.
func initNormalLogger() {
	closeChan = make(chan struct{})
	remoteFlag = true
	for _, option := range defaultProductionOptions {
		option()
	}
	confDir := config.LoongcollectorGlobalConfig.LoongCollectorLogConfDir
	if _, err := os.Stat(confDir); os.IsNotExist(err) {
		_ = os.MkdirAll(confDir, os.ModePerm)
	}
	setLogConf(path.Join(confDir, "plugin_logger.xml"))
}

// initTestLogger extracted from Init method for unit test.
func initTestLogger(options ...ConfigOption) {
	closeChan = make(chan struct{})
	remoteFlag = false
	for _, option := range defaultTestOptions {
		option()
	}
	for _, option := range options {
		option()
	}
	setLogConf(path.Join(config.LoongcollectorGlobalConfig.LoongCollectorLogConfDir, "plugin_logger.xml"))
}

func Debug(ctx context.Context, kvPairs ...interface{}) {
	if !DebugFlag() {
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
	if !DebugFlag() {
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

func Warning(ctx context.Context, alarmType selfmonitor.AlarmType, kvPairs ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		kvPairs = append(kvPairs, "logstore", ltCtx.GetLogStore(), "config", ltCtx.GetConfigName())
	}
	msg := generateLog(kvPairs...)
	if ok {
		_ = logtailLogger.Warn(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelWaring, msg)
		}
	} else {
		_ = logtailLogger.Warn("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelWaring, msg)
		}
	}
}

func Warningf(ctx context.Context, alarmType selfmonitor.AlarmType, format string, params ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		format += "\tlogstore:%v\tconfig:%v"
		params = append(params, ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	msg := fmt.Sprintf(format, params...)
	if ok {
		_ = logtailLogger.Warn(ltCtx.LoggerHeader(), "AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelWaring, msg)
		}
	} else {
		_ = logtailLogger.Warn("AlarmType:", alarmType, "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelWaring, msg)
		}
	}
}

func Error(ctx context.Context, alarmType selfmonitor.AlarmType, kvPairs ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		kvPairs = append(kvPairs, "logstore", ltCtx.GetLogStore(), "config", ltCtx.GetConfigName())
	}
	msg := generateLog(kvPairs...)
	if ok {
		_ = logtailLogger.Error(ltCtx.LoggerHeader(), "AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelError, msg)
		}
	} else {
		_ = logtailLogger.Error("AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelError, msg)
		}
	}
}

func Errorf(ctx context.Context, alarmType selfmonitor.AlarmType, format string, params ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		format += "\tlogstore:%v\tconfig:%v"
		params = append(params, ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	msg := fmt.Sprintf(format, params...)
	if ok {
		_ = logtailLogger.Error(ltCtx.LoggerHeader(), "AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelError, msg)
		}
	} else {
		_ = logtailLogger.Error("AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelError, msg)
		}
	}
}

func Critical(ctx context.Context, alarmType selfmonitor.AlarmType, kvPairs ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		kvPairs = append(kvPairs, "logstore", ltCtx.GetLogStore(), "config", ltCtx.GetConfigName())
	}
	msg := generateLog(kvPairs...)
	if ok {
		_ = logtailLogger.Critical(ltCtx.LoggerHeader(), "AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelCritical, msg)
		}
	} else {
		_ = logtailLogger.Critical("AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelCritical, msg)
		}
	}
}

func Criticalf(ctx context.Context, alarmType selfmonitor.AlarmType, format string, params ...interface{}) {
	ltCtx, ok := ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		format += "\tlogstore:%v\tconfig:%v"
		params = append(params, ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	msg := fmt.Sprintf(format, params...)
	if ok {
		_ = logtailLogger.Critical(ltCtx.LoggerHeader(), "AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			ltCtx.RecordAlarm(alarmType, selfmonitor.AlarmLevelCritical, msg)
		}
	} else {
		_ = logtailLogger.Critical("AlarmType:", alarmType.String(), "\t", msg)
		if remoteFlag {
			selfmonitor.GlobalAlarm.Record(alarmType, selfmonitor.AlarmLevelCritical, msg)
		}
	}
}

// Flush logs to the output when using async logger.
func Flush() {
	logtailLogger.Flush()
}

func setLogConf(logConfig string) {
	if !retainFlag {
		_ = os.Remove(path.Join(config.LoongcollectorGlobalConfig.LoongCollectorLogConfDir, "plugin_logger.xml"))
	}
	debugFlag = 0
	logtailLogger = seelog.Disabled
	path := filepath.Clean(logConfig)
	if _, err := os.Stat(path); err != nil {
		logConfigContent := generateDefaultConfig()
		_ = os.WriteFile(path, []byte(logConfigContent), os.ModePerm)
	}
	fmt.Fprintf(os.Stderr, "load log config %s \n", path)
	content, err := os.ReadFile(path)
	if err != nil {
		fmt.Fprintln(os.Stderr, "init logger error", err)
		return
	}
	dat := string(content)
	aliyunLogtailLogLevel := strings.ToLower(os.Getenv("LOGTAIL_LOG_LEVEL"))
	if aliyunLogtailLogLevel != "" {
		pattern := `(?mi)(minlevel=")([^"]*)(")`
		regExp := regexp.MustCompile(pattern)
		dat = regExp.ReplaceAllString(dat, `${1}`+aliyunLogtailLogLevel+`${3}`)
	}
	logger, err := seelog.LoggerFromConfigAsString(dat)
	if err != nil {
		fmt.Fprintln(os.Stderr, "init logger error", err)
		return
	}
	if err := logger.SetAdditionalStackDepth(1); err != nil {
		fmt.Fprintf(os.Stderr, "cannot set logger stack depth: %v\n", err)
		return
	}
	logtailLogger = logger

	if aliyunLogtailLogLevel == "debug" || strings.Contains(dat, "minlevel=\"debug\"") {
		debugFlag = 1
	}
}

func generateLog(kvPairs ...interface{}) string {
	logString := ""
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
	return fmt.Sprintf(template, levelFlag, config.LoongcollectorGlobalConfig.LoongCollectorLogDir, config.LoongcollectorGlobalConfig.LoongCollectorPluginLogName, consoleStr, memoryReceiverFlagStr)
}

// Close the logger and recover the stdout and stderr
func Close() {
	logtailLogger.Close()
}

// CloseCatchStdout close the goroutine with the catching stdout task.
func CloseCatchStdout() {
	if consoleFlag || closedCatchStdout {
		return
	}
	close(closeChan)
	wait.Wait()
	closedCatchStdout = true
}

// catchStandardOutput catch the stdout and stderr to the ilogtail logger.
func catchStandardOutput() {
	// do not open standard output catcher when console output is opening.
	if consoleFlag {
		return
	}
	catch := func(catcher func(*os.File) (old *os.File), logger func(text []byte), recover func(old *os.File)) error {
		r, w, err := os.Pipe()
		if err != nil {
			return err
		}
		old := catcher(w)
		wait.Add(1)
		go func() {
			defer wait.Done()
			reader := bufio.NewReader(r)
			go func() {
				<-closeChan
				_ = w.Close()
				_ = r.Close()
				recover(old)
			}()
			for {
				line, errRead := reader.ReadBytes('\n')
				if errRead == io.EOF {
					time.Sleep(100 * time.Millisecond)
					continue
				} else if errRead != nil {
					Error(context.Background(), "CATCH_STANDARD_OUTPUT_ALARM", "err", errRead)
					break
				}
				logger(line)
			}
		}()
		return nil
	}
	err := catch(
		func(w *os.File) (old *os.File) {
			old = os.Stdout
			os.Stdout = w
			return
		},
		func(text []byte) {
			Info(context.Background(), "stdout", string(text))
		},
		func(old *os.File) {
			os.Stdout = old
			_, _ = fmt.Fprint(os.Stdout, "recover stdout\n")
		})
	if err != nil {
		Error(context.Background(), "INIT_CATCH_STDOUT_ALARM", "err", err)
		return
	}
	err = catch(
		func(w *os.File) (old *os.File) {
			old = os.Stderr
			os.Stderr = w
			return
		},
		func(text []byte) {
			Error(context.Background(), "STDERR_ALARM", "stderr", string(text))
		},
		func(old *os.File) {
			os.Stderr = old
			_, _ = fmt.Fprint(os.Stderr, "recover stderr\n")
		})
	if err != nil {
		Error(context.Background(), "INIT_CATCH_STDERR_ALARM", "err", err)
	}
}

// DebugFlag returns true when debug level is opening.
func DebugFlag() bool {
	return atomic.LoadInt32(&debugFlag) == 1
}
