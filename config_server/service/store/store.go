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

package store

import (
	"config-server/setting"
	"config-server/store/gorm"
	database "config-server/store/interface_database"
	"config-server/store/leveldb"
)

// Data in database

/*
Store Factory
*/
func newStore(storeType string) database.Database {
	switch storeType {
	case "gorm":
		return new(gorm.Store)
	case "leveldb":
		return new(leveldb.Store)
	default:
		panic("Wrong store type.")
	}
}

var myStore database.Database

/*
Create a singleton of store
*/
func GetStore() database.Database {
	return myStore
}

// batch

func CreateBacth() *database.Batch {
	return new(database.Batch)
}

// init

func init() {
	myStore = newStore(setting.GetSetting().StoreMode)
	err := myStore.Connect()
	if err != nil {
		panic(err)
	}
}
