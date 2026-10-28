// Copyright 2026 iLogtail Authors
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

#include "container_manager/ContainerDiff.h"
#include "file_server/ContainerInfo.h"
#include "unittest/Unittest.h"

namespace logtail {

class ContainerDiffUnittest : public testing::Test {
public:
    void TestToStringIncludesAllSections();
};

void ContainerDiffUnittest::TestToStringIncludesAllSections() {
    ContainerDiff d;
    auto a = std::make_shared<RawContainerInfo>();
    a->mName = "n1";
    a->mID = "id1";
    a->mStatus = "running";
    d.mAdded.push_back(a);

    auto m = std::make_shared<RawContainerInfo>();
    m->mName = "n2";
    m->mID = "id2";
    m->mStatus = "dead";
    d.mModified.push_back(m);

    d.mRemoved.push_back("rid");

    auto l = std::make_shared<RawContainerInfo>();
    l->mName = "leg";
    l->mID = "lid";
    d.mLegacyCheckpointAdded.emplace_back("k", l);

    d.mRefreshAllContainers = true;

    const auto s = d.ToString();
    APSARA_TEST_TRUE(s.find("n1") != std::string::npos);
    APSARA_TEST_TRUE(s.find("id1") != std::string::npos);
    APSARA_TEST_TRUE(s.find("running") != std::string::npos);
    APSARA_TEST_TRUE(s.find("n2") != std::string::npos);
    APSARA_TEST_TRUE(s.find("rid") != std::string::npos);
    APSARA_TEST_TRUE(s.find("leg") != std::string::npos);
    APSARA_TEST_TRUE(s.find("RefreshAllContainers: 1") != std::string::npos);
}

UNIT_TEST_CASE(ContainerDiffUnittest, TestToStringIncludesAllSections)

} // namespace logtail

UNIT_TEST_MAIN
