// Copyright 2021 iLogtail Authors
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

package helper

import (
	"math"
	"runtime"
	"sync/atomic"
	"unsafe"

	"github.com/alibaba/ilogtail/pkg/constraints"
)

func Max[T constraints.IntUintFloat](x T, y T) T {
	if x > y {
		return x
	}
	return y
}

func Min[T constraints.IntUintFloat](x T, y T) T {
	if x < y {
		return x
	}
	return y
}

func AtomicAddFloat64(dst *float64, n float64) {
	for {
		i := 0
		/* #nosec G103 */
		bits := atomic.LoadUint64((*uint64)(unsafe.Pointer(dst)))
		now := math.Float64bits(math.Float64frombits(bits) + n)
		/* #nosec G103 */
		if atomic.CompareAndSwapUint64((*uint64)(unsafe.Pointer(dst)), bits, now) {
			return
		}
		i++
		if i > 128 {
			runtime.Gosched()
			i = 0
		}
	}
}

func AtomicLoadFloat64(dst *float64) float64 {
	/* #nosec G103 */
	return math.Float64frombits(atomic.LoadUint64((*uint64)(unsafe.Pointer(dst))))
}

func AtomicStoreFloat64(dst *float64, n float64) {
	/* #nosec G103 */
	atomic.StoreUint64((*uint64)(unsafe.Pointer(dst)), math.Float64bits(n))
}
