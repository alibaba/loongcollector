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

package protocol

import (
	"encoding/json"
	"fmt"
	"testing"

	. "github.com/smartystreets/goconvey/convey"
)

func TestJsonMarshalAndMarshalWithoutHTMLEscaped(t *testing.T) {
	type TestData struct {
		ID   int
		Msg  string
		Data interface{}
	}

	Convey("test json marshal and marchal without html escaped", t, func() {
		data := TestData{
			ID:   0,
			Msg:  ">>>><<<)(*&^%$#@!$@#hello+1447138058167839254",
			Data: nil,
		}

		Convey("test marshalWithoutHTMLEscaped", func() {
			v, _ := marshalWithoutHTMLEscaped(data)

			str := string(v)
			fmt.Println(str)

			So(str, ShouldEqual, `{"ID":0,"Msg":">>>><<<)(*&^%$#@!$@#hello+1447138058167839254","Data":null}`)
		})

		Convey("test json marshal", func() {
			v, _ := json.Marshal(data)

			str := string(v)
			fmt.Println(str)

			So(str, ShouldNotEqual, `{"ID":0,"Msg":">>>><<<)(*&^%$#@!$@#hello+1447138058167839254","Data":null}`)
		})
	})
}
