// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SLSClientManager.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_STRING(default_access_key_id);
DECLARE_FLAG_STRING(default_access_key);

using namespace std;

namespace logtail {

class SLSClientManagerUnittest : public ::testing::Test {
public:
    void TestAccessKeyManagement();

private:
    SLSClientManager mManager;
};

void SLSClientManagerUnittest::TestAccessKeyManagement() {
    string accessKeyId, accessKeySecret;
    SLSClientManager::AuthType type;
    mManager.GetAccessKey("", type, accessKeyId, accessKeySecret);
    APSARA_TEST_EQUAL(SLSClientManager::AuthType::AK, type);
    APSARA_TEST_EQUAL(STRING_FLAG(default_access_key_id), accessKeyId);
    APSARA_TEST_EQUAL(STRING_FLAG(default_access_key), accessKeySecret);
}

UNIT_TEST_CASE(SLSClientManagerUnittest, TestAccessKeyManagement)

} // namespace logtail

UNIT_TEST_MAIN
