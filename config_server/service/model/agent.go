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

package model

import (
	proto "config-server/proto"
)

type AgentAttributes struct {
	Version  string            `json:"Version"`
	Category string            `json:"Category"`
	IP       string            `json:"IP"`
	Hostname string            `json:"Hostname"`
	Region   string            `json:"Region"`
	Zone     string            `json:"Zone"`
	Extras   map[string]string `json:"Extras"`
}

func (a *AgentAttributes) ToProto() *proto.AgentAttributes {
	pa := new(proto.AgentAttributes)
	pa.Version = a.Version
	pa.Category = a.Category
	pa.Ip = a.IP
	pa.Hostname = a.Hostname
	pa.Region = a.Region
	pa.Zone = a.Zone
	pa.Extras = a.Extras
	return pa
}

func (a *AgentAttributes) ParseProto(pa *proto.AgentAttributes) {
	a.Version = pa.Version
	a.Category = pa.Category
	a.IP = pa.Ip
	a.Hostname = pa.Hostname
	a.Region = pa.Region
	a.Zone = pa.Zone
	a.Extras = pa.Extras
}

type Agent struct {
	AgentID       string          `json:"AgentID"`
	AgentType     string          `json:"AgentType"`
	Attributes    AgentAttributes `json:"Attributes"`
	Tags          []string        `json:"Tags"`
	RunningStatus string          `json:"RunningStatus"`
	StartupTime   int64           `json:"StartupTime"`
	Interval      int32           `json:"Interval"`
}

func (a *Agent) ToProto() *proto.Agent {
	pa := new(proto.Agent)
	pa.AgentId = a.AgentID
	pa.AgentType = a.AgentType
	pa.Attributes = a.Attributes.ToProto()
	pa.Tags = a.Tags
	pa.RunningStatus = a.RunningStatus
	pa.StartupTime = a.StartupTime
	pa.Interval = a.Interval
	return pa
}

func (a *Agent) ParseProto(pa *proto.Agent) {
	a.AgentID = pa.AgentId
	a.AgentType = pa.AgentType
	a.Attributes.ParseProto(pa.Attributes)
	a.Tags = pa.Tags
	a.RunningStatus = pa.RunningStatus
	a.StartupTime = pa.StartupTime
	a.Interval = pa.Interval
}
