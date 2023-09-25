// Copyright 2022 iLogtail Authors
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

package jmxfetch

import (
	"fmt"
	"os"
	"os/exec"
	"path"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"gopkg.in/yaml.v2"

	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/udpserver"
	"github.com/alibaba/ilogtail/plugins/test/mock"

	_ "github.com/alibaba/ilogtail/plugins/extension/default_decoder" // jmxfetch depends on the statsd format from ext_default_decoder
)

var once sync.Once
var manager *Manager

const dispatchKey = "jmxfetch_ilogtail"

func GetJmxFetchManager(agentDirPath string) *Manager {
	once.Do(func() {
		manager = createManager(agentDirPath)
		// don't init the collector with struct because the collector depends on the bindMeta.
		util.RegisterAlarm("jmxfetch", manager.managerMeta.GetAlarm())
		if manager.autoInstall() || manager.manualInstall() {
			manager.initSuccess = manager.initConfDir()
		}
		if manager.initSuccess {
			manager.collector = NewLogCollector(agentDirPath)
			go manager.run()
			go manager.collector.Run()
			logger.Info(manager.managerMeta.GetContext(), "init jmxfetch manager success")
		}
	})
	return manager
}

func createManager(agentDirPath string) *Manager {
	return &Manager{
		jmxFetchPath:     agentDirPath,
		jmxfetchdPath:    path.Join(agentDirPath, scriptsName),
		jmxfetchConfPath: path.Join(agentDirPath, "conf.d"),
		allLoadedCfgs:    make(map[string]*Cfg),
		collectors:       make(map[string]pipeline.Collector),
		managerMeta:      helper.NewmanagerMeta("jmxfetch"),
		stopChan:         make(chan struct{}),
	}
}

type Manager struct {
	jmxFetchPath     string
	jmxfetchdPath    string
	jmxfetchConfPath string
	allLoadedCfgs    map[string]*Cfg
	collectors       map[string]pipeline.Collector
	managerMeta      *helper.ManagerMeta
	stopChan         chan struct{}
	uniqueCollectors string
	server           *udpserver.SharedUDPServer
	javaPath         string
	initSuccess      bool
	port             int
	collector        *LogCollector
	sync.Mutex
}

func (m *Manager) RegisterCollector(ctx pipeline.Context, key string, collector pipeline.Collector, filters []*FilterInner) {
	if !m.initSuccess {
		return
	}
	logger.Debug(m.managerMeta.GetContext(), "register collector", key)
	m.Lock()
	defer m.Unlock()
	ltCtx, ok := ctx.GetRuntimeContext().Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		m.managerMeta.Add(ltCtx.GetProject(), ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	if _, ok := m.allLoadedCfgs[key]; !ok {
		m.allLoadedCfgs[key] = NewCfg(filters)
	}
	m.collectors[key] = collector
}

func (m *Manager) UnregisterCollector(key string) {
	if !m.initSuccess {
		return
	}
	logger.Debug(m.managerMeta.GetContext(), "unregister collector", key)
	m.Lock()
	defer m.Unlock()
	if m.server != nil {
		m.server.UnregisterCollectors(key)
	}
	_ = os.Remove(path.Join(m.jmxfetchConfPath, key+".yaml"))
	delete(m.allLoadedCfgs, key)
	delete(m.collectors, key)
	if len(m.collectors) == 0 {
		m.stopChan <- struct{}{}
	}
}

// ConfigJavaHome would select the random jdk path if configured many diff paths.
func (m *Manager) ConfigJavaHome(javaHome string) {
	m.Lock()
	defer m.Unlock()
	m.javaPath = javaHome
}

func (m *Manager) Register(key string, configs map[string]*InstanceInner, newGcMetrics bool) {
	if !m.initSuccess {
		return
	}
	logger.Debug(m.managerMeta.GetContext(), "register config", len(configs))
	m.Lock()
	defer m.Unlock()
	var todoAddCfgs, todoDeleteCfgs bool
	cfg, ok := m.allLoadedCfgs[key]
	if !ok {
		logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "cannot find instance key", key)
		return
	}
	for key := range cfg.instances {
		if _, ok := configs[key]; !ok {
			todoDeleteCfgs = true
			delete(cfg.instances, key)
		}
	}
	for key := range configs {
		if _, ok := cfg.instances[key]; !ok {
			todoAddCfgs = true
			cfg.instances[key] = configs[key]
		}
	}
	if cfg.newGcMetrics != newGcMetrics {
		todoAddCfgs = true
		cfg.newGcMetrics = newGcMetrics
	}
	logger.Debugf(m.managerMeta.GetContext(), "loaded %s instances after register: %d", key, len(cfg.instances))
	cfg.change = cfg.change || todoDeleteCfgs || todoAddCfgs
}

