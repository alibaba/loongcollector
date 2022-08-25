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

package eventrecorder

import "fmt"

const ProductCode = "SLS"

const (
	LogController string = "LogController"
	Logtail       string = "Logtail"
)

type EventDefine struct {
	currentModule string
}

type Action string

const (
	UpdateConfig Action = "UpdateConfig"

	UpdateConfigStatus Action = "UpdateConfigStatus"

	DeleteConfig Action = "DeleteConfig"

	CreateProject Action = "CreateProject"

	CreateLogstore Action = "CreateLogstore"

	CreateTag Action = "CreateTag"

	CreateIndex Action = "CreateIndex"

	CreateProductLogStore Action = "CreateProductLogstore"

	CreateProductApp Action = "CreateProductApp"
)

type Alarm string

const (
	CommonAlarm Alarm = "Fail"
)

func NewEventDefine(moduleCode string) *EventDefine {
	eventDefine := &EventDefine{}
	eventDefine.currentModule = moduleCode
	return eventDefine
}

func (e *EventDefine) getErrorAction(action Action, alarm Alarm) string {
	return fmt.Sprintf("%s.%s.%s.%s", ProductCode, e.currentModule, action, alarm)
}

func (e *EventDefine) getInfoAction(action Action) string {
	return fmt.Sprintf("%s.%s.%s.%s", ProductCode, e.currentModule, action, "Success")
}
