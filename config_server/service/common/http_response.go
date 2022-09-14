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

package common

type httpStatus struct {
	Status int
	Code   string
}

var (
	Accept                 = httpStatus{200, "Accept"}
	BadRequest             = httpStatus{400, "BadRequest"}
	ConfigAlreadyExist     = httpStatus{400, "ConfigAlreadyExist"}
	ConfigNotExist         = httpStatus{404, "ConfigNotExist"}
	InternalServerError    = httpStatus{500, "InternalServerError"}
	InvalidParameter       = httpStatus{400, "InvalidParameter"}
	AgentAlreadyExist      = httpStatus{400, "AgentAlreadyExist"}
	AgentGroupAlreadyExist = httpStatus{400, "AgentGroupAlreadyExist"}
	AgentGroupNotExist     = httpStatus{404, "AgentGroupNotExist"}
	AgentNotExist          = httpStatus{404, "AgentNotExist"}
	RequestTimeout         = httpStatus{500, "RequestTimeout"}
	ServerBusy             = httpStatus{503, "ServerBusy"}
)
