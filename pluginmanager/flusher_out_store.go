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

package pluginmanager

import (
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type FlushData interface {
	protocol.LogGroup | models.PipelineGroupEvents
}

type FlushOutStore[T FlushData] struct {
	data []*T
}

func (s *FlushOutStore[T]) Add(data ...*T) {
	s.data = append(s.data, data...)
}

func (s *FlushOutStore[T]) Len() int {
	return len(s.data)
}

func (s *FlushOutStore[T]) Get() []*T {
	return s.data
}

func (s *FlushOutStore[T]) Write(ch chan *T) {
	for i := 0; i < s.Len(); i++ {
		ch <- s.data[i]
		s.data[i] = nil
	}
	s.data = s.data[:0]
}

func (s *FlushOutStore[T]) Reset() {
	s.data = make([]*T, 0)
}

func (s *FlushOutStore[T]) Merge(in *FlushOutStore[T]) {
	if s == in {
		return
	}
	if s.Len() == 0 {
		s.data = in.data
	} else {
		s.data = append(s.data, in.data...)
	}
}

func NewFlushOutStore[T FlushData]() *FlushOutStore[T] {
	return &FlushOutStore[T]{
		data: make([]*T, 0),
	}
}
