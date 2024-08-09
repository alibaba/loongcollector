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
)

var ConfigType = map[string]proto.ConfigType{
	"PIPELINE_CONFIG": proto.ConfigType_PIPELINE_CONFIG,
	"AGENT_CONFIG":    proto.ConfigType_AGENT_CONFIG,
}

type ConfigDetail struct {
	Type    string `json:"Type"`
	Name    string `json:"Name" gorm:"primaryKey"`
	Version int64  `json:"Version"`
	Context string `json:"Context"`
	Detail  string `json:"Detail"`
	DelTag  bool   `json:"DelTag"`
}

func (ConfigDetail) TableName() string {
	return common.TypeConfigDetail
}

func (c *ConfigDetail) ToProto() *proto.ConfigDetail {
	pc := new(proto.ConfigDetail)
	pc.Type = ConfigType[c.Type]
	pc.Name = c.Name
	pc.Version = c.Version
	pc.Context = c.Context
	pc.Detail = c.Detail
	return pc
}

func (c *ConfigDetail) ParseProto(pc *proto.ConfigDetail) {
	c.Type = pc.Type.String()
	c.Name = pc.Name
	c.Version = pc.Version
	c.Context = pc.Context
	c.Detail = pc.Detail
}

type Command struct {
	Type string            `json:"Type"`
	Name string            `json:"Name"`
	ID   string            `json:"ID"`
	Args map[string]string `json:"Args"`
}

func (c *Command) ToProto() *proto.Command {
	pc := new(proto.Command)
	pc.Type = c.Type
	pc.Name = c.Name
	pc.Id = c.ID
	pc.Args = c.Args
	return pc
}

func (c *Command) ParseProto(pc *proto.Command) {
	c.Type = pc.Type
	c.Name = pc.Name
	c.ID = pc.Id
	c.Args = pc.Args
}
