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

package helper

import (
	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"

	"context"
	"runtime"
)

func panicRecover(cxt context.Context, key string) {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(cxt, "PLUGIN_RUNTIME_ALARM", "key", key, "panicked", err, "stack", string(trace))
	}
}

// StartService ..
func StartService(name string, context ilogtail.Context, f func()) {
	go func() {
		logger.Info(context.GetRuntimeContext(), "service begin", name)
		defer panicRecover(context.GetRuntimeContext(), name)
		f()
		logger.Info(context.GetRuntimeContext(), "service done", name)
	}()
}
