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
	"fmt"
	"sync"
	"time"

	"go.uber.org/atomic"

	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
	"github.com/alibaba/ilogtail/plugins/input/skywalkingv3"
)

type Application struct {
	id                int32
	applicationName   string
	lastHeartBeatTime int64
}

type ApplicationInstance struct {
	application       *Application
	id                int32
	uuid              string
	lastHeartBeatTime int64
	host              string
	properties        map[string]string
}

type Endpoints struct {
	applicationID int32
	endpointName  string
	id            int32
}

type RegistryInformationCache interface {
	applicationExist(string) (int32, bool)
	instanceExist(int32, string) (int32, bool)

	registryApplications([]*agent.Service) map[string]int32
	registryApplication(string) int32

	registryApplicationInstance(int32, string, *agent.OSInfo) int32
	registryApplicationInstances(instances []*agent.ServiceInstance) map[string]int32

	registryNetworkAddresses(addresses []string) map[string]int32
	registryEndpoints(endpoints []*agent.Endpoint) []*Endpoints
	registryEndpoint(applicationID int32, endpoint string) *Endpoints

	pingApplicationAndInstance(int32)

	findApplicationRegistryInfo(s string) (*Application, bool)
	findApplicationInstanceRegistryInfo(int32) (*ApplicationInstance, bool)
	findApplicationRegistryInfoByID(s int32) (*Application, bool)
	findEndpointRegistryInfoByID(s int32) (*Endpoints, bool)

	ServiceAndNetworkAddressMappingExist(mappings []*agent.ServiceAndNetworkAddressMapping) (interface{}, interface{})
}

func NewRegistryInformationCache() RegistryInformationCache {
	return &registryInformationCacheImpl{}
}

type registryInformationCacheImpl struct {
	applicationRegistryInformationCache applicationRegistryInformationCacheImpl
	endpointRegistryInformationCache    endpointRegistryInformationCacheImpl
	instanceRegistryInformationCache    instanceRegistryInformationCacheImpl
	networkRegistryInformationCache     networkRegistryInformationCacheImpl
}

type applicationRegistryInformationCacheImpl struct {
	keyToValueCache sync.Map
	idToValueCache  sync.Map

	index atomic.Int32
}

type endpointRegistryInformationCacheImpl struct {
	keyToValueCache sync.Map
	idToValueCache  sync.Map

	index atomic.Int32
}

type instanceRegistryInformationCacheImpl struct {
	keyToValueCache sync.Map
	idToValueCache  sync.Map

	index atomic.Int32
}

type networkRegistryInformationCacheImpl struct {
	index atomic.Int32
}

func (r *registryInformationCacheImpl) applicationExist(s string) (int32, bool) {
	registryInfo, ok := r.findApplicationRegistryInfo(s)
	if !ok {
		return 0, ok
	}
	return registryInfo.id, true
}

func (r *registryInformationCacheImpl) registryApplications(services []*agent.Service) map[string]int32 {
	var result = make(map[string]int32)
	for _, service := range services {
		id, ok := r.applicationExist(service.GetServiceName())
		if ok {
			result[service.GetServiceName()] = id
			continue
		}

		id = r.applicationRegistryInformationCache.index.Inc()
		application := &Application{
			id:                id,
			applicationName:   service.GetServiceName(),
			lastHeartBeatTime: time.Now().Unix(),
		}

		r.applicationRegistryInformationCache.keyToValueCache.Store(service.GetServiceName(), application)
		r.applicationRegistryInformationCache.idToValueCache.Store(id, application)
		result[service.GetServiceName()] = id
	}
	return result
}

func (r *registryInformationCacheImpl) instanceExist(applicationID int32, instanceUUID string) (int32, bool) {
	_, ok := r.findApplicationRegistryInfoByID(applicationID)
	if !ok {
		return 0, false
	}

	a, ok := r.instanceRegistryInformationCache.keyToValueCache.Load(r.instanceMappingName(applicationID, instanceUUID))
	if !ok {
		return 0, false
	}
	return a.(*ApplicationInstance).id, true
}

func (r *registryInformationCacheImpl) registryApplicationInstances(instances []*agent.ServiceInstance) map[string]int32 {
	var result = make(map[string]int32)
	for _, instance := range instances {
		id, ok := r.instanceExist(instance.GetServiceId(), instance.GetInstanceUUID())
		if ok {
			result[instance.GetInstanceUUID()] = id
			continue
		}

		application, ok := r.findApplicationRegistryInfoByID(instance.GetServiceId())
		if !ok {
			continue
		}

		id = r.instanceRegistryInformationCache.index.Inc()
		var properties = make(map[string]string)
		for _, item := range instance.GetProperties() {
			switch item.Key {
			case "os_name":
				properties[skywalkingv3.AttributeOSType] = item.Value
			case "host_name":
				properties[skywalkingv3.AttributeHostName] = item.Value
			case "process_no":
				properties[skywalkingv3.AttributeProcessID] = item.Value
			case "language":
				properties[skywalkingv3.AttributeTelemetrySDKLanguage] = item.Value
			case "namespace":
				properties[skywalkingv3.AttributeServiceNamespace] = item.Value
			}
		}

		applicationInstance := &ApplicationInstance{
			application:       application,
			id:                id,
			uuid:              instance.InstanceUUID,
			lastHeartBeatTime: time.Now().Unix(),
			properties:        properties,
			host:              properties[skywalkingv3.AttributeHostName],
		}

		r.instanceRegistryInformationCache.keyToValueCache.Store(r.instanceMappingName(instance.GetServiceId(), instance.GetInstanceUUID()), applicationInstance)
		r.instanceRegistryInformationCache.idToValueCache.Store(id, applicationInstance)
		result[instance.GetInstanceUUID()] = id

	}
	return result
}

