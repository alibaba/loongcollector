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

type ApplicationRegisterHandle struct {
	RegistryInformationCache
}

func (h *ApplicationRegisterHandle) ApplicationCodeRegister(context context.Context, application *agent.Application) (*agent.ApplicationMapping, error) {
	defer panicRecover()
	id, ok := h.RegistryInformationCache.applicationExist(application.GetApplicationCode())
	if !ok {
		id = h.RegistryInformationCache.registryApplication(application.GetApplicationCode())
	}

	return &agent.ApplicationMapping{
		Application: &agent.KeyWithIntegerValue{Key: application.ApplicationCode, Value: id},
	}, nil
}
