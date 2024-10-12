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
	"os"
	"path"
	"regexp"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/cihub/seelog"
	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	testProject    = "mock-project"
	testLogstore   = "mock-logstore"
	testConfigName = "mock-configname"
)

var ctx context.Context
var mu sync.Mutex

func init() {
	ctx, _ = pkg.NewLogtailContextMeta(testProject, testLogstore, testConfigName)
}

func clean() {
	_ = os.Remove(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorConfDir, "plugin_logger.xml"))
	_ = os.Remove(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "go_plugin.LOG"))
}

func readLog(index int) string {
	bytes, _ := os.ReadFile(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "go_plugin.LOG"))
	logs := strings.Split(string(bytes), "\n")
	if index > len(logs)-1 {
		return ""
	}
	return logs[index]
}

func Test_generateLog(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	type args struct {
		kvPairs []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "empty",
			args: args{},
			want: "",
		},
		{
			name: "odd number",
			args: args{kvPairs: []interface{}{"a", "b", "c"}},
			want: "a:b\tc:\t",
		},
		{
			name: "even number",
			args: args{kvPairs: []interface{}{"a", "b", "c", "d"}},
			want: "a:b\tc:d\t",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			if got := generateLog(tt.args.kvPairs...); got != tt.want {
				t.Errorf("generateLog() = %v, want %v", got, tt.want)
			}
		})
	}
}

func Test_generateDefaultConfig(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	template = asyncPattern
	tests := []struct {
		name       string
		want       string
		flagSetter func()
	}{
		{
			name:       "production",
			want:       fmt.Sprintf(template, "info", config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "", ""),
			flagSetter: func() {},
		},
		{
			name: "test-debug-level",
			want: fmt.Sprintf(template, "debug", config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "", ""),
			flagSetter: func() {
				flag.Set(FlagLevelName, "debug")
			},
		},
		{
			name: "test-wrong-level",
			want: fmt.Sprintf(template, "info", config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "", ""),
			flagSetter: func() {
				flag.Set(FlagLevelName, "debug111")
			},
		},
		{
			name: "test-open-console",
			want: fmt.Sprintf(template, "info", config.LoongcollectorGlobalConfig.LoongcollectorLogDir, "<console/>", ""),
			flagSetter: func() {
				flag.Set(FlagConsoleName, "true")
			},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			tt.flagSetter()
			clean()
			initNormalLogger()
			if got := generateDefaultConfig(); got != tt.want {
				t.Errorf("generateDefaultConfig() = %v, want %v", got, tt.want)
			}
			flag.Set(FlagConsoleName, "false")
			flag.Set(FlagLevelName, "info")
		})
	}
}

func TestDebug(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	flag.Set(FlagLevelName, seelog.DebugStr)
	clean()
	initNormalLogger()
	type args struct {
		ctx     context.Context
		kvPairs []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "with-header",
			args: args{
				ctx:     ctx,
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\t\\[a b\\]:.*",
		},
		{
			name: "without-header",
			args: args{
				ctx:     context.Background(),
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[a b\\]:.*",
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Debug(tt.args.ctx, tt.args.kvPairs)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
		})
	}
}

func TestLogLevelFromEnv(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	clean()
	os.Setenv("LOGTAIL_LOG_LEVEL", "debug")
	initNormalLogger()
	os.Unsetenv("LOGTAIL_LOG_LEVEL")
	type args struct {
		ctx     context.Context
		kvPairs []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "with-header",
			args: args{
				ctx:     ctx,
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\t\\[a b\\]:.*",
		},
		{
			name: "without-header",
			args: args{
				ctx:     context.Background(),
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[a b\\]:.*",
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Debug(tt.args.ctx, tt.args.kvPairs)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
		})
	}
}

func TestDebugf(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	flag.Set(FlagLevelName, seelog.DebugStr)
	clean()
	initNormalLogger()
	type args struct {
		ctx    context.Context
		format string
		params []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "without-header",
			args: args{
				ctx:    context.Background(),
				format: "test %s",
				params: []interface{}{"a"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] test \\[a\\].*",
		},
		{
			name: "with-header",
			args: args{
				ctx:    ctx,
				format: "test %s",
				params: []interface{}{"a"},
			},
			want: ".*\\[DBG\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\ttest \\[a\\].*",
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Debugf(tt.args.ctx, tt.args.format, tt.args.params)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
		})
	}
}