func (r *registryInformationCacheImpl) registryApplication(s string) int32 {
	a, ok := r.findApplicationRegistryInfo(s)
	if ok {
		return a.id
	}

	id := r.applicationRegistryInformationCache.index.Inc()
	application := &Application{
		id:                id,
		applicationName:   s,
		lastHeartBeatTime: time.Now().Unix(),
	}

	r.applicationRegistryInformationCache.idToValueCache.Store(id, application)
	r.applicationRegistryInformationCache.keyToValueCache.Store(s, application)

	return id
}

func (r *registryInformationCacheImpl) registryApplicationInstance(applicationID int32, agentUUID string, info *agent.OSInfo) int32 {
	application, ok := r.findApplicationRegistryInfoByID(applicationID)
	if !ok {
		return 0
	}

	var properties = make(map[string]string)
	properties[skywalkingv3.AttributeOSType] = info.GetOsName()
	properties[skywalkingv3.AttributeHostName] = info.GetHostname()
	properties[skywalkingv3.AttributeProcessID] = string(info.GetProcessNo())

	id := r.instanceRegistryInformationCache.index.Inc()
	applicationInstance := &ApplicationInstance{
		application:       application,
		id:                id,
		uuid:              agentUUID,
		lastHeartBeatTime: time.Now().Unix(),
		properties:        properties,
		host:              properties[skywalkingv3.AttributeHostName],
	}

	r.instanceRegistryInformationCache.idToValueCache.Store(id, applicationInstance)
	r.instanceRegistryInformationCache.keyToValueCache.Store(r.instanceMappingName(applicationID, agentUUID), applicationInstance)
	return id
}

func (r *registryInformationCacheImpl) pingApplicationAndInstance(instanceID int32) {
	i, ok := r.instanceRegistryInformationCache.idToValueCache.Load(instanceID)
	if !ok {
		return
	}

	instance := i.(*ApplicationInstance)
	instance.lastHeartBeatTime = time.Now().Unix()
	instance.application.lastHeartBeatTime = time.Now().Unix()
}

func (r *registryInformationCacheImpl) findApplicationInstanceRegistryInfo(instanceID int32) (*ApplicationInstance, bool) {
	i, ok := r.instanceRegistryInformationCache.idToValueCache.Load(instanceID)
	if !ok {
		return nil, false
	}
	return i.(*ApplicationInstance), true
}

func (r *registryInformationCacheImpl) registryNetworkAddresses(addresses []string) map[string]int32 {
	var result = make(map[string]int32)

	for _, address := range addresses {
		id := r.networkRegistryInformationCache.index.Inc()
		result[address] = id
	}

	return result
}

func (r *registryInformationCacheImpl) registryEndpoints(endpoints []*agent.Endpoint) []*Endpoints {
	var result = make([]*Endpoints, 0, len(endpoints))

	for _, endpoint := range endpoints {
		if e := r.registryEndpoint(endpoint.GetServiceId(), endpoint.GetEndpointName()); e != nil {
			result = append(result, e)
		}
	}

	return result
}

func (r *registryInformationCacheImpl) registryEndpoint(applicationID int32, endpoint string) *Endpoints {
	_, ok := r.findApplicationRegistryInfoByID(applicationID)
	if !ok {
		return nil
	}

	ei, ok := r.findEndpointRegistryInfo(r.endpointMappingName(applicationID, endpoint))
	if ok {
		return ei
	}

	id := r.endpointRegistryInformationCache.index.Inc()
	e := &Endpoints{
		applicationID: applicationID,
		endpointName:  endpoint,
		id:            id,
	}

	r.endpointRegistryInformationCache.keyToValueCache.Store(r.endpointMappingName(applicationID, endpoint), e)
	r.endpointRegistryInformationCache.idToValueCache.Store(id, e)

	return e
}

func (r *registryInformationCacheImpl) ServiceAndNetworkAddressMappingExist(mappings []*agent.ServiceAndNetworkAddressMapping) (interface{}, interface{}) {
	return nil, nil
}

func (r *registryInformationCacheImpl) findApplicationRegistryInfo(s string) (*Application, bool) {
	a, ok := r.applicationRegistryInformationCache.keyToValueCache.Load(s)
	if !ok {
		return nil, false
	}

	return a.(*Application), true
}

func (r *registryInformationCacheImpl) findEndpointRegistryInfo(endpoint string) (*Endpoints, bool) {
	a, ok := r.endpointRegistryInformationCache.keyToValueCache.Load(endpoint)
	if !ok {
		return nil, false
	}

	return a.(*Endpoints), true
}

func (r *registryInformationCacheImpl) findEndpointRegistryInfoByID(s int32) (*Endpoints, bool) {
	a, ok := r.endpointRegistryInformationCache.idToValueCache.Load(s)
	if !ok {
		return nil, false
	}

	return a.(*Endpoints), true
}

func (r *registryInformationCacheImpl) findApplicationRegistryInfoByID(s int32) (*Application, bool) {
	a, ok := r.applicationRegistryInformationCache.idToValueCache.Load(s)
	if !ok {
		return nil, false
	}

	return a.(*Application), true
}

func (r *registryInformationCacheImpl) instanceMappingName(applicationID int32, instanceUUID string) string {
	return fmt.Sprintf("%d-i-%s", applicationID, instanceUUID)
}

func (r *registryInformationCacheImpl) endpointMappingName(applicationID int32, endpoint string) string {
	return fmt.Sprintf("%d-e-%s", applicationID, endpoint)
}
