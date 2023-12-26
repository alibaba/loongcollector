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

package envconfig

import (
	"context"
	"encoding/json"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

const nginxIngressProcessor = `
{
	"KeepSource": true,
	"Keys": [
		"client_ip",
		"x_forward_for",
		"remote_user",
		"time",
		"method",
		"url",
		"version",
		"status",
		"body_bytes_sent",
		"http_referer",
		"http_user_agent",
		"request_length",
		"request_time",
		"proxy_upstream_name",
		"upstream_addr",
		"upstream_response_length",
		"upstream_response_time",
		"upstream_status",
		"req_id"
	],
	"NoKeyError": true,
	"NoMatchError": true,
	"Regex": "^(\\S+)\\s-\\s\\[([^]]+)]\\s-\\s(\\S+)\\s\\[(\\S+)\\s\\S+\\s\"(\\w+)\\s(\\S+)\\s([^\"]+)\"\\s(\\d+)\\s(\\d+)\\s\"([^\"]*)\"\\s\"([^\"]*)\"\\s(\\S+)\\s(\\S+)+\\s\\[([^]]*)]\\s(\\S+)\\s(\\S+)\\s(\\S+)\\s(\\S+)\\s(\\S+).*",

	"SourceKey": "content",
	"Type": "processor_regex"
}`

func initNginxIngress(_ *helper.K8SInfo, config *AliyunLogConfigSpec, configType string) {
	if !isDockerStdout(configType) {
		return
	}
	if config.LogtailConfig.Processors == nil {
		config.LogtailConfig.Processors = make([]map[string]interface{}, 0, 1)
	}
	nginxIngressProcessorDetail := make(map[string]interface{})
	err := json.Unmarshal(([]byte)(nginxIngressProcessor), &nginxIngressProcessorDetail)
	if err != nil {
		logger.Error(context.Background(), "LOGTAIL_CONFIG_ALARM", "invalid nginx ingress config", "processor type error")
		return
	}
	config.LogtailConfig.Processors = append(config.LogtailConfig.Processors, nginxIngressProcessorDetail)
	logger.Debug(context.Background(), "init nginx ingress config", config.LogtailConfig.Processors)
}

func initLogtailConfigForProduct(k8sInfo *helper.K8SInfo, config *AliyunLogConfigSpec, configType string) {
	if config.ProductCode == "k8s-nginx-ingress" {
		initNginxIngress(k8sInfo, config, configType)
	}
}
