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

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/pkg/logger"
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
	r.cache[key] = properties
	return true
}

func (r *ResourcePropertiesCache) save(ctx ilogtail.Context) {
	r.lock.Lock()
	jsonBytes, _ := json.Marshal(r.cache)
	r.lock.Unlock()
	err := ctx.SaveCheckPoint(r.cacheKey, jsonBytes)
	if err != nil {
		logger.Error(ctx.GetRuntimeContext(), "SKYWALKING_SAVE_CHECKPOINT_FAIL", "err", err.Error())
	} else {
		logger.Info(ctx.GetRuntimeContext(), "msg", "skywalking save checkpoint done")
	}
}

func (r *ResourcePropertiesCache) load(ctx ilogtail.Context) bool {
	bytes, ok := ctx.GetCheckPoint(r.cacheKey)
	if ok {
		err := json.Unmarshal(bytes, &r.cache)
		if err != nil {
			logger.Error(ctx.GetRuntimeContext(), "SKYWALKING_LOAD_CHECKPOINT_FAIL", "err", err.Error())
			return false
		}
		logger.Info(ctx.GetRuntimeContext(), "msg", "skywalking load checkpoint done", "count", len(r.cache))
	}
	return true
}

type ManagementHandler struct {
	context   ilogtail.Context
	collector ilogtail.Collector
	cache     *ResourcePropertiesCache
}

func NewManagementHandler(context ilogtail.Context, collector ilogtail.Collector, cache *ResourcePropertiesCache) *ManagementHandler {
	return &ManagementHandler{
		context:   context,
		collector: collector,
		cache:     cache,
	}
}

func (m *ManagementHandler) ReportInstanceProperties(ctx context.Context, req *management.InstanceProperties) (*v3.Commands, error) {
	defer panicRecover()
	propertyMap := ConvertResourceOt(req)
	if m.cache.put(req.Service, req.ServiceInstance, propertyMap) {
		m.cache.save(m.context)
	}
	return &v3.Commands{}, nil
}
func (*ManagementHandler) KeepAlive(ctx context.Context, req *management.InstancePingPkg) (*v3.Commands, error) {
	return &v3.Commands{}, nil
}
