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

package setting

import (
	"log"
	"reflect"

	"config-server/common"
)

type Setting struct {
	IP                  string `json:"ip"`                    // default: "127.0.0.1"
	StoreMode           string `json:"store_mode"`            // support "leveldb", "gorm"
	Port                string `json:"port"`                  // default: "8899"
	DbPath              string `json:"db_path"`               // default: "./DB"
	Driver              string `json:"driver"`                // support "mysql", "postgre", "sqlite", "sqlserver"
	Dsn                 string `json:"dsn"`                   // gorm dsn
	AutoMigrateSchema   bool   `json:"auto_migrate_schema"`   // auto migrate schema
	AgentUpdateInterval int    `json:"agent_update_interval"` // default: 1s
	ConfigSyncInterval  int    `json:"config_sync_interval"`  // default: 3s
}

var mySetting *Setting

var settingFile = "./setting/setting.json"

/*
Create a singleton of setting
*/
func GetSetting() Setting {
	return *mySetting
}

/*
Use map to update setting.
For example, if the value of map is {store_mode:"mysql"},
this function will change "mySetting's StoreMode" to "mysql".
*/
func UpdateSetting(tagMap map[string]interface{}) {
	v := reflect.ValueOf(mySetting).Elem()
	t := v.Type()

	fieldNum := v.NumField()
	for i := 0; i < fieldNum; i++ {
		fieldInfo := t.Field(i)
		tag := fieldInfo.Tag.Get("json")
		if tag == "" {
			continue
		}
		if value, ok := tagMap[tag]; ok {
			if reflect.ValueOf(value).Type() == v.FieldByName(fieldInfo.Name).Type() {
				v.FieldByName(fieldInfo.Name).Set(reflect.ValueOf(value))
			}
		}
	}
	err := common.WriteJSON(settingFile, mySetting)
	if err != nil {
		log.Println(err.Error())
	}
}

func init() {
	mySetting = new(Setting)
	err := common.ReadJSON(settingFile, mySetting)
	if err != nil {
		panic(err)
	}
	if mySetting.IP == "" {
		mySetting.IP = "127.0.0.1"
	}
	if mySetting.Port == "" {
		mySetting.Port = "8899"
	}
	if mySetting.StoreMode == "" {
		panic("Please set store mode")
	}
	if mySetting.StoreMode == "gorm" {
		if mySetting.Driver == "" {
			panic("Please set driver")
		}
		if mySetting.Dsn == "" {
			panic("Please set dsn")
		}
	}
	if mySetting.AgentUpdateInterval == 0 {
		mySetting.AgentUpdateInterval = 1
	}
	if mySetting.ConfigSyncInterval == 0 {
		mySetting.ConfigSyncInterval = 3
	}
}
