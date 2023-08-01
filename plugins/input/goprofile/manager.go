// Copyright 2023 iLogtail Authors
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

package goprofile

import (
	"context"
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/helper/profile/pyroscope/pprof"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/mitchellh/mapstructure"
	"github.com/prometheus/client_golang/prometheus"
	"github.com/pyroscope-io/pyroscope/pkg/ingestion"
	"github.com/pyroscope-io/pyroscope/pkg/scrape"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/config"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/discovery"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/model"
	"github.com/pyroscope-io/pyroscope/pkg/storage/metadata"
	"github.com/pyroscope-io/pyroscope/pkg/util/bytesize"
	"github.com/sirupsen/logrus"
)

type Mode string

const (
	HostMode       Mode = "host"
	KubernetesMode Mode = "kubernetes"
)

type Register struct {
	// todo convert make an component to convert prometheus metrics to ilogtail statistics
}

func (r *Register) Register(collector prometheus.Collector) error {
	return nil
}
func (r *Register) MustRegister(collector ...prometheus.Collector) {
}
func (r *Register) Unregister(collector prometheus.Collector) bool {
	return true
}

type Ingestion struct {
	collector pipeline.Collector
}

func (i *Ingestion) Ingest(ctx context.Context, input *ingestion.IngestInput) error {
	logger.Debug(ctx, input.Format, input.Metadata.SpyName, "size", len(input.Profile.Profile))
	p := pprof.NewRawProfileByPull(input.Profile.Profile, input.Profile.PreviousProfile, input.Profile.SampleTypeConfig)
	getAggType := func() profile.AggType {
		switch input.Metadata.AggregationType {
		case metadata.AverageAggregationType:
			return profile.AvgAggType
		case metadata.SumAggregationType:
			return profile.SumAggType
		default:
			return profile.SumAggType
		}

	}
	logs, err := p.Parse(ctx, &profile.Meta{
		StartTime:       input.Metadata.StartTime,
		EndTime:         input.Metadata.EndTime,
		SpyName:         "go",
		SampleRate:      input.Metadata.SampleRate,
		Units:           profile.Units(input.Metadata.Units),
		AggregationType: getAggType(),
		Tags:            input.Metadata.Key.Labels(),
	}, map[string]string{})
	if err != nil {
		logger.Debug(context.Background(), "parse pprof fail err", err.Error())
		return err
	}
	for _, log := range logs {
		i.collector.AddRawLog(log)
	}
	return nil
}

type Manager struct {
	scrapeManager    *scrape.Manager
	discoveryManager *discovery.Manager
}

func NewManager(collector pipeline.Collector) *Manager {
	logrusLogger := logrus.New()
	logrusLogger.SetOutput(os.Stdout)
	if logger.DebugFlag() {
		logrusLogger.SetLevel(5)
	} else {
		logrusLogger.SetLevel(4)
	}
	m := new(Manager)
	m.discoveryManager = discovery.NewManager(logrusLogger)
	m.scrapeManager = scrape.NewManager(logrusLogger, &Ingestion{collector: collector}, new(Register), false)
	return m
}

func (m *Manager) Stop() {
	m.discoveryManager.Stop()
	m.scrapeManager.Stop()
}

func (m *Manager) Start(p *GoProfile) error {
	c := config.DefaultConfig()
	name := strings.ToLower(p.ctx.GetConfigName())
	helper.ReplaceInvalidChars(&name)
	c.JobName = name
	c.ScrapeInterval = time.Second * time.Duration(p.Interval)
	c.ScrapeTimeout = time.Second * time.Duration(p.Timeout)
	c.BodySizeLimit = bytesize.KB * bytesize.ByteSize(p.BodyLimitSize)
	c.EnabledProfiles = p.EnabledProfiles
	labelSet := make(model.LabelSet)
	for key, val := range p.Labels {
		labelSet[model.LabelName(key)] = model.LabelValue(val)
	}
	switch p.Mode {
	case HostMode:
		var cfg StaticConfig
		if err := mapstructure.Decode(p.Config, &cfg); err != nil {
			return err
		}
		cfg.labelSet = labelSet
		c.ServiceDiscoveryConfigs = append(c.ServiceDiscoveryConfigs, &cfg)
	case KubernetesMode:
		var cfg KubernetesConfig
		if err := mapstructure.Decode(p.Config, &cfg); err != nil {
			return err
		}
		cfg.labelSet = labelSet
		c.ServiceDiscoveryConfigs = append(c.ServiceDiscoveryConfigs, &cfg)
	default:
		return fmt.Errorf("unsupported mode %s", p.Mode)
	}
	if err := m.scrapeManager.ApplyConfig([]*config.Config{c}); err != nil {
		return err
	}
	go func() {
		logger.Debug(context.Background(), "init discovery manager")
		if err := m.discoveryManager.ApplyConfig(map[string]discovery.Configs{
			c.JobName: c.ServiceDiscoveryConfigs,
		}); err != nil {
			logger.Error(context.Background(), "GOPROFILE_ALARM", "apply discovery config error", err.Error())
		}
		err := m.discoveryManager.Run()
		logger.Debug(context.Background(), "finish discovery manager")
		if err != nil {
			logger.Error(context.Background(), "GOPROFILE_ALARM", "discovery err", err.Error())
		}
	}()
	go func() {
		logger.Debug(context.Background(), "init scrape manager")
		err := m.scrapeManager.Run(m.discoveryManager.SyncCh())
		logger.Debug(context.Background(), "finish scrape manager")
		if err != nil {
			logger.Error(context.Background(), "GOPROFILE_ALARM", "scrape err", err.Error())
		}
	}()
	return nil
}
