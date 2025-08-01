// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package selfmonitor

type AlarmType string

func (a AlarmType) String() string {
	return string(a)
}

// todo: add more alarm type
const (
	InputCollectAlarm    AlarmType = "INPUT_COLLECT_ALARM"
	CategoryConfigAlarm  AlarmType = "CATEGORY_CONFIG_ALARM"
	ContainerCenterAlarm AlarmType = "CONTAINER_CENTER_ALARM"
)

type AlarmLevel string

func (a AlarmLevel) String() string {
	return string(a)
}

func (a AlarmLevel) IsValid() bool {
	switch a {
	case AlarmLevelWaring, AlarmLevelError, AlarmLevelCritical:
		return true
	default:
		return false
	}
}

const (
	AlarmLevelWaring   AlarmLevel = "1" // 单点报错，不影响整体流程
	AlarmLevelError    AlarmLevel = "2" // 对主要流程有影响，如果不优化处理可能导致风险
	AlarmLevelCritical AlarmLevel = "3" // 采集配置/重要模块不可用;对Agent稳定性造成影响;导致资损（数据丢失等）
)