func TestInfo(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	clean()
	initNormalLogger()
	type args struct {
		ctx     context.Context
		kvPairs []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "with-header",
			args: args{
				ctx:     ctx,
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[INF\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\t\\[a b\\]:.*",
		},
		{
			name: "without-header",
			args: args{
				ctx:     context.Background(),
				kvPairs: []interface{}{"a", "b"},
			},
			want: ".*\\[INF\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[a b\\]:.*",
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Info(tt.args.ctx, tt.args.kvPairs)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
		})
	}
}

func TestInfof(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	flag.Set(FlagLevelName, seelog.DebugStr)
	clean()
	initNormalLogger()
	type args struct {
		ctx    context.Context
		format string
		params []interface{}
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "without-header",
			args: args{
				ctx:    context.Background(),
				format: "test %s",
				params: []interface{}{"a"},
			},
			want: ".*\\[INF\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] test \\[a\\].*",
		},
		{
			name: "with-header",
			args: args{
				ctx:    ctx,
				format: "test %s",
				params: []interface{}{"a"},
			},
			want: ".*\\[INF\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\ttest \\[a\\].*",
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Infof(tt.args.ctx, tt.args.format, tt.args.params)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
		})
	}
}

func TestWarn(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	clean()
	initNormalLogger()
	type args struct {
		ctx       context.Context
		alarmType string
		kvPairs   []interface{}
	}
	tests := []struct {
		name   string
		args   args
		want   string
		getter func() *util.Alarm
	}{
		{
			name: "with-header",
			args: args{
				ctx:       ctx,
				alarmType: "WITH_HEADER",
				kvPairs:   []interface{}{"a", "b"},
			},
			want: ".*\\[WRN\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\tAlarmType:WITH_HEADER\t\\[a b\\]:.*",
			getter: func() *util.Alarm {
				return ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm()
			},
		},
		{
			name: "without-header",
			args: args{
				ctx:       context.Background(),
				alarmType: "WITHOUT_HEADER",
				kvPairs:   []interface{}{"a", "b"},
			},
			want: ".*\\[WRN\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] AlarmType:WITHOUT_HEADER\t\\[a b\\]:.*",
			getter: func() *util.Alarm {
				return util.GlobalAlarm
			},
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Warning(tt.args.ctx, tt.args.alarmType, tt.args.kvPairs)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
			alarmMap := tt.getter().AlarmMap
			assert.Equal(t, 1, len(alarmMap), "the size of alarm map; %d", len(alarmMap))
			for at, item := range alarmMap {
				assert.Equal(t, tt.args.alarmType, at)
				fmt.Printf("got alarm msg %s: %+v\n", tt.args.alarmType, item)
			}
			for k := range alarmMap {
				delete(alarmMap, k)
			}
		})
	}
}

func TestWarnf(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	flag.Set(FlagLevelName, seelog.DebugStr)
	clean()
	initNormalLogger()
	type args struct {
		ctx       context.Context
		format    string
		alarmType string
		params    []interface{}
	}
	tests := []struct {
		name   string
		args   args
		want   string
		getter func() *util.Alarm
	}{
		{
			name: "without-header",
			args: args{
				ctx:       context.Background(),
				alarmType: "WITHOUT_HEADER",
				format:    "test %s",
				params:    []interface{}{"a"},
			},
			want: ".*\\[WRN\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] AlarmType:WITHOUT_HEADER\ttest \\[a\\].*",
			getter: func() *util.Alarm {
				return util.GlobalAlarm
			},
		},
		{
			name: "with-header",
			args: args{
				ctx:       ctx,
				alarmType: "WITH_HEADER",
				format:    "test %s",
				params:    []interface{}{"a"},
			},
			want: ".*\\[WRN\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\tAlarmType:WITH_HEADER\ttest \\[a\\].*",

			getter: func() *util.Alarm {
				return ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm()
			},
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Warningf(tt.args.ctx, tt.args.alarmType, tt.args.format, tt.args.params)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
			alarmMap := tt.getter().AlarmMap
			assert.Equal(t, 1, len(alarmMap), "the size of alarm map; %d", len(alarmMap))
			for at, item := range alarmMap {
				assert.Equal(t, tt.args.alarmType, at)
				fmt.Printf("got alarm msg %s: %+v\n", tt.args.alarmType, item)
			}
			for k := range alarmMap {
				delete(alarmMap, k)
			}
			for k := range alarmMap {
				delete(alarmMap, k)
			}
		})
	}
}

