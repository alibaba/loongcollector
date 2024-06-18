// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package helper

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestGenericPool(t *testing.T) {
	bufferPool := NewGenericPool(func() []byte {
		return make([]byte, 0, 128)
	})
	b := bufferPool.Get()
	assert.Empty(t, b)
	*b = append(*b, 'a')
	assert.Equal(t, []byte{'a'}, *b)
	bufferPool.Put(b)
	b = bufferPool.Get()
	assert.Empty(t, b)
	bufferPool.Put(b)

	indexPool := NewGenericPool(func() []string {
		return make([]string, 0, 10)
	})
	i := indexPool.Get()
	assert.Empty(t, i)
	*i = append(*i, "a")
	assert.Equal(t, []string{"a"}, *i)
	indexPool.Put(i)
	i = indexPool.Get()
	assert.Empty(t, i)
	indexPool.Put(i)
}
