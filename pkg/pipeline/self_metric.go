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
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type CounterMetric interface {
	Name() string

	Add(v int64)

	// Clear same with set
	Clear(v int64)

	Get() int64

	Serialize(log *protocol.Log)
}

type StringMetric interface {
	Name() string

	Set(v string)

	Get() string

	Serialize(log *protocol.Log)
}

type LatencyMetric interface {
	Name() string

	Begin()

	Clear()

	End()
	// nano second
	Get() int64

	Serialize(log *protocol.Log)
}
