// Copyright 2024 iLogtail Authors
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

//go:build !enterprise

package pluginmanager

import "github.com/alibaba/ilogtail/pkg/util"

const (
	hostNameDefaultTagKey    = "__hostname__"
	hostIPDefaultTagKey      = "__host_ip__"
	defaultConfigTagKeyValue = "__default__"
)

func (p *ProcessorTag) parseAllConfigurableTags(pipelineMetaTagKey map[string]string) {
	p.parseDefaultAddedTag("HOST_NAME", TagKeyHostName, hostNameDefaultTagKey, pipelineMetaTagKey)
	p.parseDefaultAddedTag("HOST_IP", TagKeyHostIP, hostIPDefaultTagKey, pipelineMetaTagKey)
}

// should keep same with C++ ProcessorTagNative::Process
func (p *ProcessorTag) addAllConfigurableTags(tagsMap map[string]string) {
	p.addTag(TagKeyHostName, util.GetHostName(), tagsMap)
	p.addTag(TagKeyHostIP, util.GetIPAddress(), tagsMap)
}
