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
	"sync"
	"testing"

	"github.com/stretchr/testify/assert"
)

func Test_Max(t *testing.T) {
	assert.Equal(t, int(20), Max(int(10), int(20)))
	assert.Equal(t, int8(20), Max(int8(10), int8(20)))
	assert.Equal(t, int16(20), Max(int16(10), int16(20)))
	assert.Equal(t, int32(20), Max(int32(10), int32(20)))
	assert.Equal(t, int64(20), Max(int64(10), int64(20)))

	assert.Equal(t, uint(20), Max(uint(10), uint(20)))
	assert.Equal(t, uint8(20), Max(uint8(10), uint8(20)))
	assert.Equal(t, uint16(20), Max(uint16(10), uint16(20)))
	assert.Equal(t, uint32(20), Max(uint32(10), uint32(20)))
	assert.Equal(t, uint64(20), Max(uint64(10), uint64(20)))

	assert.Equal(t, float32(20.1), Max(float32(10.1), float32(20.1)))
	assert.Equal(t, float64(20.1), Max(float64(10.1), float64(20.1)))
}

func TestAtomicAddFloat64(t *testing.T) {
	var num float64
	var wg sync.WaitGroup
	wg.Add(100)

	for i := 0; i < 100; i++ {
		go func() {
			AtomicAddFloat64(&num, 1.0)
			wg.Done()
		}()
	}

	wg.Wait()

	if num != 100.0 {
		t.Errorf("Expected num to be 100.0, got %f", num)
	}
}

func TestAtomicLoadFloat64(t *testing.T) {
	var num = 42.0
	result := AtomicLoadFloat64(&num)

	if result != 42.0 {
		t.Errorf("Expected result to be 42.0, got %f", result)
	}
}

func TestAtomicStoreFloat64(t *testing.T) {
	var num float64
	AtomicStoreFloat64(&num, 10.5)

	if num != 10.5 {
		t.Errorf("Expected num to be 10.5, got %f", num)
	}
}

func TestAtomicFloatFunctions(t *testing.T) {
	var num float64
	AtomicStoreFloat64(&num, 5.5)
	if num != 5.5 {
		t.Errorf("Expected num to be 5.5, got %f", num)
	}

	AtomicAddFloat64(&num, 2.5)
	if num != 8.0 {
		t.Errorf("Expected num to be 8.0, got %f", num)
	}

	result := AtomicLoadFloat64(&num)
	if result != 8.0 {
		t.Errorf("Expected result to be 8.0, got %f", result)
	}

	AtomicStoreFloat64(&num, math.NaN())
	result = AtomicLoadFloat64(&num)
	if !math.IsNaN(result) {
		t.Errorf("Expected result to be NaN, got %f", result)
	}
}
