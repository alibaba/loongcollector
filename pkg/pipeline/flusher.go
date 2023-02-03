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
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

// Flusher ...
// Sample flusher implementation: see plugin_manager/flusher_sls.gox.
type Flusher interface {
	// Init called for init some system resources, like socket, mutex...
	Init(Context) error

	// Description returns a one-sentence description on the Input.
	Description() string

	// IsReady checks if flusher is ready to accept more data.
	// @projectName, @logstoreName, @logstoreKey: meta of the corresponding data.
	// Note: If SetUrgent is called, please make some adjustment so that IsReady
	//   can return true to accept more data in time and config instance can be
	//   stopped gracefully.
	IsReady(projectName string, logstoreName string, logstoreKey int64) bool

	// SetUrgent indicates the flusher that it will be destroyed soon.
	// @flag indicates if main program (Logtail mostly) will exit after calling this.
	//
	// Note: there might be more data to flush after SetUrgent is called, and if flag
	//   is true, these data will be passed to flusher through IsReady/Export before
	//   program exits.
	//
	// Recommendation: set some state flags in this method to guide the behavior
	//   of other methods.
	SetUrgent(flag bool)

	// Stop stops flusher and release resources.
	// It is time for flusher to do cleaning jobs, includes:
	// 1. Export cached but not flushed data. For flushers that contain some kinds of
	//   aggregation or buffering, it is important to flush cached out now, otherwise
	//   data will lost.
	// 2. Release opened resources: goroutines, file handles, connections, etc.
	// 3. Maybe more, it depends.
	// In a word, flusher should only have things that can be recycled by GC after this.
	Stop() error
}

type FlusherV1 interface {
	Flusher
	// Flush flushes data to destination, such as SLS, console, file, etc.
	// It is expected to return no error at most time because IsReady will be called
	// before it to make sure there is space for next data.
	Flush(projectName string, logstoreName string, configName string, logGroupList []*protocol.LogGroup) error
}

type FlusherV2 interface {
	Flusher
	// Export data to destination, such as gRPC, console, file, etc.
	// It is expected to return no error at most time because IsReady will be called
	// before it to make sure there is space for next data.
	Export([]*models.PipelineGroupEvents, PipelineContext) error
}
