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

	"github.com/alibaba/ilogtail/test/engine/setup"
)

func DeleteContainers(ctx context.Context) (context.Context, error) {
	if setup.Env.GetType() == "docker-compose" {
		// Delete containers
		dockerComposeEnv, ok := setup.Env.(*setup.DockerComposeEnv)
		if !ok {
			return ctx, nil
		}
		err := dockerComposeEnv.Clean()
		if err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}
