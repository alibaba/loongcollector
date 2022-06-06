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
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

var statusCheckInterval = time.Second * time.Duration(30)

type Config struct {
	Name   string
	Detail string
}

type BindingMeta struct {
	Metas map[string]map[string]map[string]struct{}
	ctx   context.Context
	meta  *pkg.LogtailContextMeta
}

func NewBindMeta() *BindingMeta {
	ctx, meta := pkg.NewLogtailContextMeta("", "", "telegraf")
	return &BindingMeta{
		Metas: make(map[string]map[string]map[string]struct{}),
		ctx:   ctx,
		meta:  meta,
	}
}

func (b *BindingMeta) Add(prj, logstore, cfg string) {
	change := false
	if _, ok := b.Metas[prj]; !ok {
		b.Metas[prj] = make(map[string]map[string]struct{})
		change = true
	}
	if _, ok := b.Metas[prj][logstore]; !ok {
		b.Metas[prj][logstore] = make(map[string]struct{})
		change = true
	}
	if _, ok := b.Metas[prj][logstore][cfg]; !ok {
		b.Metas[prj][logstore][cfg] = struct{}{}
	}
	if change {
		b.UpdateAlarm()
	}
}

func (b *BindingMeta) Delete(prj, logstore, cfg string) {
	change := false
	delete(b.Metas[prj][logstore], cfg)
	if _, ok := b.Metas[prj][logstore]; ok && len(b.Metas[prj][logstore]) == 0 {
		delete(b.Metas[prj], logstore)
		change = true
	}
	if _, ok := b.Metas[prj]; ok && len(b.Metas[prj]) == 0 {
		delete(b.Metas, prj)
		change = true
	}
	if change {
		b.UpdateAlarm()
	}
}

func (b *BindingMeta) UpdateAlarm() {
	var prjSlice, logstoresSlice []string
	for prj, logstores := range b.Metas {
		for logstore := range logstores {

			logstoresSlice = append(logstoresSlice, logstore)
		}
		prjSlice = append(prjSlice, prj)
	}
	sort.Strings(prjSlice)
	sort.Strings(logstoresSlice)
	b.meta.GetAlarm().Update(strings.Join(prjSlice, ","), strings.Join(logstoresSlice, ","))
}

// Telegraf supervisor for agent start, stop, config reload...
//
// Because Telegraf will send all inputs' data to all outputs, so only ONE Logtail
//   config will be passed to Telegraf simultaneously.
//
// Data link: Telegraf ------ HTTP ------> Logtail ----- Protobuf ------> SLS.
// Logtail will work as an InfluxDB server to receive data from telegraf by HTTP protocol.
type Manager struct {
	// Although only one config will be loaded, all configs should be saved for replacement
	// while current config is unregistered.
	configs map[string]*Config
	mu      sync.Mutex

	loadedConfigs map[string]*Config

	ch               chan struct{}
	telegrafPath     string
	telegrafdPath    string
	telegrafConfPath string
	collector        *LogCollector
	bindMeta         *BindingMeta
}

