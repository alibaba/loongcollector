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

package pipeline

import "sync"

// AsyncControl is an asynchronous execution control that can be canceled.
type AsyncControl struct {
	cancelToken chan struct{}
	wg          sync.WaitGroup
}

// CancelToken returns a readonly channel that can be subscribed to as a cancel token
func (p *AsyncControl) CancelToken() <-chan struct{} {
	return p.cancelToken
}

func (p *AsyncControl) Notify() {
	p.cancelToken <- struct{}{}
}

// Reset cancel channal
func (p *AsyncControl) Reset() {
	if p.cancelToken == nil {
		p.cancelToken = make(chan struct{}, 1)
	}
}

// Run function as a Task
func (p *AsyncControl) Run(task func(*AsyncControl)) {
	p.wg.Add(1)
	go func(cc *AsyncControl, fn func(*AsyncControl)) {
		defer cc.wg.Done()
		fn(cc)
	}(p, task)
}

// Waiting for executing task to be canceled
func (p *AsyncControl) WaitCancel() {
	close(p.cancelToken)
	p.wg.Wait()
	p.cancelToken = nil
}

func NewAsyncControl() *AsyncControl {
	return &AsyncControl{
		cancelToken: make(chan struct{}, 1),
	}
}
