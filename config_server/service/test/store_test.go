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

	"github.com/alibaba/ilogtail/config_server/service/common"
	"github.com/alibaba/ilogtail/config_server/service/model"
	"github.com/alibaba/ilogtail/config_server/service/store"
)

func TestStore(t *testing.T) {
	Convey("Test load store, check it's type.", t, func() {
		So(store.GetStore().GetMode(), ShouldEqual, "leveldb")
	})

	Convey("Test store's CURD, take config as example.", t, func() {
		s := store.GetStore()

		Convey("First, add a config named config-test to store.", func() {
			config := new(model.Config)
			config.Name = "config-test"
			config.AgentType = "iLogtail"
			config.Content = "test"
			config.Version = 1
			config.Description = ""
			config.DelTag = false
			s.Add(common.TypeCollectionConfig, config.Name, config)

			value, getErr := s.Get(common.TypeCollectionConfig, "config-test")
			So(getErr, ShouldBeNil)
			So(value.(*model.Config), ShouldResemble, &model.Config{Name: "config-test", AgentType: "iLogtail", Content: "test", Version: 1, Description: "", DelTag: false})
		})

		Convey("Second, update config-test's content.", func() {
			value, getErr := s.Get(common.TypeCollectionConfig, "config-test")
			So(getErr, ShouldBeNil)
			config := value.(*model.Config)
			config.Description = "test"
			s.Update(common.TypeCollectionConfig, config.Name, config)

			value, getErr = s.Get(common.TypeCollectionConfig, "config-test")
			So(getErr, ShouldBeNil)
			So(value.(*model.Config), ShouldResemble, &model.Config{Name: "config-test", AgentType: "iLogtail", Content: "test", Version: 1, Description: "test", DelTag: false})
		})

		Convey("Third, delete config-test.", func() {
			s.Delete(common.TypeCollectionConfig, "config-test")
			_, getErr := s.Get(common.TypeCollectionConfig, "config-test")
			So(getErr, ShouldNotBeNil)
		})
	})
}
