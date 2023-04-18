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
	"github.com/stretchr/testify/assert"

	"testing"
	"unsafe"
)

func TestZeroCopyString(t *testing.T) {
	slice := []byte("jkdfjksdfj")
	newStr := ZeroCopyBytesToString(slice)
	bytes := *(*[]byte)(unsafe.Pointer(&newStr))
	assert.Equal(t, &bytes[0], &slice[0])
}

func TestZeroCopySlice(t *testing.T) {
	str := "dfdsfdsf"
	bytes := *(*[]byte)(unsafe.Pointer(&str))
	slice := ZeroCopyStringToBytes(str)
	assert.Equal(t, &bytes[0], &slice[0])
}

func TestZeroCopyStringNil(t *testing.T) {
	newStr := ZeroCopyBytesToString(nil)
	newStr2 := ZeroCopyBytesToString([]byte(""))
	assert.Equal(t, newStr, newStr2)
}

func TestZeroCopySliceEmpty(t *testing.T) {
	str := ""
	bytes := *(*[]byte)(unsafe.Pointer(&str))
	slice := ZeroCopyStringToBytes(str)
	assert.Equal(t, &bytes, &slice)
}

// goos: darwin
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/pkg/helper
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkZeroCopyString
// BenchmarkZeroCopyString/no-zero-copy
// BenchmarkZeroCopyString/no-zero-copy-12                 59734488                20.07 ns/op           16 B/op          1 allocs/op
// BenchmarkZeroCopyString/zero-copy
// BenchmarkZeroCopyString/zero-copy-12                    474650650                2.515 ns/op           0 B/op          0 allocs/op
func BenchmarkZeroCopyString(b *testing.B) {
	slice := []byte("jkdfjksdfj")
	tests := []struct {
		name string
		fun  func() string
	}{
		{
			"no-zero-copy",
			func() string {
				return string(slice)
			},
		},
		{
			"zero-copy",
			func() string {
				return ZeroCopyBytesToString(slice)
			},
		},
	}
	for _, tt := range tests {
		b.Run(tt.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				tt.fun()
			}
		})
	}
}

// goos: darwin
// goarch: amd64
// pkg: github.com/alibaba/ilogtail/pkg/helper
// cpu: Intel(R) Core(TM) i7-9750H CPU @ 2.60GHz
// BenchmarkZeroCopySlice
// BenchmarkZeroCopySlice/no-zero-copy
// BenchmarkZeroCopySlice/no-zero-copy-12                  48458805                23.11 ns/op           16 B/op          1 allocs/op
// BenchmarkZeroCopySlice/zero-copy
// BenchmarkZeroCopySlice/zero-copy-12                     430296753                2.712 ns/op           0 B/op          0 allocs/op
func BenchmarkZeroCopySlice(b *testing.B) {
	str := "dfadsfadsf"
	tests := []struct {
		name string
		fun  func() []byte
	}{
		{
			"no-zero-copy",
			func() []byte {
				return []byte(str)
			},
		},
		{
			"zero-copy",
			func() []byte {
				return ZeroCopyStringToBytes(str)
			},
		},
	}
	for _, tt := range tests {
		b.Run(tt.name, func(b *testing.B) {
			for i := 0; i < b.N; i++ {
				tt.fun()
			}
		})
	}
}

func TestIsSafeString(t *testing.T) {
	str1 := "123456"
	str2 := "123"
	unsafeStr := ZeroCopyBytesToString(ZeroCopyStringToBytes(str1)[:4])
	assert.True(t, IsSafeString(str2, str1))
	assert.False(t, IsSafeString(str1, unsafeStr))
	assert.False(t, IsSafeString(unsafeStr, str1))
}
