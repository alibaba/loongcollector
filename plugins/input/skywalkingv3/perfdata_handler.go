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

package skywalkingv3

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	agent "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/agent/v3"
)

type perfDataHandler interface {
	collectorPerfData(logs *agent.BrowserPerfData) (*v3.Commands, error)
}

type perfDataHandlerImpl struct {
	context   pipeline.Context
	collector pipeline.Collector
}

func (p perfDataHandlerImpl) collectorPerfData(perfData *agent.BrowserPerfData) (*v3.Commands, error) {
	return &v3.Commands{}, nil
}
