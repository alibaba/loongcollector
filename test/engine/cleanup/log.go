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
	"fmt"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
)

func AllGeneratedLog(ctx context.Context) (context.Context, error) {
	command := fmt.Sprintf("rm -rf %s/*", config.TestConfig.GeneratedLogDir)
	if _, err := setup.Env.ExecOnSource(ctx, command); err != nil {
		return ctx, err
	}
	return ctx, nil
}
