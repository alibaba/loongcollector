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
	"testing"

	"github.com/stretchr/testify/assert"
)

func excludeFlag() {
	flag.Set(FlagLevelName, "xxx")
	flag.Set(FlagConsoleName, "xxx")
	flag.Set(FlagRetainName, "xxx")
}

func TestOptionDebugLevel(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionDebugLevel, OptionOffConsole)
	Debug(context.Background(), "line0")
	assert.True(t, readLog(0) != "")

}

func TestOptionInfoLevel(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionInfoLevel)
	Debug(context.Background(), "line")
	assert.True(t, readLog(0) == "", "read %s", readLog(0))
	Info(context.Background(), "line")
	assert.True(t, readLog(0) != "")
}

func TestOptionWarnLevel(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionWarnLevel)
	Info(context.Background(), "line")
	assert.True(t, readLog(0) == "")
	Warning(context.Background(), "ALARM", "line")
	assert.True(t, readLog(0) != "")
}

func TestOptionErrorLevel(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionErrorLevel)
	Warning(context.Background(), "line")
	assert.True(t, readLog(0) == "")
	Error(context.Background(), "ALARM", "line")
	assert.True(t, readLog(0) != "")
}

func TestOptionConsole(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger()
	Info(context.Background(), "hello")
	initTestLogger(OptionOffConsole)
	Info(context.Background(), "hello2")
}

func TestOptionConfigRetain(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionDebugLevel, OptionOffConsole)
	Debug(context.Background(), "line0")
	assert.True(t, readLog(0) != "")
	initTestLogger(OptionRetainConfig)
	Debug(context.Background(), "line1")
	assert.True(t, readLog(1) != "")
}

func TestOptionAsync(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionOffConsole)
	Info(context.Background(), "line0")
	assert.True(t, readLog(0) != "")
	initTestLogger(OptionOffConsole, OptionAsyncLogger)
	Info(context.Background(), "line1")
	assert.True(t, readLog(1) == "")
	Flush()
	assert.True(t, readLog(1) != "")
}

func TestOpenMemoryReceiver(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionOpenMemoryReceiver)
	Info(context.Background(), "a")
	_, ok := ReadMemoryLog(1)
	assert.True(t, ok)
}