func TestError(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	clean()
	initNormalLogger()
	type args struct {
		ctx       context.Context
		alarmType string
		kvPairs   []interface{}
	}
	tests := []struct {
		name   string
		args   args
		want   string
		getter func() *util.Alarm
	}{
		{
			name: "with-header",
			args: args{
				ctx:       ctx,
				alarmType: "WITH_HEADER",
				kvPairs:   []interface{}{"a", "b"},
			},
			want: ".*\\[ERR\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\tAlarmType:WITH_HEADER\t\\[a b\\]:.*",
			getter: func() *util.Alarm {
				return ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm()
			},
		},
		{
			name: "without-header",
			args: args{
				ctx:       context.Background(),
				alarmType: "WITHOUT_HEADER",
				kvPairs:   []interface{}{"a", "b"},
			},
			want: ".*\\[ERR\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] AlarmType:WITHOUT_HEADER\t\\[a b\\]:.*",
			getter: func() *util.Alarm {
				return util.GlobalAlarm
			},
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Error(tt.args.ctx, tt.args.alarmType, tt.args.kvPairs)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
			alarmMap := tt.getter().AlarmMap
			assert.Equal(t, 1, len(alarmMap), "the size of alarm map; %d", len(alarmMap))
			for at, item := range alarmMap {
				assert.Equal(t, tt.args.alarmType, at)
				fmt.Printf("got alarm msg %s: %+v\n", tt.args.alarmType, item)
			}
			for k := range alarmMap {
				delete(alarmMap, k)
			}
		})
	}
}

func TestErrorf(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	flag.Set(FlagLevelName, seelog.DebugStr)
	clean()
	initNormalLogger()
	type args struct {
		ctx       context.Context
		format    string
		alarmType string
		params    []interface{}
	}
	tests := []struct {
		name   string
		args   args
		want   string
		getter func() *util.Alarm
	}{
		{
			name: "without-header",
			args: args{
				ctx:       context.Background(),
				alarmType: "WITHOUT_HEADER",
				format:    "test %s",
				params:    []interface{}{"a"},
			},
			want: ".*\\[ERR\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] AlarmType:WITHOUT_HEADER\ttest \\[a\\].*",
			getter: func() *util.Alarm {
				return util.GlobalAlarm
			},
		},
		{
			name: "with-header",
			args: args{
				ctx:       ctx,
				alarmType: "WITH_HEADER",
				format:    "test %s",
				params:    []interface{}{"a"},
			},
			want: ".*\\[ERR\\] \\[logger_test.go:\\d{1,}\\] \\[func\\d{1,}\\] \\[mock-configname,mock-logstore\\]\tAlarmType:WITH_HEADER\ttest \\[a\\].*",
			getter: func() *util.Alarm {
				return ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm()
			},
		},
	}
	for i, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			Errorf(tt.args.ctx, tt.args.alarmType, tt.args.format, tt.args.params)
			time.Sleep(time.Millisecond)
			Flush()
			log := readLog(i)
			assert.True(t, regexp.MustCompile(tt.want).Match([]byte(log)), "want regexp %s, but got: %s", tt.want, log)
			alarmMap := tt.getter().AlarmMap
			assert.Equal(t, 1, len(alarmMap), "the size of alarm map; %d", len(alarmMap))
			for at, item := range alarmMap {
				assert.Equal(t, tt.args.alarmType, at)
				fmt.Printf("got alarm msg %s: %+v\n", tt.args.alarmType, item)
			}
			for k := range alarmMap {
				delete(alarmMap, k)
			}
		})
	}
}

func TestOffRemote(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	clean()
	initTestLogger()
	Warning(context.Background(), "ALARM_TYPE", "a", "b")
	assert.Equal(t, 0, len(util.GlobalAlarm.AlarmMap))
	Error(context.Background(), "ALARM_TYPE", "a", "b")
	assert.Equal(t, 0, len(util.GlobalAlarm.AlarmMap))
	Warningf(context.Background(), "ALARM_TYPE", "test %s", "b")
	assert.Equal(t, 0, len(util.GlobalAlarm.AlarmMap))
	Errorf(context.Background(), "ALARM_TYPE", "test %s", "b")
	assert.Equal(t, 0, len(util.GlobalAlarm.AlarmMap))
	delete(util.GlobalAlarm.AlarmMap, "ALARM_TYPE")
}
