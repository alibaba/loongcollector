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

	"config-server/common"
	"config-server/model"
	"config-server/store"
)

func TestStore(t *testing.T) {
	Convey("Test load store, check it's type.", t, func() {
		So(store.GetStore().GetMode(), ShouldEqual, "leveldb")
	})

	Convey("Test store's CURD, take config as example.", t, func() {
		s := store.GetStore()

		Convey("First, add a config named config-test to store.", func() {
			config := new(model.ConfigDetail)
			config.Name = "config-test"
			config.Type = "PIPELINE_CONFIG"
			config.Detail = "test"
			config.Version = 1
			config.Context = ""
			config.DelTag = false
			s.Add(common.TypeConfigDetail, config.Name, config)

			value, getErr := s.Get(common.TypeConfigDetail, "config-test")
			So(getErr, ShouldBeNil)
			So(value.(*model.ConfigDetail), ShouldResemble, &model.ConfigDetail{Name: "config-test", Type: "PIPELINE_CONFIG", Detail: "test", Version: 1, Context: "", DelTag: false})
		})

		Convey("Second, update config-test's content.", func() {
			value, getErr := s.Get(common.TypeConfigDetail, "config-test")
			So(getErr, ShouldBeNil)
			config := value.(*model.ConfigDetail)
			config.Context = "test"
			s.Update(common.TypeConfigDetail, config.Name, config)

			value, getErr = s.Get(common.TypeConfigDetail, "config-test")
			So(getErr, ShouldBeNil)
			So(value.(*model.ConfigDetail), ShouldResemble, &model.ConfigDetail{Name: "config-test", Type: "PIPELINE_CONFIG", Detail: "test", Version: 1, Context: "test", DelTag: false})
		})

		Convey("Third, delete config-test.", func() {
			s.Delete(common.TypeConfigDetail, "config-test")
			_, getErr := s.Get(common.TypeConfigDetail, "config-test")
			So(getErr, ShouldNotBeNil)
		})
	})
}
