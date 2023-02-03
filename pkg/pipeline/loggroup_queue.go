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

// LogGroupQueue for aggregator, Non blocked
// if aggregator's buffer is full, aggregator can add LogGroup to this queue
// return error if LogGroupQueue is full
type LogGroupQueue interface {
	// no blocking
	Add(loggroup *protocol.LogGroup) error
	AddWithWait(loggroup *protocol.LogGroup, duration time.Duration) error
}