func (m *Manager) startServer() {
	logger.Debug(m.managerMeta.GetContext(), "start", "server")
	if m.server == nil {
		m.port, _ = helper.GetFreePort()
		m.server, _ = udpserver.NewSharedUDPServer(mock.NewEmptyContext("", "", "jmxfetchserver"), "ext_default_decoder", "statsd", ":"+strconv.Itoa(m.port), dispatchKey, 65535)
	}
	if !m.server.IsRunning() {
		if err := m.server.Start(); err != nil {
			logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "start jmx server err", err)
		} else {
			p, err := m.checkJavaPath(m.javaPath)
			if err != nil {
				logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "java path", m.javaPath, "err", err)
				return
			}
			logger.Infof(m.managerMeta.GetContext(), "find jdk path: %s", p)
			err = m.installScripts(p)
			if err != nil {
				logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "jmxfetch script install fail", err)
				return
			}
			logger.Info(m.managerMeta.GetContext(), "install jmx scripts success")
		}
		m.collector.JmxfetchStart()
	}
}

func (m *Manager) stopServer() {
	if m.server != nil && m.server.IsRunning() {
		logger.Info(m.managerMeta.GetContext(), "stop jmxfetch server goroutine")
		_ = m.server.Stop()
		m.server = nil
		m.collector.JmxfetchStop()
	}
}

func (m *Manager) run() {
	cfgticker := time.NewTicker(time.Second * 5)
	statusticker := time.NewTicker(time.Second * 5)

	updateFunc := func() {
		m.Lock()
		defer m.Unlock()
		for key, cfg := range m.allLoadedCfgs {
			if cfg.change {
				logger.Info(m.managerMeta.GetContext(), "update config", key)
				m.updateFiles(key, cfg)
				cfg.change = false
				m.reload()
			}
		}

	}
	startFunc := func() {
		m.Lock()
		defer m.Unlock()
		var temp []string
		if len(m.collectors) == 0 {
			return
		}
		for s := range m.collectors {
			temp = append(temp, s)
		}
		sort.Strings(temp)
		uniq := strings.Join(temp, ",")
		if m.uniqueCollectors != uniq {
			m.stop()
			m.stopServer()
			m.startServer()
			for key, collector := range m.collectors {
				m.server.RegisterCollectors(key, collector)
			}
			m.start()
			m.uniqueCollectors = uniq
		} else {
			m.startServer()
			m.start()
		}
	}

	stopFunc := func() {
		m.Lock()
		defer m.Unlock()
		if m.server != nil && len(m.collectors) == 0 {
			logger.Info(m.managerMeta.GetContext(), "stop jmxfetch process")
			m.stop()
			m.stopServer()
			m.uniqueCollectors = ""
		}
	}

	for {
		select {
		case <-cfgticker.C:
			updateFunc()
		case <-statusticker.C:
			startFunc()
		case <-m.stopChan:
			stopFunc()
		}
	}
}

// updateFiles update config.yaml, and don't need to start again because datadog jmxfetch support hot reload config.
func (m *Manager) updateFiles(key string, userCfg *Cfg) {
	if len(userCfg.instances) == 0 {
		return
	}
	cfg := make(map[string]interface{})
	initCfg := map[string]interface{}{
		"is_jmx":         true,
		"new_gc_metrics": userCfg.newGcMetrics,
	}
	cfg["init_config"] = initCfg

	if len(userCfg.include) != 0 {
		fiterCfg := make([]map[string]*FilterInner, 0, 16)
		for _, filter := range userCfg.include {
			fiterCfg = append(fiterCfg, map[string]*FilterInner{"include": filter})
		}
		initCfg["conf"] = fiterCfg
	}
	var instances []*InstanceInner
	for k := range userCfg.instances {
		instances = append(instances, userCfg.instances[k])
	}
	cfg["instances"] = instances
	bytes, err := yaml.Marshal(cfg)
	if err != nil {
		logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "cannot convert to yaml bytes", err)
		return
	}
	cfgPath := path.Join(m.jmxfetchConfPath, key+".yaml")
	logger.Debug(m.managerMeta.GetContext(), "write files", string(bytes), "path", cfgPath)
	err = os.WriteFile(cfgPath, bytes, 0600)
	if err != nil {
		logger.Error(m.managerMeta.GetContext(), JMXAlarmType, "write config file err", err, "path", cfgPath)
	}
}

