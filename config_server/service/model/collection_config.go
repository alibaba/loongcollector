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

import proto "github.com/alibaba/ilogtail/config_server/service/proto"

type Config struct {
	Name        string `json:"Name"`
	AgentType   string `json:"AgentType"`
	Content     string `json:"Content"`
	Version     int64  `json:"Version"`
	Description string `json:"Description"`
	DelTag      bool   `json:"DelTag"`
}

func (c *Config) ToProto() *proto.Config {
	pc := new(proto.Config)
	pc.ConfigName = c.Name
	pc.AgentType = c.AgentType
	pc.Description = c.Description
	pc.Content = c.Content
	return pc
}

func (c *Config) ParseProto(pc *proto.Config) {
	c.Name = pc.ConfigName
	c.AgentType = pc.AgentType
	c.Description = pc.Description
	c.Content = pc.Content
}
