// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package elasticsearch

import (
	"testing"

	. "github.com/smartystreets/goconvey/convey"
)

func TestGetIndexKeys(t *testing.T) {
	Convey("Given an empty index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should not be extracted correctly", func() {
				So(err, ShouldNotBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(keys, ShouldBeNil)
			})
		})
	})
	Convey("Given a normal index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "normal_index",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a variable index", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_${var}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeFalse)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a field dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{content.field}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 1)
				So(keys[0], ShouldEqual, "content.field")
			})
		})
	})
	Convey("Given a timestamp dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{+yyyyMM}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 0)
			})
		})
	})
	Convey("Given a composite dynamic index expression", t, func() {
		flusher := &FlusherElasticSearch{
			Index: "index_%{content.field}_%{tag.host.ip}_%{+yyyyMMdd}",
		}
		Convey("When getIndexKeys is called", func() {
			keys, isDynamicIndex, err := flusher.getIndexKeys()
			Convey("Then the keys should be extracted correctly", func() {
				So(err, ShouldBeNil)
				So(isDynamicIndex, ShouldBeTrue)
				So(len(keys), ShouldEqual, 2)
				So(keys[0], ShouldEqual, "content.field")
				So(keys[1], ShouldEqual, "tag.host.ip")
			})
		})
	})
}
