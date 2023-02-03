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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type ProcessorWrapper struct {
	Processor pipeline.ProcessorV1
	Config    *LogstoreConfig
	LogsChan  chan *pipeline.LogWithContext
	Priority  int
}

type ProcessorWrapperArray []*ProcessorWrapper

func (c ProcessorWrapperArray) Len() int {
	return len(c)
}
func (c ProcessorWrapperArray) Swap(i, j int) {
	c[i], c[j] = c[j], c[i]
}
func (c ProcessorWrapperArray) Less(i, j int) bool {
	return c[i].Priority < c[j].Priority
}
