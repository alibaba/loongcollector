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
	"context"
	"net"
	"regexp"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	"github.com/pyroscope-io/pyroscope/pkg/scrape/discovery"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/discovery/targetgroup"
	"github.com/pyroscope-io/pyroscope/pkg/scrape/model"
)

type KubernetesConfig struct {
	IncludeEnv            map[string]string
	ExcludeEnv            map[string]string
	IncludeContainerLabel map[string]string
	ExcludeContainerLabel map[string]string
	IncludeK8sLabel       map[string]string
	ExcludeK8sLabel       map[string]string
	ExternalEnvTag        map[string]string
	ExternalK8sLabelTag   map[string]string
	K8sNamespaceRegex     string
	K8sPodRegex           string
	K8sContainerRegex     string

	includeContainerLabelRegex map[string]*regexp.Regexp
	excludeContainerLabelRegex map[string]*regexp.Regexp
	includeEnvRegex            map[string]*regexp.Regexp
	excludeEnvRegex            map[string]*regexp.Regexp
	k8sFilter                  *helper.K8SFilter

	labelSet model.LabelSet
}

func (k *KubernetesConfig) Name() string {
	return "kubernetes_ilogtail"
}

func (k *KubernetesConfig) NewDiscoverer(options discovery.DiscovererOptions) (discovery.Discoverer, error) {
	k.InitDiscoverer()
	return k, nil
}

func (k *KubernetesConfig) InitDiscoverer() {
	helper.ContainerCenterInit()
	var err error
	k.IncludeEnv, k.includeEnvRegex, err = helper.SplitRegexFromMap(k.IncludeEnv)
	if err != nil {
		logger.Warning(context.Background(), "INVALID_REGEX_ALARM", "init include env regex error", err)
	}
	k.ExcludeEnv, k.excludeEnvRegex, err = helper.SplitRegexFromMap(k.ExcludeEnv)
	if err != nil {
		logger.Warning(context.Background(), "INVALID_REGEX_ALARM", "init exclude env regex error", err)
	}
	k.IncludeContainerLabel, k.includeContainerLabelRegex, err = helper.SplitRegexFromMap(k.IncludeContainerLabel)
	if err != nil {
		logger.Warning(context.Background(), "INVALID_REGEX_ALARM", "init include label regex error", err)
	}
	k.ExcludeContainerLabel, k.excludeContainerLabelRegex, err = helper.SplitRegexFromMap(k.ExcludeContainerLabel)
	if err != nil {
		logger.Warning(context.Background(), "INVALID_REGEX_ALARM", "init exclude label regex error", err)
	}
	k.k8sFilter, err = helper.CreateK8SFilter(k.K8sNamespaceRegex, k.K8sPodRegex, k.K8sContainerRegex, k.IncludeK8sLabel, k.ExcludeK8sLabel)
	if err != nil {
		logger.Warning(context.Background(), "INVALID_REGEX_ALARM", "init k8s filter error", err)
	}
	logger.Debug(context.Background(), "profile_go_kubernetes inited successfully")
}

func (k *KubernetesConfig) Run(ctx context.Context, up chan<- []*targetgroup.Group) {
	defer func() {
		logger.Info(ctx, "stop ilogtail kubernetes discovery")
	}()
	ticker := time.NewTicker(time.Second)
	for {
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
			containers := helper.GetContainerByAcceptedInfo(k.IncludeContainerLabel, k.ExcludeContainerLabel,
				k.includeContainerLabelRegex, k.excludeContainerLabelRegex, k.IncludeEnv, k.ExcludeEnv,
				k.includeEnvRegex, k.excludeEnvRegex, k.k8sFilter)
			if logger.DebugFlag() {
				for id, detail := range containers {
					logger.Debugf(context.Background(), "go profile found containers: %s %+v", id, detail)
				}
			}
			groups := k.convertContainers2Group(containers)
			if logger.DebugFlag() {
				for _, group := range groups {
					logger.Debugf(context.Background(), "go profile detect groups:%+v", group)
				}
			}
			up <- groups
		}
	}
}

func (k *KubernetesConfig) convertContainers2Group(containers map[string]*helper.DockerInfoDetail) []*targetgroup.Group {
	res := make([]*targetgroup.Group, 0, len(containers))

	for id, detail := range containers {
		val := detail.GetEnv("ILOGTAIL_PROFILE_PORT")
		if val == "" {
			continue
		}
		if _, err := strconv.Atoi(val); err != nil {
			logger.Debug(context.Background(), "ignore container because invalid ", detail.PodName())
			continue
		}
		if detail.K8SInfo.Pod == "" {
			logger.Debug(context.Background(), "kubernetes pod name not found ", id)
			continue
		}
		var g targetgroup.Group
		addr := net.JoinHostPort(detail.ContainerIP, val)
		target := model.LabelSet{
			model.AddressLabel: model.LabelValue(addr),
			model.AppNameLabel: model.LabelValue(helper.ExtractPodWorkload(detail.K8SInfo.Pod)),
			"namespace":        model.LabelValue(detail.K8SInfo.Namespace),
			"pod":              model.LabelValue(detail.K8SInfo.Pod),
			"container":        model.LabelValue(detail.K8SInfo.ContainerName),
		}
		tags := make(map[string]string)
		detail.GetCustomExternalTags(tags, k.ExternalEnvTag, k.ExternalK8sLabelTag)
		for k, v := range tags {
			target[model.LabelName(k)] = model.LabelValue(v)
		}
		g.Targets = append(g.Targets, target)
		g.Labels = k.labelSet
		g.Source = id
		res = append(res, &g)
	}
	return res
}