// checkJavaPath detect java path by following sequences.
// configured > /etc/ilogtail/jvm/jdk > detect java home
func (m *Manager) checkJavaPath(javaPath string) (string, error) {
	if javaPath == "" {
		jdkHome := path.Join(m.jmxFetchPath, "jdk/bin/java")
		exists, _ := util.PathExists(jdkHome)
		logger.Info(m.managerMeta.GetContext(), "default java path", jdkHome, "exist", exists)
		if !exists {
			cmd := exec.Command("which", "java")
			bytes, err := util.CombinedOutputTimeout(cmd, time.Second)
			if err != nil && !strings.Contains(err.Error(), "no child process") {
				return "", fmt.Errorf("java path is illegal: %v", err)
			}
			javaPath = string(bytes[:len(bytes)-1]) // remove \n
			logger.Info(m.managerMeta.GetContext(), "detect user default java path", javaPath)
		} else {
			javaPath = jdkHome
		}
	}
	cmd := exec.Command(javaPath, "-version") //nolint:gosec
	logger.Debug(m.managerMeta.GetContext(), "try detect java cmd", cmd.String())
	if _, err := util.CombinedOutputTimeout(cmd, time.Second); err != nil && !strings.Contains(err.Error(), "no child process") {
		return "", fmt.Errorf("java cmd is illegal: %v", err)
	}
	return javaPath, nil
}

// autoInstall returns true if agent has been installed.
func (m *Manager) autoInstall() bool {
	if exist, err := util.PathExists(m.jmxfetchdPath); err != nil {
		logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "stat path %v err when install: %v", m.jmxfetchdPath, err)
		return false
	} else if exist {
		return true
	}
	scriptPath := path.Join(m.jmxFetchPath, "install.sh")
	if exist, err := util.PathExists(scriptPath); err != nil || !exist {
		logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM",
			"can not find install script %v, maybe stat error: %v", scriptPath, err)
		return false
	}
	cmd := exec.Command(scriptPath) //nolint:gosec
	output, err := cmd.CombinedOutput()
	if err != nil && !strings.Contains(err.Error(), "no child process") {
		logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM",
			"install agent error, output: %v, error: %v", string(output), err)
		return false
	}
	logger.Infof(m.managerMeta.GetContext(), "install agent done, output: %v", string(output))
	exist, err := util.PathExists(m.jmxFetchPath)
	return exist && err == nil
}

// manualInstall returns true if agent has been installed.
func (m *Manager) manualInstall() bool {
	logger.Infof(m.managerMeta.GetContext(), "init jmxfetch path: %s", m.jmxFetchPath)
	if exist, err := util.PathExists(m.jmxFetchPath); !exist {
		if err != nil {
			logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "create conf dir error, path %v, err: %v", m.jmxFetchPath, err)
			return false
		}
		err = os.MkdirAll(m.jmxFetchPath, 0750)
		if err != nil {
			logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "create conf dir error, path %v, err: %v", m.jmxFetchPath, err)
		}
	}
	return true
}

func (m *Manager) initConfDir() bool {
	if exist, err := util.PathExists(m.jmxfetchConfPath); !exist {
		if err != nil {
			logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "create conf dir error, path %v, err: %v", m.jmxfetchConfPath, err)
			return false
		}
		err = os.MkdirAll(m.jmxfetchConfPath, 0750)
		if err != nil {
			logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "create conf dir error, path %v, err: %v", m.jmxfetchConfPath, err)
			return false
		}
		return true
	}
	// Clean config files (outdated) in conf directory.
	if files, err := os.ReadDir(m.jmxfetchConfPath); err == nil {
		for _, f := range files {
			filePath := path.Join(m.jmxfetchConfPath, f.Name())
			if err = os.Remove(filePath); err == nil {
				logger.Infof(m.managerMeta.GetContext(), "delete outdated agent config file: %v", filePath)
			} else {
				logger.Warningf(m.managerMeta.GetContext(), "deleted outdated agent config file err, path: %v, err: %v",
					filePath, err)
				return false
			}
		}
	} else {
		logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_ALARM",
			"clean conf dir error, path %v, err: %v", m.jmxfetchConfPath, err)
		return false
	}
	return true
}
