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

package telegraf

import (
	"context"
	"fmt"
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"strings"
	"testing"
	"time"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

var (
	defaultConfigDetail string
	testTelegrafPath    string
)

func (tm *Manager) isRunning() bool {
	s := ""
	status, _ := tm.runTelegrafd("status", true)
	if len(status) > 0 {
		s = string(status[0 : len(status)-1])
	}
	return s == "running"
}

func init() {
	data, err := ioutil.ReadFile("local_test/telegraf_test.conf")
	if err != nil {
		panic(err)
	}
	defaultConfigDetail = string(data)

	dir, err := os.Getwd()
	if err != nil {
		panic(err)
	}
	testTelegrafPath = path.Join(dir, "local_test")
	logger.Info(context.Background(), "telegraf path", testTelegrafPath)

	statusCheckInterval = time.Second
}

func getTestTelegrafManager(t *testing.T) *Manager {
	inst := GetTelegrafManager(testTelegrafPath)
	inst.mu.Lock()
	inst.configs = make(map[string]*Config)
	inst.mu.Unlock()
	time.Sleep(time.Duration(statusCheckInterval.Seconds()*2) * time.Second)

	// No config, should be stopped.
	require.False(t, inst.isRunning())
	return inst
}

func isPathExists(p string) bool {
	_, err := os.Stat(p)
	return err == nil
}

func TestDeleteOutdatedConfig(t *testing.T) {
	logger.Infof(context.Background(), "TestDeleteOutdatedConfig begin")
	defer func() {
		logger.Infof(context.Background(), "TestDeleteOutdatedConfig end")
	}()

	confPath := path.Join(testTelegrafPath, "conf.d")
	require.NoError(t, os.MkdirAll(confPath, 0750))
	require.NoError(t, ioutil.WriteFile(path.Join(confPath, "a.conf"), []byte(`content`), 0600))
	require.NoError(t, ioutil.WriteFile(path.Join(confPath, "b.conf"), []byte(`content`), 0600))
	files, err := ioutil.ReadDir(confPath)
	require.NoError(t, err)
	require.Equal(t, len(files), 2)

	getTestTelegrafManager(t)

	files, err = ioutil.ReadDir(confPath)
	require.NoError(t, err)
	require.Equal(t, len(files), 0)
}

func TestGetTelegrafManager(t *testing.T) {
	logger.Infof(context.Background(), "TestGetTelegrafManager begin")
	defer func() {
		logger.Infof(context.Background(), "TestGetTelegrafManager end")
	}()

	inst := getTestTelegrafManager(t)

	inst.mu.Lock()
	defer inst.mu.Unlock()
	require.Equal(t, len(inst.loadedConfigs), 0)
	require.True(t, isPathExists(inst.telegrafPath))
	require.True(t, isPathExists(path.Join(inst.telegrafPath, defaultConfFileName)))
}

func TestRegisterAndUnregister(t *testing.T) {
	logger.Infof(context.Background(), "TestRegisterAndUnregister begin")
	defer func() {
		logger.Infof(context.Background(), "TestRegisterAndUnregister end")
	}()

	inst := getTestTelegrafManager(t)

	c := &Config{
		Name:   "config",
		Detail: defaultConfigDetail,
	}
	inst.RegisterConfig(nil, c)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, inst.isRunning())
	require.True(t, isPathExists(path.Join(inst.telegrafConfPath, c.Name+".conf")))

	inst.UnregisterConfig(nil, c)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.False(t, inst.isRunning())
	require.False(t, isPathExists(path.Join(inst.telegrafConfPath, c.Name+".conf")))
}

func TestUpdateConfig(t *testing.T) {
	logger.Infof(context.Background(), "TestUpdateConfig begin")
	defer func() {
		logger.Infof(context.Background(), "TestUpdateConfig end")
	}()

	inst := getTestTelegrafManager(t)

	c := &Config{
		Name:   "config",
		Detail: defaultConfigDetail,
	}
	inst.RegisterConfig(nil, c)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, inst.isRunning())

	cmd := fmt.Sprintf("ps -ef | grep \"%v/telegraf \" | grep -v grep | awk '{print $2}'", testTelegrafPath)
	oldPid, _ := exec.Command("sh", "-c", cmd).Output()

	c2 := &Config{
		Name:   c.Name,
		Detail: defaultConfigDetail + "\n",
	}
	inst.RegisterConfig(nil, c2)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, inst.isRunning())

	newPid, _ := exec.Command("sh", "-c", cmd).Output()
	require.Equal(t, oldPid, newPid)

	inst.mu.Lock()
	loadedC, exists := inst.loadedConfigs[c.Name]
	require.True(t, exists)
	require.Equal(t, loadedC.Detail, c2.Detail)
	inst.mu.Unlock()

	inst.UnregisterConfig(nil, c2)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.False(t, inst.isRunning())
}

