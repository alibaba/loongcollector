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

package util

import (
	"reflect"
	"unsafe"
)

//nolint:gosec
func ZeroCopyBytesToString(b []byte) (s string) {
	if b == nil {
		return
	}
	pbytes := (*reflect.SliceHeader)(unsafe.Pointer(&b))
	pstring := (*reflect.StringHeader)(unsafe.Pointer(&s))
	pstring.Data = pbytes.Data
	pstring.Len = pbytes.Len
	return
}

//nolint:gosec
func ZeroCopyStringToBytes(s string) (b []byte) {
	if s == "" {
		return
	}
	pbytes := (*reflect.SliceHeader)(unsafe.Pointer(&b))
	pstring := (*reflect.StringHeader)(unsafe.Pointer(&s))
	pbytes.Data = pstring.Data
	pbytes.Len = pstring.Len
	pbytes.Cap = pstring.Len
	return
}

func IsSafeString(str1, str2 string) bool {
	slice1 := ZeroCopyStringToBytes(str1)
	slice2 := ZeroCopyStringToBytes(str2)
	isIn := func(bytes1, bytes2 []byte) bool {
		for i := range bytes2 {
			if &bytes2[i] == &bytes1[0] || &bytes2[i] == &bytes1[len(bytes1)-1] {
				return true
			}
		}
		return false
	}
	return !isIn(slice1, slice2) && !isIn(slice2, slice1)
}
