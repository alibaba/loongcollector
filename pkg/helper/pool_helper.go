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
	"sync"
)

// GenericPool is a pool for *[]T
type GenericPool[T any] struct {
	sync.Pool
}

// NewGenericPool returns a GenericPool.
// It accepts a function that returns a new []T. e.g., func() []byte {return make([]byte, 0, 128)}
func NewGenericPool[T any](fn func() []T) GenericPool[T] {
	return GenericPool[T]{
		sync.Pool{
			New: func() interface{} {
				t := fn()
				return &t
			},
		},
	}
}

func (p *GenericPool[T]) Get() *[]T {
	res := p.Pool.Get().(*[]T)
	*res = (*res)[:0]
	return res
}

func (p *GenericPool[T]) Put(t *[]T) {
	if t != nil {
		p.Pool.Put(t)
	}
}
