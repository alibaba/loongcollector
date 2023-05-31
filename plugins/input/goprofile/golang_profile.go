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
	"errors"

	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type GoProfile struct {
	Mode            Mode
	Interval        int32 // unit second
	Timeout         int32 // unit second
	BodyLimitSize   int32 // unit KB
	EnabledProfiles []string
	Labels          map[string]string
	Config          map[string]interface{}

	ctx     pipeline.Context
	manager *Manager
}

func (g *GoProfile) Init(context pipeline.Context) (int, error) {
	g.ctx = context
	if g.Mode == "" {
		return 0, errors.New("mode is empty")
	}
	if len(g.Config) == 0 {
		return 0, errors.New("config is empty")
	}
	return 0, nil
}

func (g *GoProfile) Description() string {
	return "Go profile support to pull go pprof profile"
}

func (g *GoProfile) Stop() error {
	g.manager.Stop()
	return nil
}

func (g *GoProfile) Start(collector pipeline.Collector) error {
	g.manager = NewManager(collector)
	return g.manager.Start(g)
}

func init() {
	pipeline.ServiceInputs["service_go_profile"] = func() pipeline.ServiceInput {
		return &GoProfile{
			// here you could set default value.
			Interval:        10,
			Timeout:         15,
			BodyLimitSize:   10240,
			EnabledProfiles: []string{"cpu", "mem", "goroutines", "mutex", "block"},
			Labels:          map[string]string{},
		}
	}

}
