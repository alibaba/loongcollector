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

#include <string>

#include "boost/filesystem/operations.hpp"

#include "SystemInformationTools.h"
#include "host_monitor/Constants.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SystemInformationToolsUnittest : public testing::Test {
public:
    void TestGetHostSystemStat() const;

protected:
    void SetUp() override {
        bfs::create_directories("./1");
        ofstream ofs("./stat", std::ios::trunc);
        ofs << "btime 1731142542";
    }
};

void SystemInformationToolsUnittest::TestGetHostSystemStat() const {
    auto lines = vector<string>();
    std::string errorMessage;
    APSARA_TEST_TRUE(GetHostSystemStat(lines, errorMessage));
    APSARA_TEST_EQUAL(1, lines.size());
    APSARA_TEST_EQUAL("btime 1731142542", lines[0]);
}

} // namespace logtail

UNIT_TEST_MAIN
