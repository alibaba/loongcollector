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

package defaultone

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type ProcessorDefault struct {
}

// Init called for init some system resources, like socket, mutex...
func (*ProcessorDefault) Init(ilogtail.Context) error {
	return nil
}

func (*ProcessorDefault) Description() string {
	return "default processor for logtail"
}

func (*ProcessorDefault) ProcessLogs(logArray []*protocol.Log) []*protocol.Log {
	return logArray
}

func init() {
	ilogtail.Processors["processor_default"] = func() ilogtail.Processor {
		return &ProcessorDefault{}
	}
}
