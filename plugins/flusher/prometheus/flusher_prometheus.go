// Copyright 2024 iLogtail Authors
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
	"fmt"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/plugins/flusher/http"
)

var _ pipeline.FlusherV2 = (*FlusherPrometheus)(nil)

type FlusherPrometheus struct {
	config            // config for flusher_prometheus only (not belongs to flusher_http)
	*http.FlusherHTTP // with config for flusher_http as well

	ctx pipeline.Context
}

func NewPrometheusFlusher() *FlusherPrometheus {
	httpFlusher := http.NewHTTPFlusher()
	httpFlusher.DropEventWhenQueueFull = true

	return &FlusherPrometheus{
		FlusherHTTP: httpFlusher,
	}
}

func (p *FlusherPrometheus) Init(context pipeline.Context) error {
	p.ctx = context
	if p.FlusherHTTP == nil {
		logger.Debugf(p.ctx.GetRuntimeContext(), "prometheus flusher (%s) has no http flusher instance", p.Description())
		p.FlusherHTTP = http.NewHTTPFlusher()
	}

	if err := p.prepareInit(); err != nil {
		logger.Errorf(p.ctx.GetRuntimeContext(), "PROMETHEUS_FLUSHER_INIT_ALARM",
			"prometheus flusher prepare init failed, error: %s", err.Error())
		return err
	}

	if err := p.FlusherHTTP.Init(context); err != nil {
		logger.Errorf(p.ctx.GetRuntimeContext(), "PROMETHEUS_FLUSHER_INIT_ALARM",
			"prometheus flusher init http flusher failed, error: %s", err.Error())
		return err
	}

	logger.Infof(p.ctx.GetRuntimeContext(), "%s init success", p.Description())

	return nil
}

func (p *FlusherPrometheus) prepareInit() error {
	if err := p.validateConfig(); err != nil {
		return fmt.Errorf("validate config error: %w", err)
	}

	p.trySetDefaultConfig()
	p.initHTTPFlusherConfig()

	return nil
}

func (p *FlusherPrometheus) validateConfig() error {
	return getValidate().Struct(p.config)
}

func (p *FlusherPrometheus) trySetDefaultConfig() {
	cfg := &p.config

	if cfg.SeriesLimit <= 0 {
		cfg.SeriesLimit = defaultSeriesLimit
	}
}

func (p *FlusherPrometheus) initHTTPFlusherConfig() {
	hc := p.FlusherHTTP

	// Phase1. init http request config
	hc.RemoteURL = p.Endpoint
	if hc.Timeout <= 0 {
		hc.Timeout = defaultTimeout
	}
	if hc.MaxConnsPerHost <= 0 {
		hc.MaxConnsPerHost = defaultMaxConnsPerHost
	}
	if hc.MaxIdleConnsPerHost <= 0 {
		hc.MaxIdleConnsPerHost = defaultMaxIdleConnsPerHost
	}
	if hc.IdleConnTimeout <= 0 {
		hc.IdleConnTimeout = defaultIdleConnTimeout
	}
	if hc.WriteBufferSize <= 0 {
		hc.WriteBufferSize = defaultWriteBufferSize
	}
	if hc.Headers == nil {
		hc.Headers = make(map[string]string)
	}
	// according to VictoriaMetrics/app/vmagent/remotewrite/client.go, i.e.
	// https://github.com/VictoriaMetrics/VictoriaMetrics/blob/v1.103.0/app/vmagent/remotewrite/client.go#L385-393
	hc.Headers[headerKeyUserAgent] = headerValUserAgent
	hc.Headers[headerKeyContentType] = headerValContentType
	hc.Headers[headerKeyContentEncoding] = headerValContentEncoding
	hc.Headers[headerKeyPromRemoteWriteVersion] = headerValPromRemoteWriteVersion

	// Phase2. init http flusher inner config
	if hc.Concurrency <= 0 {
		hc.Concurrency = defaultConcurrency
	}
	if hc.QueueCapacity <= 0 {
		hc.QueueCapacity = defaultQueueCapacity
	}

	// Phase3. init pipeline group events handle config
	if hc.Encoder == nil {
		hc.Encoder = &extensions.ExtensionConfig{
			Type:    "ext_default_encoder",
			Options: map[string]any{"Format": "prometheus", "SeriesLimit": p.SeriesLimit},
		}
	}

	// Phase4. mutate rule
	// transport config refers to the configuration of vmagent:
	// https://github.com/VictoriaMetrics/VictoriaMetrics/blob/v1.100.1/app/vmagent/remotewrite/client.go#L123-126
	//
	// however, there is a little difference between flusher_http & vmagent:
	//   in vmagent, concurrency means number of concurrent queues to each -remoteWrite.url (multiple queues)
	//   in flusher_http, concurrency means how many goroutines consume data queue (single queue)
	if numConns := 2 * hc.Concurrency; hc.MaxConnsPerHost < numConns {
		hc.MaxConnsPerHost = numConns
	}
	if numConns := 2 * hc.Concurrency; hc.MaxIdleConnsPerHost < numConns {
		hc.MaxIdleConnsPerHost = numConns
	}
}

func (p *FlusherPrometheus) Description() string {
	return "prometheus flusher for ilogtail"
}

func (p *FlusherPrometheus) IsReady(projectName string, logstoreName string, logstoreKey int64) bool {
	if p.FlusherHTTP != nil {
		return p.FlusherHTTP.IsReady(projectName, logstoreName, logstoreKey)
	}

	return false
}

func (p *FlusherPrometheus) SetUrgent(flag bool) {
	if p.FlusherHTTP != nil {
		p.FlusherHTTP.SetUrgent(flag)
	}
}

func (p *FlusherPrometheus) Stop() error {
	if p.FlusherHTTP != nil {
		return p.FlusherHTTP.Stop()
	}

	return nil
}

func (p *FlusherPrometheus) Export(events []*models.PipelineGroupEvents, context pipeline.PipelineContext) error {
	if p.FlusherHTTP != nil {
		return p.FlusherHTTP.Export(events, context)
	}

	return errNoHTTPFlusher
}

func init() {
	pipeline.AddFlusherCreator("flusher_prometheus", func() pipeline.Flusher {
		return NewPrometheusFlusher()
	})
}
