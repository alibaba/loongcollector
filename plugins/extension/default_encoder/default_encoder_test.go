// Copyright 2024 iLogtail Authors
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

package defaultencoder

import (
	"encoding/json"
	"fmt"
	"testing"

	. "github.com/smartystreets/goconvey/convey"

	"github.com/alibaba/ilogtail/pkg/protocol/encoder/prometheus"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

// 场景：插件初始化
// 因子：Format 字段存在
// 因子：Prometheus Protocol
// 预期：初始化成功，且 Encoder 为 Prometheus Encoder
func TestEncoder_ShouldPassConfigToRealEncoder_GivenCorrectConfigInput(t *testing.T) {
	Convey("Given correct config json string", t, func() {
		e := NewExtensionDefaultEncoder()
		So(e, ShouldNotBeNil)
		So(e.Encoder, ShouldBeNil)
		So(e.Format, ShouldBeEmpty)
		So(e.options, ShouldBeNil)

		encodeProtocol := "prometheus"
		optField, optValue := "SeriesLimit", 1024
		configJSONStr := fmt.Sprintf(`{"Format":"%s","%s":%d}`, encodeProtocol, optField, optValue)
		// must using float64(optValue), not optValue
		// https://github.com/smartystreets/goconvey/issues/437
		wantOpts := map[string]any{optField: float64(optValue)}

		Convey("Then should json unmarshal success", func() {
			err := json.Unmarshal([]byte(configJSONStr), e)
			So(err, ShouldBeNil)
			So(e.Encoder, ShouldBeNil)
			So(e.options, ShouldResemble, wantOpts)
			So(e.Format, ShouldEqual, encodeProtocol)

			Convey("Then should init success", func() {
				err = e.Init(mock.NewEmptyContext("p", "l", "c"))
				So(err, ShouldBeNil)
				So(e.Encoder, ShouldNotBeNil)

				Convey("Then encoder implement should be *prometheus.Encoder", func() {
					promEncoder, ok := e.Encoder.(*prometheus.Encoder)
					So(ok, ShouldBeTrue)
					So(promEncoder, ShouldNotBeNil)
					So(promEncoder.SeriesLimit, ShouldEqual, optValue)
				})
			})

			Convey("Then should stop success", func() {
				err := e.Stop()
				So(err, ShouldBeNil)
			})
		})
	})
}

// 场景：插件初始化
// 因子：Format 字段存在
// 因子：Unsupported Protocol
// 预期：初始化失败
func TestEncoder_ShouldNotPassConfigToRealEncoder_GivenIncorrectConfigInput(t *testing.T) {
	Convey("Given incorrect config json string but with Format field", t, func() {
		e := NewExtensionDefaultEncoder()
		So(e, ShouldNotBeNil)
		So(e.Encoder, ShouldBeNil)
		So(e.Format, ShouldBeEmpty)
		So(e.options, ShouldBeNil)

		encodeProtocol := "unknown"
		configJSONStr := fmt.Sprintf(`{"Format":"%s"}`, encodeProtocol)

		Convey("Then should json unmarshal success", func() {
			err := json.Unmarshal([]byte(configJSONStr), e)
			So(err, ShouldBeNil)
			So(e.Encoder, ShouldBeNil)
			So(e.Format, ShouldEqual, encodeProtocol)

			Convey("Then should init failed", func() {
				err = e.Init(mock.NewEmptyContext("p", "l", "c"))
				So(err, ShouldNotBeNil)
				So(e.Encoder, ShouldBeNil)
			})

			Convey("Then should stop success", func() {
				err := e.Stop()
				So(err, ShouldBeNil)
			})
		})
	})
}

// 场景：插件初始化
// 因子：Format 字段缺失
// 预期：json unmarshal 失败，初始化失败
func TestEncoder_ShouldUnmarshalFailed_GivenConfigWithoutFormat(t *testing.T) {
	Convey("Given incorrect config json string and without Format field", t, func() {
		e := NewExtensionDefaultEncoder()
		So(e, ShouldNotBeNil)
		So(e.Encoder, ShouldBeNil)
		So(e.Format, ShouldBeEmpty)
		So(e.options, ShouldBeNil)

		configJSONStr := `{"Unknown":"unknown"}`

		Convey("Then should json unmarshal failed", func() {
			err := json.Unmarshal([]byte(configJSONStr), e)
			So(err, ShouldNotBeNil)
			So(e.Encoder, ShouldBeNil)
			So(e.Format, ShouldBeEmpty)

			Convey("Then should init failed", func() {
				err = e.Init(mock.NewEmptyContext("p", "l", "c"))
				So(err, ShouldNotBeNil)
				So(e.Encoder, ShouldBeNil)
			})

			Convey("Then should stop success", func() {
				err := e.Stop()
				So(err, ShouldBeNil)
			})
		})
	})
}
