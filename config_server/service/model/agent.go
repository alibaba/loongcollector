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
	"config-server/common"
	proto "config-server/proto"
	"database/sql/driver"
	"encoding/json"
	"fmt"

	"gorm.io/gorm"
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

func (a *AgentAttributes) Scan(value interface{}) error {
	if value == nil {
		return nil
	}

	b, ok := value.([]byte)
	if !ok {
		return fmt.Errorf("value is not []byte, value: %v", value)
	}

	return json.Unmarshal(b, a)
}

func (a AgentAttributes) Value() (driver.Value, error) {
	v, err := json.Marshal(a)
	if err != nil {
		return nil, err
	}
	return v, nil
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
	AgentID        string          `json:"AgentID" gorm:"primaryKey;column:agent_id"`
	AgentType      string          `json:"AgentType"`
	Attributes     AgentAttributes `json:"Attributes"`
	Tags           []string        `json:"Tags" gorm:"-"`
	SerializedTags string          `json:"-" gorm:"column:tags;type:json"`
	RunningStatus  string          `json:"RunningStatus"`
	StartupTime    int64           `json:"StartupTime"`
	Interval       int32           `json:"Interval"`
}

func (Agent) TableName() string {
	return common.TypeAgent
}

func (a *Agent) AfterFind(tx *gorm.DB) (err error) {
	if a.SerializedTags == "" {
		return nil
	}
	err = json.Unmarshal([]byte(a.SerializedTags), &a.Tags)
	if err != nil {
		return err
	}

	return nil
}

func (a *Agent) BeforeSave(tx *gorm.DB) (err error) {
	if a.Tags == nil {
		return nil
	}
	data, err := json.Marshal(a.Tags)
	if err != nil {
		return err
	}
	tx.Statement.SetColumn("tags", string(data))

	return nil
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
