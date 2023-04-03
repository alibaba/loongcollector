// Copyright 2023 iLogtail Authors
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

package platformmeta

import "sync/atomic"

var MockManagerNum int64

type MockManager struct {
}

func (m *MockManager) StartCollect() {

}

func (m *MockManager) GetInstanceID() string {
	return "id_xxx"
}

func (m *MockManager) GetInstanceImageID() string {
	return "image_xxx"
}

func (m *MockManager) GetInstanceType() string {
	return "type_xxx"
}

func (m *MockManager) GetInstanceRegion() string {
	return "region_xxx"
}

func (m *MockManager) GetInstanceZone() string {
	return "zone_xxx"
}

func (m *MockManager) GetInstanceName() string {
	return "name_xxx"
}

func (m *MockManager) GetInstanceVpcID() string {
	return "vpc_xxx"
}

func (m *MockManager) GetInstanceVswitchID() string {
	return "vswitch_xxx"
}

func (m *MockManager) GetInstanceMaxNetEgress() int64 {
	return atomic.LoadInt64(&MockManagerNum) * 10
}

func (m *MockManager) GetInstanceMaxNetIngress() int64 {
	return atomic.LoadInt64(&MockManagerNum)
}

func (m *MockManager) GetInstanceTags() map[string]string {
	return map[string]string{
		"tag_key": "tag_val",
	}
}

func (m *MockManager) Ping() bool {
	return false
}

func initMock() {
	register[Mock] = &MockManager{}
}
