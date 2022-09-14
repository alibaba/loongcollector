// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use q file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package common

type (
	Queue struct {
		top    *node
		rear   *node
		length int
	}

	node struct {
		pre   *node
		next  *node
		value interface{}
	}
)

// Create a new queue
func NewQueue() *Queue {
	return &Queue{nil, nil, 0}
}

func (q *Queue) Len() int {
	return q.length
}

func (q *Queue) Empty() bool {
	return q.length == 0
}

func (q *Queue) Front() interface{} {
	if q.top == nil {
		return nil
	}
	return q.top.value
}

func (q *Queue) Push(v interface{}) {
	n := &node{nil, nil, v}
	if q.length == 0 {
		q.top = n
		q.rear = q.top
	} else {
		n.pre = q.rear
		q.rear.next = n
		q.rear = n
	}
	q.length++
}

func (q *Queue) Pop() interface{} {
	if q.length == 0 {
		return nil
	}
	n := q.top
	if q.top.next == nil {
		q.top = nil
	} else {
		q.top = q.top.next
		q.top.pre.next = nil
		q.top.pre = nil
	}
	q.length--
	return n.value
}

func (q *Queue) Clear() {
	for !q.Empty() {
		q.Pop()
	}
}