func (tm *Manager) RegisterConfig(ctx ilogtail.Context, c *Config) {
	tm.mu.Lock()
	tm.configs[c.Name] = c
	ltCtx, ok := ctx.GetRuntimeContext().Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		tm.bindMeta.Add(ltCtx.GetProject(), ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	tm.mu.Unlock()
	logger.Debugf(telegrafManager.GetContext(), "register config: %v", c)
	tm.notify()
}

func (tm *Manager) UnregisterConfig(ctx ilogtail.Context, c *Config) {
	tm.mu.Lock()
	delete(tm.configs, c.Name)
	ltCtx, ok := ctx.GetRuntimeContext().Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		tm.bindMeta.Delete(ltCtx.GetProject(), ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	tm.mu.Unlock()
	logger.Debugf(telegrafManager.GetContext(), "unregister config: %v", c)
	tm.notify()
}

func (tm *Manager) notify() {
	select {
	case tm.ch <- struct{}{}:
	default:
	}
}

func isPathExist(p string) (bool, error) {
	_, err := os.Stat(p)
	switch {
	case err == nil:
		return true, nil
	case os.IsNotExist(err):
		return false, nil
	default:
		return false, err
	}
}

// shouldCreatePath returns true if p is not existing and no error when stat.
func shouldCreatePath(p string) bool {
	ret, err := isPathExist(p)
	if err == nil {
		return !ret
	}
	logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
		"stat path %v err: %v", p, err)
	return false
}

// makeSureDirectoryExist returns true if the directory is created just now.
func makeSureDirectoryExist(p string) (bool, error) {
	if !shouldCreatePath(p) {
		return false, nil
	}
	return true, os.MkdirAll(p, 0750)
}

const defaultConfFileName = "telegraf.conf"
const defaultConfig = `
# DO NOT MODIFY: It will be overwrited when Logtail starts.
[agent]
  debug = %t
  logfile = "telegraf.log"
  logfile_rotation_max_size = 1024000
  logfile_rotation_max_archives = 2
`

func (tm *Manager) initAgentDir() {
	if newDir, err := makeSureDirectoryExist(tm.telegrafConfPath); newDir {
		if err != nil {
			logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
				"create conf dir error, path %v, err: %v", tm.telegrafConfPath, err)
		}
	} else {
		// Clean config files (outdated) in conf directory.
		if files, err := ioutil.ReadDir(tm.telegrafConfPath); err == nil {
			for _, f := range files {
				filePath := path.Join(tm.telegrafConfPath, f.Name())
				if err = os.Remove(filePath); err == nil {
					logger.Infof(telegrafManager.GetContext(), "delete outdated agent config file: %v", filePath)
				} else {
					logger.Warningf(telegrafManager.GetContext(), "deleted outdated agent config file err, path: %v, err: %v",
						filePath, err)
				}
			}
		} else {
			logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
				"clean conf dir error, path %v, err: %v", tm.telegrafConfPath, err)
		}
	}
	defaultConfigPath := path.Join(tm.telegrafPath, defaultConfFileName)
	if err := ioutil.WriteFile(defaultConfigPath, []byte(fmt.Sprintf(defaultConfig, logger.DebugFlag())), 0600); err != nil {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
			"write default config error, path: %v, err: %v", defaultConfigPath, err)
	}
}

func (tm *Manager) run() {
	tm.initAgentDir()
	for {
		select {
		case <-time.After(statusCheckInterval):
		case <-tm.ch:
		}
		logger.Debugf(telegrafManager.GetContext(), "start to check")
		tm.check()
		logger.Debugf(telegrafManager.GetContext(), "check done")
	}
}

func (tm *Manager) getLatestConfigs() map[string]*Config {
	tm.mu.Lock()
	defer tm.mu.Unlock()

	if len(tm.configs) == 0 {
		return nil
	}

	toLoadConfigs := make(map[string]*Config)
	for name, cfg := range tm.configs {
		toLoadConfigs[name] = cfg
	}
	return toLoadConfigs
}

func (tm *Manager) check() {
	configs := tm.getLatestConfigs()
	logger.Debugf(telegrafManager.GetContext(), "latest configs: %v", configs)

	// Clear all loaded config files and stop telegraf.
	if configs == nil {
		if len(tm.loadedConfigs) > 0 {
			for name := range tm.loadedConfigs {
				tm.removeConfigFile(name)
			}
			logger.Infof(telegrafManager.GetContext(), "clear all configs and stop agent, count: %v", len(tm.loadedConfigs))
			tm.loadedConfigs = make(map[string]*Config)
		}
		tm.stop()
		tm.collector.TelegrafStop()
		return
	}

	if !tm.install() {
		return
	}

	// Still have configs, do comparison.
	toRemoveConfigs := make([]string, 0)
	toAddOrUpdateConfigs := make([]*Config, 0)

	for name, curCfg := range tm.loadedConfigs {
		if cfg, exists := configs[name]; exists {
			if curCfg.Detail != cfg.Detail {
				toAddOrUpdateConfigs = append(toAddOrUpdateConfigs, cfg)
			}
		} else {
			toRemoveConfigs = append(toRemoveConfigs, name)
		}
	}
	for name, cfg := range configs {
		if _, exists := tm.loadedConfigs[name]; !exists {
			toAddOrUpdateConfigs = append(toAddOrUpdateConfigs, cfg)
		}
	}

	for _, name := range toRemoveConfigs {
		tm.removeConfigFile(name)
		delete(tm.loadedConfigs, name)
	}
	for _, cfg := range toAddOrUpdateConfigs {
		if tm.overwriteConfigFile(cfg) {
			tm.loadedConfigs[cfg.Name] = cfg
		}
	}

	if len(toRemoveConfigs) == 0 && len(toAddOrUpdateConfigs) == 0 {
		tm.start()
	} else {
		tm.reload()
	}
	tm.collector.TelegrafStart()
}

