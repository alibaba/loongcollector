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

package pipeline

import (
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

type LogWithContext struct {
	Log     *protocol.Log
	Context map[string]interface{}
}

// Collector ...
type Collector interface {
	AddData(tags map[string]string,
		fields map[string]string,
		t ...time.Time)

	AddDataArray(tags map[string]string,
		columns []string,
		values []string,
		t ...time.Time)

	AddRawLog(log *protocol.Log)

	AddDataWithContext(tags map[string]string,
		fields map[string]string,
		ctx map[string]interface{},
		t ...time.Time)

	AddDataArrayWithContext(tags map[string]string,
		columns []string,
		values []string,
		ctx map[string]interface{},
		t ...time.Time)

	AddRawLogWithContext(log *protocol.Log, ctx map[string]interface{})
}
