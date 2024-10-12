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
	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"path"
)

type ServiceTelegraf struct {
	Detail string

	config  *Config
	context pipeline.Context
	tm      *Manager
}

func (s *ServiceTelegraf) Init(ctx pipeline.Context) (int, error) {
	// TODO: validate Detail

	s.context = ctx
	s.config = &Config{
		Name:   ctx.GetConfigName(),
		Detail: s.Detail,
	}
	s.tm = GetTelegrafManager(path.Join(config.LoongcollectorGlobalConfig.LoongcollectorThirdPartyDir, "telegraf"))
	return 0, nil
}

func (s *ServiceTelegraf) Description() string {
	return "service for Telegraf agent"
}

func (s *ServiceTelegraf) Collect(collector pipeline.Collector) error {
	return nil
}

func (s *ServiceTelegraf) Start(collector pipeline.Collector) error {
	s.tm.RegisterConfig(s.context, s.config)
	return nil
}

func (s *ServiceTelegraf) Stop() error {
	s.tm.UnregisterConfig(s.context, s.config)
	return nil
}

func init() {
	pipeline.ServiceInputs["service_telegraf"] = func() pipeline.ServiceInput {
		return &ServiceTelegraf{}
	}
}
