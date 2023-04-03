// Copyright 2022 iLogtail Authors
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

package jmxfetch

import (
	"sort"
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/util"
)

type InstanceInner struct {
	Port              int32    `yaml:"port,omitempty"`
	Host              string   `yaml:"host,omitempty"`
	User              string   `yaml:"user,omitempty"`
	Password          string   `yaml:"password,omitempty"`
	Tags              []string `yaml:"tags,omitempty"`
	Name              string   `yaml:"name"`
	DefaultJvmMetrics bool     `yaml:"collect_default_jvm_metrics"`
}

func (i *InstanceInner) Hash() string {
	var hashStrBuilder strings.Builder
	hashStrBuilder.WriteString(i.Host)
	hashStrBuilder.WriteString(strconv.Itoa(int(i.Port)))
	for idx := range i.Tags {
		hashStrBuilder.WriteString(i.Tags[idx])
	}
	return hashStrBuilder.String()
}

func NewInstanceInner(port int32, host, user, passowrd string, tags map[string]string, defaultJvmMetrics bool) *InstanceInner {
	var hostname, instance string
	_ = util.InitFromEnvString("_node_name_", &hostname, util.GetHostName())
	tags["hostname"] = hostname

	if host == "localhost" || host == "127.0.0.1" {
		instance = util.GetHostName() + "_" + strconv.Itoa(int(port))
	} else {
		instance = host + "_" + strconv.Itoa(int(port))
	}
	helper.ReplaceInvalidChars(&instance)

	if _, ok := tags["service"]; !ok {
		tags["service"] = hostname
	}
	tagsArr := make([]string, 0, len(tags))
	for k, v := range tags {
		tagsArr = append(tagsArr, k+":"+v)
	}

	sort.Strings(tagsArr)
	return &InstanceInner{
		Port:              port,
		Host:              host,
		User:              user,
		Password:          passowrd,
		Tags:              tagsArr,
		DefaultJvmMetrics: defaultJvmMetrics,
		Name:              instance,
	}
}

type FilterInner struct {
	Domain    string      `yaml:"domain,omitempty"`
	BeanRegex string      `yaml:"bean_regex,omitempty"`
	Type      string      `yaml:"type,omitempty"`
	Name      string      `yaml:"name,omitempty"`
	Attribute interface{} `yaml:"attribute,omitempty"`
}

func NewFilterInner(filter *Filter) *FilterInner {
	f := &FilterInner{
		Domain:    filter.Domain,
		BeanRegex: filter.BeanRegex,
		Type:      filter.Type,
		Name:      filter.Name,
	}

	if len(filter.Attribute) > 0 {
		listMode := false
		for _, attribute := range filter.Attribute {
			if attribute.MetricType == "" || attribute.Alias == "" {
				listMode = true
				break
			}
		}
		if listMode {
			var names []string
			for _, attribute := range filter.Attribute {
				names = append(names, attribute.Name)
			}
			f.Attribute = names
		} else {
			attrubutes := make(map[string]map[string]string)
			for _, attribute := range filter.Attribute {
				attrubutes[attribute.Name] = map[string]string{
					"metric_type": attribute.MetricType,
					"alias":       attribute.Alias,
				}
			}
			f.Attribute = attrubutes
		}
	}
	return f
}

type Cfg struct {
	include      []*FilterInner
	instances    map[string]*InstanceInner
	newGcMetrics bool
	change       bool
}

func NewCfg(filters []*FilterInner) *Cfg {
	return &Cfg{
		instances: map[string]*InstanceInner{},
		include:   filters,
	}
}
