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
package chaos

import (
	"context"
	"fmt"
	"strconv"

	"github.com/alibaba/ilogtail/test/engine/setup"
)

func DiskFull(ctx context.Context, time int) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade create disk fill --path /usr/local/ilogtail/ --size 10240000 --timeout " + strconv.FormatInt(int64(time), 10)
		_, err := setup.Env.ExecOnLoongCollector(command)
		if err != nil {
			return ctx, err
		}
	default:
		return ctx, fmt.Errorf("not supported")
	}
	return ctx, nil
}

func DiskBurRead(ctx context.Context, time int) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade create disk burn --read --path /usr/local/ilogtail/ --timeout " + strconv.FormatInt(int64(time), 10)
		_, err := setup.Env.ExecOnLoongCollector(command)
		if err != nil {
			return ctx, err
		}
	default:
		return ctx, fmt.Errorf("not supported")
	}
	return ctx, nil
}

func DiskBurWrite(ctx context.Context, time int) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade create disk burn  --write --path /usr/local/ilogtai/ --timeout " + strconv.FormatInt(int64(time), 10)
		_, err := setup.Env.ExecOnLoongCollector(command)
		if err != nil {
			return ctx, err
		}
	default:
		return ctx, fmt.Errorf("not supported")
	}
	return ctx, nil
}
