// Copyright 2025 iLogtail Authors
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

#include "common/CmsIdUtil.h"
#include "unittest/Unittest.h"

namespace logtail {

class CmsIdUtilUnittest : public ::testing::Test {
public:
    void TestEncodeUserId() {
        std::string userIdEncoded;
        int ret = EncodeUserId("123123123", userIdEncoded);
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL(userIdEncoded, "21ayer");
    }

    void TestGenAPMPid() {
        std::string pid;
        int res = GenAPMPid("1672753017899339", "mall-gateway", pid);
        APSARA_TEST_EQUAL(res, 0);
        APSARA_TEST_EQUAL(pid, "ggxw4lnjuz@7e393063f3fd6ad");
    }
    void TestWorkspace() {
        std::string userId = "1654218965343050";
        std::string regionId = "cn-hangzhou";
        auto ws = GetDefaultWorkspace(userId, regionId);
        APSARA_TEST_EQUAL(ws, "default-cms-1654218965343050-cn-hangzhou");
        APSARA_TEST_TRUE(IsDefaultWorkspace(userId, regionId, ""));
        APSARA_TEST_TRUE(IsDefaultWorkspace(userId, regionId, "default-cms-1654218965343050-cn-hangzhou"));
        APSARA_TEST_FALSE(IsDefaultWorkspace(userId, regionId, "abcd"));
    }
    void TestGenServiceId() {
        std::string pid;
        std::string userId = "1654218965343050";
        int res = GenAPMPid(userId, "cc-test", pid);
        APSARA_TEST_EQUAL(res, 0);
        std::string serviceId;
        res = GenerateServiceId(userId, "cn-hangzhou", pid, "kunshuo-hangzhou-stg", serviceId);
        APSARA_TEST_EQUAL(res, 0);
        APSARA_TEST_EQUAL(serviceId, "gaddp9ap8q@62b4210e37d02cf404728");
    }

    void TestAdaptMD5() {
        std::string md5 = "a13827b2e0db9efd042faf9ecdaa12f7";
        int ret = AdaptMD5(md5);
        APSARA_TEST_EQUAL(ret, 0);
        APSARA_TEST_EQUAL("a13827b2e0db9efd42faf9ecdaa12f7", md5);
    }

    void TestAPMProjectName() {
        std::string project;
        int res = GenerateWorkspaceAPMProject("kunshuo-hangzhou-stg", "1654218965343050", "cn-hangzhou", project);
        APSARA_TEST_EQUAL(res, 0);
        APSARA_TEST_EQUAL(project, "proj-xtrace-6cda6b8c5f47b529ab416e96c496118e-cn-hangzhou");

        // 默认 workspace
        res = GenerateWorkspaceAPMProject("default-cms-1654218965343050-cn-hangzhou", "1654218965343050", "cn-hangzhou", project);
        APSARA_TEST_EQUAL(res, 0);
        APSARA_TEST_EQUAL(project, "proj-xtrace-a13827b2e0db9efd42faf9ecdaa12f7-cn-hangzhou");

        // 不指定 workspace
        res = GenerateWorkspaceAPMProject("", "1654218965343050", "cn-hangzhou", project);
        APSARA_TEST_EQUAL(res, 0);
        APSARA_TEST_EQUAL(project, "proj-xtrace-a13827b2e0db9efd42faf9ecdaa12f7-cn-hangzhou");
    }
};

UNIT_TEST_CASE(CmsIdUtilUnittest, TestEncodeUserId);
UNIT_TEST_CASE(CmsIdUtilUnittest, TestGenAPMPid);
UNIT_TEST_CASE(CmsIdUtilUnittest, TestWorkspace);
UNIT_TEST_CASE(CmsIdUtilUnittest, TestGenServiceId);
UNIT_TEST_CASE(CmsIdUtilUnittest, TestAdaptMD5);
UNIT_TEST_CASE(CmsIdUtilUnittest, TestAPMProjectName);

} // namespace logtail

UNIT_TEST_MAIN
