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
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/VictoriaMetrics/VictoriaMetrics/lib/auth"
	liblogger "github.com/VictoriaMetrics/VictoriaMetrics/lib/logger"
	"github.com/VictoriaMetrics/VictoriaMetrics/lib/prompbmarshal"
	"github.com/VictoriaMetrics/VictoriaMetrics/lib/promscrape"
	"github.com/VictoriaMetrics/VictoriaMetrics/lib/protoparser/common"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

var libLoggerOnce sync.Once

type ServiceStaticPrometheus struct {
	Yaml              string            `comment:"the prometheus configuration content, more details please see [here](https://prometheus.io/docs/prometheus/latest/configuration/configuration/)"`
	ConfigFilePath    string            `comment:"the prometheus configuration path, and the param would be ignored when Yaml param is configured."`
	AuthorizationPath string            `comment:"the prometheus authorization path, only using in authorization files. When Yaml param is configured, the default value is the current binary path. However, the default value is the ConfigFilePath directory when ConfigFilePath is working."`
	ExtraFlags        map[string]string `comment:"the prometheus extra configuration flags, like promscrape.maxScrapeSize, for more flags please see [here](https://docs.victoriametrics.com/vmagent.html#advanced-usage)"`
	NoStaleMarkers    bool              `comment:"Whether to disable sending Prometheus stale markers for metrics when scrape target disappears. This option may reduce memory usage if stale markers aren't needed for your setup. This option also disables populating the scrape_series_added metric. See https://prometheus.io/docs/concepts/jobs_instances/#automatically-generated-labels-and-time-series"`

	scraper   *promscrape.Scraper //nolint:typecheck
	shutdown  chan struct{}
	waitGroup sync.WaitGroup
	context   pipeline.Context
	lock      sync.Mutex
	running   bool
	collector pipeline.Collector
	kubeMeta  *KubernetesMeta
}

func (p *ServiceStaticPrometheus) Init(context pipeline.Context) (int, error) {
	p.kubeMeta = NewKubernetesMeta(context)
	if p.kubeMeta.readKubernetesWorkloadMeta() {
		promscrape.ConfigMemberInfo(int(p.kubeMeta.replicas), strconv.Itoa(p.kubeMeta.currentNum))
	}
	libLoggerOnce.Do(func() {
		if f := flag.Lookup("loggerOutput"); f != nil {
			_ = f.Value.Set("stdout")
		}
		// set max scrape size to 256MB
		err := flag.Set("promscrape.maxScrapeSize", "268435456")
		logger.Info(context.GetRuntimeContext(), "set config maxScrapeSize to 256MB, error", err)
		liblogger.Init()
		common.StartUnmarshalWorkers()
		if p.NoStaleMarkers {
			err := flag.Set("promscrape.noStaleMarkers", "true")
			logger.Info(context.GetRuntimeContext(), "set config", "promscrape.noStaleMarkers", "value", "true", "error", err)
		}
		for k, v := range p.ExtraFlags {
			err := flag.Set(k, v)
			logger.Info(context.GetRuntimeContext(), "set config", k, "value", v, "error", err)
		}
	})
	p.context = context
	var detail []byte
	switch {
	case p.Yaml != "":
		detail = []byte(p.Yaml)
		if p.AuthorizationPath == "" {
			p.AuthorizationPath = config.LoongcollectorGlobalConfig.LoongcollectorConfDir
		}
	case p.ConfigFilePath != "":
		f, err := os.Open(p.ConfigFilePath)
		if err != nil {
			return 0, fmt.Errorf("cannot find prometheus configuration file")
		}
		defer func(f *os.File) {
			_ = f.Close()
		}(f)
		bytes, err := io.ReadAll(f)
		if err != nil {
			return 0, fmt.Errorf("cannot read prometheus configuration file")
		}
		detail = bytes
		if p.AuthorizationPath == "" {
			p.AuthorizationPath = filepath.Dir(p.ConfigFilePath)
		}
	default:
		return 0, errors.New("the scrape configuration is required")
	}
	var err error
	if p.AuthorizationPath, err = filepath.Abs(p.AuthorizationPath); err != nil {
		return 0, fmt.Errorf("cannot find the abs authorization path: %v", err)
	}
	name := strings.Join([]string{context.GetProject(), context.GetLogstore(), context.GetConfigName()}, "_")
	p.scraper = promscrape.NewScraper(detail, name, p.AuthorizationPath) //nolint:typecheck
	if err := p.scraper.CheckConfig(); err != nil {
		return 0, fmt.Errorf("illegal prometheus configuration file %s: %v", name, err)
	}
	return 0, nil
}

func (p *ServiceStaticPrometheus) Description() string {
	return "prometheus scrape plugin for logtail, use vmagent lib"
}

// Start starts the ServiceInput's service, whatever that may be
func (p *ServiceStaticPrometheus) Start(c pipeline.Collector) error {
	p.collector = c
	p.shutdown = make(chan struct{})
	p.waitGroup.Add(1)
	defer p.waitGroup.Done()
	p.scraper.Init(p.slsPushData)
	p.running = true
	if p.kubeMeta.isWorkingOnClusterMode() {
		p.StartKubeReloadScraper()
	}
	<-p.shutdown
	p.scraper.Stop()
	return nil
}

// Stop stops the services and closes any necessary channels and connections
func (p *ServiceStaticPrometheus) Stop() error {
	p.lock.Lock()
	defer p.lock.Unlock()
	p.running = false
	close(p.shutdown)
	p.waitGroup.Wait()
	return nil
}

func (p *ServiceStaticPrometheus) StartKubeReloadScraper() {
	go func() {
		ticker := time.NewTicker(time.Second * 10)
		for {
			select {
			case <-p.shutdown:
				return
			case <-ticker.C:
				change, err := p.kubeMeta.getPrometheusReplicas()
				if !change || err != nil {
					continue
				}
				logger.Info(p.context.GetRuntimeContext(), "found change replicas, would start reload prometheus scraper", p.kubeMeta.replicas)
				p.lock.Lock()
				if !p.running {
					return
				}
				promscrape.ConfigMemberInfo(int(p.kubeMeta.replicas), strconv.Itoa(p.kubeMeta.currentNum))
				p.scraper.Stop()
				p.scraper.Init(p.slsPushData)
				p.lock.Unlock()
				logger.Info(p.context.GetRuntimeContext(), "reload prometheus scraper done")
			}
		}
	}()
}

func init() {
	pipeline.ServiceInputs["service_prometheus"] = func() pipeline.ServiceInput {
		return &ServiceStaticPrometheus{
			NoStaleMarkers: true,
		}
	}
}

func (p *ServiceStaticPrometheus) slsPushData(_ *auth.Token, wr *prompbmarshal.WriteRequest) {
	logger.Debug(p.context.GetRuntimeContext(), "append new metrics", wr.Size())
	appendTSDataToSlsLog(p.collector, wr)
	logger.Debug(p.context.GetRuntimeContext(), "append done", wr.Size())
}
