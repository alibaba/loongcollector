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

package config

import "encoding/json"

// export http endpoints.
const (
	EnpointLoadconfig = "/loadconfig"
	EnpointStop       = "/stop"
	EnpointHoldon     = "/holdon"
)

// LoadedConfig used to start logtail plugin sniffer.
type LoadedConfig struct {
	Project     string `json:"project"`
	Logstore    string `json:"logstore"`
	ConfigName  string `json:"config_name"`
	LogstoreKey int64  `json:"logstore_key"`
	JSONStr     string `json:"json_str"`
}

func SerizlizeLoadedConfig(c []*LoadedConfig) []byte {
	bytes, _ := json.Marshal(c)
	return bytes
}

func DeserializeLoadedConfig(cfg []byte) ([]*LoadedConfig, error) {
	var c []*LoadedConfig
	if err := json.Unmarshal(cfg, &c); err != nil {
		return nil, err
	}
	return c, nil
}

func GetRealConfigName(configName string) string {
	realConfigName := configName
	if len(configName) > 2 && (configName[len(configName)-2:] == "/1" || configName[len(configName)-2:] == "/2") {
		realConfigName = configName[:len(configName)-2]
	}
	return realConfigName
}
