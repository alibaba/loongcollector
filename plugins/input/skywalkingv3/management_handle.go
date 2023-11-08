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
	"encoding/json"
	"reflect"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	management "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/management/v3"
)

type ResourcePropertiesCache struct {
	cache    map[string]map[string]string
	cacheKey string
	lock     sync.Mutex
}

const CheckpointKey string = "skywalking_instance_properties"

func (r *ResourcePropertiesCache) get(service, serviceInstance string) map[string]string {
	defer r.lock.Unlock()
	r.lock.Lock()
	return r.cache[service+serviceInstance]
}

// return need to save
func (r *ResourcePropertiesCache) put(service string, serviceInstance string, properties map[string]string) bool {
	key := service + serviceInstance
	defer r.lock.Unlock()
	r.lock.Lock()

	if vals, ok := r.cache[key]; ok {
		if reflect.DeepEqual(vals, properties) {
			return false
		}
	}
	r.cache[key] = r.filterProperties(properties)
	return true
}

func (r *ResourcePropertiesCache) filterProperties(properties map[string]string) map[string]string {
	if properties == nil {
		return make(map[string]string)
	}

	delete(properties, "Start Time")
	delete(properties, "JVM Arguments")
	delete(properties, "Jar Dependencies")

	if properties["namespace"] != "" {
		properties[AttributeServiceNamespace] = properties["namespace"]
		delete(properties, "namespace")
	}

	return properties
}

func (r *ResourcePropertiesCache) save(ctx pipeline.Context) {
	r.lock.Lock()
	jsonBytes, _ := json.Marshal(r.cache)
	r.lock.Unlock()
	err := ctx.SaveCheckPoint(r.cacheKey, jsonBytes)
	if err != nil {
		logger.Error(ctx.GetRuntimeContext(), "SKYWALKING_SAVE_CHECKPOINT_FAIL", "err", err.Error())
	}
}

func (r *ResourcePropertiesCache) load(ctx pipeline.Context) bool {
	bytes, ok := ctx.GetCheckPoint(r.cacheKey)
	if ok {
		err := json.Unmarshal(bytes, &r.cache)
		if err != nil {
			logger.Error(ctx.GetRuntimeContext(), "SKYWALKING_LOAD_CHECKPOINT_FAIL", "err", err.Error())
			return false
		}
	}
	return true
}

type ManagementHandler struct {
	context   pipeline.Context
	collector pipeline.Collector
	cache     *ResourcePropertiesCache
}

func (m *ManagementHandler) keepAliveHandler(req *management.InstancePingPkg) (*v3.Commands, error) {
	return &v3.Commands{}, nil
}

func (m *ManagementHandler) reportInstanceProperties(instanceProperties *management.InstanceProperties) (result *v3.Commands, e error) {
	defer panicRecover()
	propertyMap := ConvertResourceOt(instanceProperties)
	if m.cache.put(instanceProperties.Service, instanceProperties.ServiceInstance, propertyMap) {
		m.cache.save(m.context)
	}
	return &v3.Commands{}, nil
}

type keepAliveHandler interface {
	keepAliveHandler(req *management.InstancePingPkg) (*v3.Commands, error)
}

type reportInstancePropertiesHandler interface {
	reportInstanceProperties(instanceProperties *management.InstanceProperties) (result *v3.Commands, e error)
}

func NewManagementHandler(context pipeline.Context, collector pipeline.Collector, cache *ResourcePropertiesCache) *ManagementHandler {
	return &ManagementHandler{
		context:   context,
		collector: collector,
		cache:     cache,
	}
}

func (m *ManagementHandler) ReportInstanceProperties(ctx context.Context, req *management.InstanceProperties) (*v3.Commands, error) {
	return m.reportInstanceProperties(req)
}
func (*ManagementHandler) KeepAlive(ctx context.Context, req *management.InstancePingPkg) (*v3.Commands, error) {
	return &v3.Commands{}, nil
}
