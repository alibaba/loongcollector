// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package cleanup

import (
	"context"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/alibaba/ilogtail/test/engine/control"
	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

func HandleSignal() {
	if setup.Env == nil {
		return
	}
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigs
		All()
		os.Exit(0)
	}()
}

func All() {
	if setup.Env == nil {
		return
	}
	ctx := context.TODO()
	_, _ = control.RemoveAllLocalConfig(ctx)
	_, _ = AllGeneratedLog(ctx)
	_, _ = GoTestCache(ctx)
	_, _ = DeleteContainers(ctx)
	if subscriber.TestSubscriber != nil {
		_ = subscriber.TestSubscriber.Stop()
	}
	time.Sleep(5 * time.Second)
}
