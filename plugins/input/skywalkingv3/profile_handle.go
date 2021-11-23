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

package skywalkingv3

import (
	"context"

	common "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	profile "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/language/profile/v3"
)

type ProfileHandler struct{}

func (*ProfileHandler) GetProfileTaskCommands(ctx context.Context, req *profile.ProfileTaskCommandQuery) (*common.Commands, error) {
	return &common.Commands{}, nil
}
func (*ProfileHandler) CollectSnapshot(srv profile.ProfileTask_CollectSnapshotServer) error {
	return nil
}
func (*ProfileHandler) ReportTaskFinish(ctx context.Context, req *profile.ProfileTaskFinishReport) (*common.Commands, error) {
	return &common.Commands{}, nil
}
