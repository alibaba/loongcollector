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

package test

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"config-server/setting"
)

func TestSetting(t *testing.T) {
	Convey("Test load setting.", t, func() {
		So(setting.GetSetting().StoreMode, ShouldEqual, "leveldb")
	})

	Convey("Test update setting.", t, func() {
		So(setting.GetSetting().StoreMode, ShouldEqual, "leveldb")
		setting.UpdateSetting(map[string]interface{}{"store_mode": "mysql"})
		So(setting.GetSetting().StoreMode, ShouldEqual, "mysql")
		setting.UpdateSetting(map[string]interface{}{"store_mode": "leveldb"})
		So(setting.GetSetting().StoreMode, ShouldEqual, "leveldb")
	})
}
