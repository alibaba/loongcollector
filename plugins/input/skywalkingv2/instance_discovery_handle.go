// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package skywalkingv2

import (
	"context"

	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
)

type InstanceDiscoveryServiceHandle struct {
	RegistryInformationCache
}

func (i InstanceDiscoveryServiceHandle) RegisterInstance(ctx context.Context, instance *agent.ApplicationInstance) (*agent.ApplicationInstanceMapping, error) {
	defer panicRecover()
	id, ok := i.RegistryInformationCache.instanceExist(instance.GetApplicationId(), instance.GetAgentUUID())
	if !ok {
		id = i.RegistryInformationCache.registryApplicationInstance(instance.GetApplicationId(), instance.GetAgentUUID(), instance.GetOsinfo())
	}
	return &agent.ApplicationInstanceMapping{ApplicationId: instance.GetApplicationId(), ApplicationInstanceId: id}, nil
}

func (i InstanceDiscoveryServiceHandle) Heartbeat(ctx context.Context, heartbeat *agent.ApplicationInstanceHeartbeat) (*agent.Downstream, error) {
	defer panicRecover()
	i.RegistryInformationCache.pingApplicationAndInstance(heartbeat.GetApplicationInstanceId())
	return &agent.Downstream{}, nil
}
