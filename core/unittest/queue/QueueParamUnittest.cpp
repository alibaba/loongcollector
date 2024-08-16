// Copyright 2024 iLogtail Authors
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

#include "queue/QueueParam.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class BoundedQueueParamUnittest : public testing::Test {
public:
    void TestInit();
};

void BoundedQueueParamUnittest::TestInit() {
    {
        BoundedQueueParam param(100);
        APSARA_TEST_EQUAL(param.GetCapacity(), 100U);
        APSARA_TEST_EQUAL(param.GetHighWatermark(), 100U);
        APSARA_TEST_EQUAL(param.GetLowWatermark(), 66U);
    }
    {
        BoundedQueueParam param(0);
        APSARA_TEST_EQUAL(param.GetCapacity(), 1U);
        APSARA_TEST_EQUAL(param.GetHighWatermark(), 1U);
        APSARA_TEST_EQUAL(param.GetLowWatermark(), 0U);
    }
}

UNIT_TEST_CASE(BoundedQueueParamUnittest, TestInit)

} // namespace logtail

UNIT_TEST_MAIN
