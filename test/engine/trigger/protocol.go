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
package trigger

import (
	"context"
	"fmt"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

func TrigerHTTP(ctx context.Context, count int, interval int, url string) (context.Context, error) {
	logger.Debugf(context.Background(), "count:%d interval:%d url:%s", count, interval, url)
	cmd := fmt.Sprintf("curl -vL %s", url)
	time.Sleep(time.Second * 5)
	for i := 0; i < count; i++ {
		_, err := setup.Env.ExecOnSource(ctx, cmd)
		if err != nil {
			return ctx, err
		}
		time.Sleep(time.Duration(interval) * time.Millisecond)
	}
	return ctx, nil
}
