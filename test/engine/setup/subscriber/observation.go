// Copyright 2026 iLogtail Authors
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

package subscriber

// Batch is a set of records observed by an E2E subscriber. Different backends
// expose different wire formats, so assertions use this local view.
type Batch struct {
	Records []*Record
	Tags    map[string]string
}

// Record keeps observed field order and duplicates for assertions.
type Record struct {
	Fields    []Field
	Timestamp uint64 // Unix nanoseconds, when supplied by the backend.
}

type Field struct {
	Key   string
	Value string
}