func TestMultipleConfig(t *testing.T) {
	logger.Infof(context.Background(), "TestMultipleConfig begin")
	defer func() {
		logger.Infof(context.Background(), "TestMultipleConfig end")
	}()

	inst := getTestTelegrafManager(t)

	c := &Config{
		Name:   "config",
		Detail: defaultConfigDetail,
	}
	inst.RegisterConfig(nil, c)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, inst.isRunning())
	require.True(t, isPathExists(path.Join(inst.telegrafConfPath, c.Name+".conf")))

	c2 := &Config{
		Name:   c.Name + "2",
		Detail: defaultConfigDetail + "\n",
	}
	inst.RegisterConfig(nil, c2)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, inst.isRunning())
	require.True(t, isPathExists(path.Join(inst.telegrafConfPath, c2.Name+".conf")))

	inst.mu.Lock()
	{
		loadedC, exists := inst.loadedConfigs[c.Name]
		require.True(t, exists)
		require.Equal(t, loadedC.Detail, c.Detail)

		loadedC, exists = inst.loadedConfigs[c2.Name]
		require.True(t, exists)
		require.Equal(t, loadedC.Detail, c2.Detail)
	}
	inst.mu.Unlock()

	inst.UnregisterConfig(nil, c)
	inst.UnregisterConfig(nil, c2)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.False(t, inst.isRunning())
	require.False(t, isPathExists(path.Join(inst.telegrafConfPath, c.Name+".conf")))
	require.False(t, isPathExists(path.Join(inst.telegrafConfPath, c2.Name+".conf")))
}

func TestInstall(t *testing.T) {
	logger.Infof(context.Background(), "TestInstall begin")
	defer func() {
		logger.Infof(context.Background(), "TestInstall end")
	}()

	inst := getTestTelegrafManager(t)
	scriptPath := path.Join(inst.telegrafPath, "install.sh")
	require.NoError(t, os.Remove(scriptPath))
	require.True(t, inst.install())

	tmpName := inst.telegrafdPath + ".tmp"
	require.NoError(t, os.Rename(inst.telegrafdPath, tmpName))
	require.False(t, inst.install())

	fileContent := fmt.Sprintf("#!/bin/sh\nmv %v %v", tmpName, inst.telegrafdPath)
	require.NoError(t, ioutil.WriteFile(scriptPath, []byte(fileContent), 0600))

	require.True(t, inst.install())
	require.True(t, isPathExists(inst.telegrafdPath))
}

func TestOverwriteConfigFile(t *testing.T) {
	logger.Infof(context.Background(), "TestOverwriteConfigFile begin")
	defer func() {
		logger.Infof(context.Background(), "TestOverwriteConfigFile end")
	}()

	inst := getTestTelegrafManager(t)
	require.True(t, isPathExists(inst.telegrafConfPath))
	require.NoError(t, os.Remove(inst.telegrafConfPath))

	c := &Config{
		Name:   "config",
		Detail: defaultConfigDetail,
	}
	inst.RegisterConfig(nil, c)
	time.Sleep(time.Millisecond * time.Duration(500))
	require.True(t, isPathExists(inst.telegrafConfPath))
}

func TestNewBindMeta(t *testing.T) {
	genFunc := func(prefix string, start, end int) string {
		var res []string
		for i := start; i <= end; i++ {
			res = append(res, fmt.Sprintf("%s_%d", prefix, i))
		}
		return strings.Join(res, ",")
	}
	meta := NewBindMeta()
	for i := 0; i < 8; i++ {
		prjNum := i / 4
		logNum := i / 2
		cfgNum := i
		meta.Add(fmt.Sprintf("p_%d", prjNum), fmt.Sprintf("l_%d", logNum), fmt.Sprintf("c_%d", cfgNum))
		assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, genFunc("p", 0, prjNum))
		assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, genFunc("l", 0, logNum))
	}

	for i := 7; i >= 0; i-- {
		prjNum := i / 4
		logNum := i / 2
		cfgNum := i
		meta.Delete(fmt.Sprintf("p_%d", prjNum), fmt.Sprintf("l_%d", logNum), fmt.Sprintf("c_%d", cfgNum))
		cfgNum--
		if i%2 == 0 {
			logNum--
		}
		if i%4 == 0 {
			prjNum--
		}
		if prjNum == -1 && logNum == -1 && cfgNum == -1 {
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, "")
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, "")
		} else {
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Project, genFunc("p", 0, prjNum))
			assert.Equal(t, meta.ctx.Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta).GetAlarm().Logstore, genFunc("l", 0, logNum))

		}
	}
}
