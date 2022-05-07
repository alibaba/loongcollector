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

//go:build !linux
// +build !linux

package process

import (
	"encoding/json"
	"fmt"
	"testing"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/plugins/test"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestInputProcess_Collect(t *testing.T) {
	type conditions struct {
		OpenFD bool
		Thread bool
		NetIO  bool
		IO     bool
	}
	tests := []struct {
		name       string
		conditions conditions
		wantNum    int
	}{
		{
			"default_collector",
			conditions{
				false,
				false,
				false,
				false,
			},
			7,
		},
	}
	cxt := mock.NewEmptyContext("project", "store", "config")
	p := ilogtail.MetricInputs["metric_process_v2"]().(*InputProcess)
	if _, err := p.Init(cxt); err != nil {
		t.Errorf("cannot init the mock process plugin: %v", err)
		return
	}
	c := &test.MockMetricCollector{}
	p.TopNCPU = 1

	_ = p.Collect(c)
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c.Logs = nil
			p.OpenFD = tt.conditions.OpenFD
			p.IO = tt.conditions.IO
			p.NetIO = tt.conditions.NetIO
			p.Thread = tt.conditions.Thread

			if err := p.Collect(c); err != nil {
				t.Errorf("cannot collect the process metrics: %v", err)
				return
			}
			if len(c.Logs) != tt.wantNum {
				bytes, _ := json.Marshal(c.Logs)
				t.Errorf("process v2 plugins want to collect %d metrics, but got %d: %s", tt.wantNum, len(c.Logs), string(bytes))
				return
			}
			for _, log := range c.Logs {
				bytes, _ := json.Marshal(log)
				fmt.Printf("got metrics: %s\n", string(bytes))
			}
		})
	}
}