func (tm *Manager) concatConfFilePath(name string) string {
	return path.Join(tm.telegrafConfPath, fmt.Sprintf("%v.conf", name))
}

func (tm *Manager) overwriteConfigFile(cfg *Config) bool {
	filePath := tm.concatConfFilePath(cfg.Name)
	if _, err := makeSureDirectoryExist(tm.telegrafConfPath); err != nil {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_OVERWRITE_CONFIG_ALARM",
			"overwrite local config file error, path: %v err: %v", filePath, err)
		return false
	}
	if err := ioutil.WriteFile(filePath, []byte(cfg.Detail), 0600); err != nil {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_OVERWRITE_CONFIG_ALARM",
			"overwrite local config file error, path: %v err: %v", filePath, err)
		return false
	}

	logger.Infof(telegrafManager.GetContext(), "overwrite agent config %v", cfg.Name)
	return true
}

func (tm *Manager) removeConfigFile(name string) {
	filePath := path.Join(tm.telegrafConfPath, fmt.Sprintf("%v.conf", name))
	if err := os.Remove(filePath); err != nil {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_REMOVE_CONFIG_ALARM",
			"remove local config file error, path: %v, err: %v", filePath, err)
		return
	}

	logger.Infof(telegrafManager.GetContext(), "remove agent config %v", name)
}

func (tm *Manager) runTelegrafd(command string, needOutput bool) (output []byte, err error) {
	cmd := exec.Command(tm.telegrafdPath, command) //nolint:gosec
	if needOutput {
		output, err = cmd.CombinedOutput()
	} else {
		// Must call start/reload without output, because they might fork sub process,
		// which will hang when CombinedOutput is called.
		err = cmd.Run()
	}
	// Workaround: exec.Command throws wait:no child process error always under c-shared buildmode.
	// TODO: try cgo, implement exec with C and popen.
	if err != nil && !strings.Contains(err.Error(), "no child process") {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
			"%v error, output: %v, error: %v", command, string(output), err)
	}
	return
}

// install returns true if agent has been installed.
func (tm *Manager) install() bool {
	if exist, err := isPathExist(tm.telegrafdPath); err != nil {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
			"stat path %v err when install: %v", tm.telegrafdPath, err)
		return false
	} else if exist {
		return true
	}

	scriptPath := path.Join(tm.telegrafPath, "install.sh")
	if exist, err := isPathExist(scriptPath); err != nil || !exist {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
			"can not find install script %v, maybe stat error: %v", scriptPath, err)
		return false
	}

	// Install by execute install.sh
	cmd := exec.Command(scriptPath) //nolint:gosec
	output, err := cmd.CombinedOutput()
	if err != nil && !strings.Contains(err.Error(), "no child process") {
		logger.Warningf(telegrafManager.GetContext(), "SERVICE_TELEGRAF_RUNTIME_ALARM",
			"install agent error, output: %v, error: %v", string(output), err)
		return false
	}
	tm.initAgentDir()
	logger.Infof(telegrafManager.GetContext(), "install agent done, output: %v", string(output))
	return true
}

func (tm *Manager) start() {
	_, _ = tm.runTelegrafd("start", false)
}

func (tm *Manager) stop() {
	if exist, _ := isPathExist(tm.telegrafdPath); !exist {
		return
	}
	_, _ = tm.runTelegrafd("stop", false)
}

func (tm *Manager) reload() {
	_, _ = tm.runTelegrafd("reload", false)
	logger.Infof(telegrafManager.GetContext(), "agent config reloaded")
}

func (tm *Manager) GetContext() context.Context {
	return tm.bindMeta.ctx
}

var telegrafManager *Manager
var once sync.Once

func GetTelegrafManager(agentDirPath string) *Manager {
	once.Do(func() {
		telegrafManager = &Manager{
			configs:          make(map[string]*Config),
			loadedConfigs:    make(map[string]*Config),
			ch:               make(chan struct{}, 1),
			telegrafPath:     agentDirPath,
			bindMeta:         NewBindMeta(),
			telegrafdPath:    path.Join(agentDirPath, "telegrafd"),
			telegrafConfPath: path.Join(agentDirPath, "conf.d"),
		}
		// don't init the collector with struct because the collector depends on the bindMeta.
		telegrafManager.collector = NewLogCollector(agentDirPath)
		util.RegisterAlarm("telegraf", telegrafManager.bindMeta.meta.GetAlarm())
		go telegrafManager.run()
		go telegrafManager.collector.Run()
	})
	return telegrafManager
}
