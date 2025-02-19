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

#include "SystemInformationTools.h"
#include "host_monitor/Constants.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SystemInformationToolsUnittest : public testing::Test {
public:
    void TestGetSystemBootSeconds() const;
};

void SystemInformationToolsUnittest::TestGetSystemBootSeconds() const {
    PROCESS_DIR = ".";
    APSARA_TEST_EQUAL(1731142542, GetSystemBootSeconds());
}


UNIT_TEST_CASE(SystemInformationToolsUnittest, TestGetSystemBootSeconds);

} // namespace logtail

UNIT_TEST_MAIN
