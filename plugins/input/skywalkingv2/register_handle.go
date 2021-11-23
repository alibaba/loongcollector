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

	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/common"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
)

type RegisterHandle struct {
	RegistryInformationCache
}

func (r RegisterHandle) DoServiceAndNetworkAddressMappingRegister(ctx context.Context, mappings *agent.ServiceAndNetworkAddressMappings) (*common.Commands, error) {
	return nil, nil
}

func (r RegisterHandle) DoServiceRegister(ctx context.Context, services *agent.Services) (*agent.ServiceRegisterMapping, error) {
	defer panicRecover()
	idMappings := r.RegistryInformationCache.registryApplications(services.GetServices())
	var pair []*common.KeyIntValuePair
	for key, value := range idMappings {
		pair = append(pair, &common.KeyIntValuePair{Key: key, Value: value})
	}

	return &agent.ServiceRegisterMapping{Services: pair}, nil
}

func (r RegisterHandle) DoServiceInstanceRegister(ctx context.Context, instances *agent.ServiceInstances) (*agent.ServiceInstanceRegisterMapping, error) {
	defer panicRecover()
	idMappings := r.RegistryInformationCache.registryApplicationInstances(instances.GetInstances())

	var pair []*common.KeyIntValuePair
	for key, value := range idMappings {
		pair = append(pair, &common.KeyIntValuePair{Key: key, Value: value})
	}

	return &agent.ServiceInstanceRegisterMapping{ServiceInstances: pair}, nil
}

func (r RegisterHandle) DoEndpointRegister(ctx context.Context, endpoints *agent.Endpoints) (*agent.EndpointMapping, error) {
	defer panicRecover()
	idMappings := r.RegistryInformationCache.registryEndpoints(endpoints.GetEndpoints())

	var pair = make([]*agent.EndpointMappingElement, 0, len(idMappings))
	for _, endpoint := range idMappings {
		pair = append(pair, &agent.EndpointMappingElement{EndpointId: endpoint.id, EndpointName: endpoint.endpointName, ServiceId: endpoint.applicationID})
	}

	return &agent.EndpointMapping{Elements: pair}, nil
}

func (r RegisterHandle) DoNetworkAddressRegister(ctx context.Context, addresses *agent.NetAddresses) (*agent.NetAddressMapping, error) {
	defer panicRecover()
	idMappings := r.RegistryInformationCache.registryNetworkAddresses(addresses.GetAddresses())

	var pair = make([]*common.KeyIntValuePair, 0, len(idMappings))
	for key, value := range idMappings {
		pair = append(pair, &common.KeyIntValuePair{Key: key, Value: value})
	}

	return &agent.NetAddressMapping{AddressIds: pair}, nil
}
