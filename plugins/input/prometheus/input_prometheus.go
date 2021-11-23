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

package prometheus

import (
	"errors"
	"flag"
	"io/ioutil"
	"os"
	"sync"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"
	"github.com/VictoriaMetrics/VictoriaMetrics/lib/promscrape"
)

var configFileLock sync.Mutex

var configFilePath string
var configName string
var project string
var logstore string
var collector ilogtail.Collector

var promscrapeInitOnce sync.Once

const defaultConfigFilePath = "prometheus_scrape_config_default.yml"

type ServiceStaticPrometheus struct {
	Yaml           string
	ConfigFilePath string

	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   ilogtail.Context
}

func (p *ServiceStaticPrometheus) Init(context ilogtail.Context) (int, error) {
	p.context = context
	return 0, nil
}

func (p *ServiceStaticPrometheus) Description() string {
	return "prometheus scrape plugin for logtail, use vmagent lib"
}

// Gather takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (p *ServiceStaticPrometheus) Collect(ilogtail.Collector) error {
	return nil
}

func (p *ServiceStaticPrometheus) recoverConfig() {
	configFileLock.Lock()
	defer configFileLock.Unlock()
	if configName == p.context.GetConfigName() {
		logger.Info(p.context.GetRuntimeContext(), "stop prometheus scrape config", "success")
		_ = ioutil.WriteFile(configFilePath, []byte(""), os.FileMode(0744))
		project = ""
		logstore = ""
		configName = ""
		configFilePath = ""
		collector = nil
	}
}

func (p *ServiceStaticPrometheus) checkAndSetConfig(c ilogtail.Collector) error {
	configFileLock.Lock()
	defer configFileLock.Unlock()

	if configFilePath != "" {
		logger.Error(p.context.GetRuntimeContext(), "MULTI_PROMETHEUS_SCRAPE_ALARM",
			"error", "logtail can only run one prometheus scrape config, now config is",
			"config", configFilePath,
			"project", project,
			"logstore", logstore,
			"config", configName)
		return errors.New("logtail can only run one prometheus scrape config")
	}
	var fileContent []byte
	if p.ConfigFilePath == "" {
		if p.Yaml == "" {
			return errors.New("empty config")
		}
		fileContent = []byte(p.Yaml)
	} else {
		data, err := ioutil.ReadFile(p.ConfigFilePath)
		if err != nil {
			return err
		}
		fileContent = data
	}
	configFilePath = defaultConfigFilePath
	_ = ioutil.WriteFile(configFilePath, fileContent, os.FileMode(0744))
	project = p.context.GetProject()
	logstore = p.context.GetLogstore()
	configName = p.context.GetConfigName()
	collector = c

	logger.Info(p.context.GetRuntimeContext(), "prometheus scrape init", "success")

	return nil
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceStaticPrometheus) Start(c ilogtail.Collector) error {
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	defer p.waitGroup.Done()

	if err := p.checkAndSetConfig(c); err != nil {
		return err
	}
	if scrapeFlag := flag.Lookup("promscrape.config"); scrapeFlag != nil {
		_ = scrapeFlag.Value.Set(configFilePath)
		logger.Info(p.context.GetRuntimeContext(), "set prometheus scrape config file to", scrapeFlag.Value.String())
	}
	if scrapeFlag := flag.Lookup("promscrape.configCheckInterval"); scrapeFlag != nil {
		_ = scrapeFlag.Value.Set("15s")
		logger.Info(p.context.GetRuntimeContext(), "set prometheus scrape config file reload interval", "15s")
	}
	if err := promscrape.CheckConfig(); err != nil {
		return err
	}

	promscrapeInitOnce.Do(
		func() {
			promscrape.Init(func(wr *prompbmarshal.WriteRequest) {
				tmpCollector := collector
				if tmpCollector != nil {
					appendTSDataToSlsLog(tmpCollector, wr)
				}
			})
			logger.Info(p.context.GetRuntimeContext(), "prometheus scrape init", "success")
		},
	)

	<-p.shutdown
	p.recoverConfig()
	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceStaticPrometheus) Stop() error {

	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func init() {
	ilogtail.ServiceInputs["service_prometheus"] = func() ilogtail.ServiceInput {
		return &ServiceStaticPrometheus{}
	}
}
