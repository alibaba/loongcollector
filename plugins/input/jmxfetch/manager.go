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
	"io/ioutil"
	"os"
	"os/exec"
	"path"
	"sort"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
	"github.com/alibaba/ilogtail/plugins/input/udpserver"
	"github.com/alibaba/ilogtail/plugins/test/mock"
	"gopkg.in/yaml.v2"
)

var once sync.Once
var manager *Manager

const dispatchKey = "jmxfetch_ilogtail"

type Cfg struct {
	include   []*Filter
	instances map[string]*InstanceInner
	change    bool
}

func NewCfg(filters []*Filter) *Cfg {
	return &Cfg{
		instances: map[string]*InstanceInner{},
		include:   filters,
	}
}

type Manager struct {
	jmxFetchPath     string
	jmxfetchdPath    string
	jmxfetchConfPath string
	allLoadedCfgs    map[string]*Cfg
	collectors       map[string]ilogtail.Collector
	managerMeta      *helper.ManagerMeta
	stopChan         chan struct{}
	uniqueCollectors string

	server      *udpserver.SharedUDPServer
	javaPath    string
	initSuccess bool
	sync.Mutex
	port int
}

func (m *Manager) RegisterCollector(key string, collector ilogtail.Collector, filters []*Filter) {
	if !m.initSuccess {
		return
	}
	logger.Debug(m.managerMeta.GetContext(), "register collector", key)
	m.Lock()
	defer m.Unlock()
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

func (m *Manager) Register(ctx ilogtail.Context, key string, configs map[string]*InstanceInner) {
	logger.Debug(m.managerMeta.GetContext(), "register config111", len(configs))
	if !m.initSuccess {
		return
	}
	logger.Debug(m.managerMeta.GetContext(), "register config", len(configs))
	m.Lock()
	defer m.Unlock()
	ltCtx, ok := ctx.GetRuntimeContext().Value(pkg.LogTailMeta).(*pkg.LogtailContextMeta)
	if ok {
		m.managerMeta.Add(ltCtx.GetProject(), ltCtx.GetLogStore(), ltCtx.GetConfigName())
	}
	var todoAddCfgs, todoDeleteCfgs bool
	cfg, ok := m.allLoadedCfgs[key]
	if !ok {
		logger.Error(m.managerMeta.GetContext(), "JMX_ALARM", "cannot find instance key", key)
		return
	}
	for key := range cfg.instances {
		_, ok := configs[key]
		if !ok {
			todoDeleteCfgs = true
			delete(cfg.instances, key)
		}
	}
	for key := range configs {
		_, ok := cfg.instances[key]
		if !ok {
			todoAddCfgs = true
			cfg.instances[key] = configs[key]
		}
	}
	logger.Infof(m.managerMeta.GetContext(), "loaded %s instances after register: %d", key, len(cfg.instances))
	cfg.change = cfg.change || todoDeleteCfgs || todoAddCfgs
}

func (m *Manager) startServer() {
	logger.Info(m.managerMeta.GetContext(), "start", "server")
	if m.server == nil {
		m.port, _ = helper.GetFreePort()
		m.server, _ = udpserver.NewSharedUDPServer(mock.NewEmptyContext("", "", "jmxfetchserver"), "statsd", ":"+strconv.Itoa(m.port), dispatchKey, 65535)
	}
	if !m.server.IsRunning() {
		if err := m.server.Start(); err != nil {
			logger.Error(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "start jmx server err", err)
		} else {
			logger.Info(m.managerMeta.GetContext(), "start jmxfetch server goroutine success")
			p, err := m.checkJavaPath(m.javaPath)
			if err != nil {
				logger.Error(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "java path", m.javaPath, "err", err)
				return
			}
			logger.Infof(m.managerMeta.GetContext(), "find jdk path: %s", p)
			err = m.installScripts(p)
			if err != nil {
				logger.Error(m.managerMeta.GetContext(), "JMXFETCH_ALARM", "jmxfetch script install fail", err)
				return
			}
			logger.Info(m.managerMeta.GetContext(), "install jmx scripts success")
		}
	}
}

func (m *Manager) stopServer() {
	if m.server != nil && m.server.IsRunning() {
		logger.Info(m.managerMeta.GetContext(), "stop jmxfetch server goroutine")
		_ = m.server.Stop()
		m.server = nil
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
			}
		}

	}
	startFunc := func() {
		m.Lock()
		defer m.Unlock()
		var temp []string
		for s := range m.allLoadedCfgs {
			temp = append(temp, s)
		}
		sort.Strings(temp)
		uniq := strings.Join(temp, ",")
		if m.uniqueCollectors != uniq {
			m.stopServer()
			m.stop()
			m.startServer()
			for key, collector := range m.collectors {
				m.server.RegisterCollectors(key, collector)
			}
			m.start()
			m.uniqueCollectors = uniq
		}
	}

	stopFunc := func() {
		m.Lock()
		defer m.Unlock()
		if m.server != nil && m.server.CollectorsNum() == 0 {
			logger.Info(m.managerMeta.GetContext(), "stop jmxfetch process")
			m.stopServer()
			m.stop()
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
		"is_jmx": true,
	}
	cfg["init_config"] = initCfg

	if len(userCfg.include) != 0 {
		fiterCfg := make([]map[string]*Filter, 0, 16)
		for _, filter := range userCfg.include {
			fiterCfg = append(fiterCfg, map[string]*Filter{
				"include": filter,
			})
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
		logger.Error(m.managerMeta.GetContext(), "JMXFETCH_CONFIG_ALARM", "cannot convert to yaml bytes", err)
		return
	}
	cfgPath := path.Join(m.jmxfetchConfPath, key+".yaml")
	logger.Debug(m.managerMeta.GetContext(), "write files", string(bytes), "path", cfgPath)
	err = ioutil.WriteFile(cfgPath, bytes, 0600)
	if err != nil {
		logger.Error(m.managerMeta.GetContext(), "JMXFETCH_CONFIG_ALARM", "write config file err", err, "path", cfgPath)
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
			logger.Info(m.managerMeta.GetContext(), "detect user default java path", string(bytes[:len(bytes)-1]))
			if err != nil && !strings.Contains(err.Error(), "no child process") {
				return "", fmt.Errorf("java path is illegal: %v", err)
			}
			javaPath = string(bytes[:len(bytes)-1]) // remove \n
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
	if files, err := ioutil.ReadDir(m.jmxfetchConfPath); err == nil {
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

func GetJmxFetchManager(agentDirPath string) *Manager {
	once.Do(func() {
		manager = createManager(agentDirPath)
		// don't init the collector with struct because the collector depends on the bindMeta.
		util.RegisterAlarm("jmxfetch", manager.managerMeta.GetAlarm())
		if manager.autoInstall() || manager.manualInstall() {
			manager.initSuccess = manager.initConfDir()
		}
		if manager.initSuccess {
			logger.Info(manager.managerMeta.GetContext(), "init jmxfetch manager success")
			go manager.run()
		}
	})
	return manager
}

func createManager(agentDirPath string) *Manager {
	return &Manager{
		managerMeta:      helper.NewmanagerMeta("jmxfetch"),
		jmxFetchPath:     agentDirPath,
		jmxfetchConfPath: path.Join(agentDirPath, "conf.d"),
		jmxfetchdPath:    path.Join(agentDirPath, scriptsName),
		stopChan:         make(chan struct{}, 1),
		allLoadedCfgs:    make(map[string]*Cfg),
		collectors:       make(map[string]ilogtail.Collector),
	}
}
