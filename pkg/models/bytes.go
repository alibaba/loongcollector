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

package models

// Defines a ByteArray event.
// For example, the agent is deployed as a proxy
// to directly transparently transmit the received binary data to the backend,
// using ByteArray can avoid additional Marshal/Unmarshal overhead.
type ByteArray []byte

func (ByteArray) GetName() string {
	return ""
}

func (ByteArray) SetName(name string) {
}

func (ByteArray) GetTags() Tags {
	return NilStringValues
}

func (ByteArray) GetType() EventType {
	return EventTypeByteArray
}

func (ByteArray) GetTimestamp() uint64 {
	return 0
}

func (ByteArray) GetObservedTimestamp() uint64 {
	return 0
}

func (ByteArray) SetObservedTimestamp(timestamp uint64) {
}

func (b ByteArray) GetSize() int64 {
	return int64(len(b))
}

func (b ByteArray) Clone() PipelineEvent {
	return b
}
