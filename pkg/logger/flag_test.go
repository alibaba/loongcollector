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

	"github.com/cihub/seelog"
	"github.com/stretchr/testify/assert"
)

func TestOverrideLevel(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	flag.Set(FlagLevelName, seelog.DebugStr)
	initTestLogger()
	Debugf(context.Background(), "%s", "hello")
	assert.True(t, readLog(0) != "")
	clean()
	flag.Set(FlagLevelName, "xxx")
	initTestLogger()
	Debugf(context.Background(), "%s", "hello")
	assert.True(t, readLog(0) == "")
}

func TestOverrideConsole(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	flag.Set(FlagConsoleName, "true")
	initTestLogger(OptionOffConsole)
	Infof(context.Background(), "%s", "hello")
	clean()
	flag.Set(FlagConsoleName, "xxx")
	initTestLogger()
	initTestLogger(OptionOffConsole)
	Infof(context.Background(), "%s", "hello")
}

func TestOverrideRetain(t *testing.T) {
	mu.Lock()
	defer mu.Unlock()
	excludeFlag()
	clean()
	initTestLogger(OptionDebugLevel)
	Debugf(context.Background(), "%s", "hello")
	assert.True(t, readLog(0) != "")
	flag.Set(FlagRetainName, "true")
	initTestLogger()
	Debugf(context.Background(), "%s", "hello")
	assert.True(t, readLog(1) != "")
}
