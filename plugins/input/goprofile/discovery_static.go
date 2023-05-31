// Copyright 2023 iLogtail Authors
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

package goprofile

import (
	"errors"
	"net"
	"strconv"

	"github.com/pyroscope-io/pyroscope/pkg/scrape/discovery"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/discovery/targetgroup"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/model"
)

type StaticConfig struct {
	Addresses []Address

	labelSet model.LabelSet
}

type Address struct {
	Host           string
	Port           int32
	InstanceLabels map[string]string
}

func (k *StaticConfig) Name() string {
	return "static_ilogtail"
}

func (k *StaticConfig) NewDiscoverer(options discovery.DiscovererOptions) (discovery.Discoverer, error) {
	config, err := k.convertStaticConfig()
	if err != nil {
		return nil, err
	}
	return config.NewDiscoverer(options)
}

func (k *StaticConfig) convertStaticConfig() (discovery.StaticConfig, error) {
	cfg := make(discovery.StaticConfig, 0, len(k.Addresses))
	for _, address := range k.Addresses {

		var appName model.LabelValue
		if value, ok := k.labelSet[model.LabelName("service")]; ok {
			appName = value
			delete(k.labelSet, "service")
		} else if value, ok := address.InstanceLabels["service"]; ok {
			appName = model.LabelValue(value)
			delete(address.InstanceLabels, "service")
		} else {
			return nil, errors.New("not found required service labels")
		}
		addr := net.JoinHostPort(address.Host, strconv.Itoa(int(address.Port)))
		innerLabels := make(model.LabelSet)
		innerLabels[model.AddressLabel] = model.LabelValue(addr)
		innerLabels[model.AppNameLabel] = appName

		for key, val := range address.InstanceLabels {
			innerLabels[model.LabelName(key)] = model.LabelValue(val)
		}
		cfg = append(cfg, &targetgroup.Group{
			Source:  addr,
			Labels:  k.labelSet,
			Targets: []model.LabelSet{innerLabels},
		})
	}
	return cfg, nil
}
