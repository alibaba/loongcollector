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


#include <array>
#include <boost/format.hpp>

#include "Flags.h"
#include "MachineInfoUtil.h"
#include "common/FileSystemUtil.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_STRING(agent_host_id);

namespace logtail {

class InstanceIdentityUnittest : public ::testing::Test {
public:
    void TestECSMeta();
    void TestUpdateECSMeta();
    void TestInitFromFileRandomHostId();
    void TestInitFromFileEcsMeta();
};

UNIT_TEST_CASE(InstanceIdentityUnittest, TestECSMeta);
UNIT_TEST_CASE(InstanceIdentityUnittest, TestUpdateECSMeta);
UNIT_TEST_CASE(InstanceIdentityUnittest, TestInitFromFileRandomHostId);
UNIT_TEST_CASE(InstanceIdentityUnittest, TestInitFromFileEcsMeta);

void InstanceIdentityUnittest::TestECSMeta() {
    {
        ECSMeta meta;
        meta.SetInstanceID("i-1234567890");
        meta.SetUserID("1234567890");
        meta.SetRegionID("cn-hangzhou");
        APSARA_TEST_TRUE(meta.IsValid());
        APSARA_TEST_EQUAL(meta.GetInstanceID().to_string(), "i-1234567890");
        APSARA_TEST_EQUAL(meta.mInstanceIDLen, 12);
        APSARA_TEST_EQUAL(meta.GetUserID().to_string(), "1234567890");
        APSARA_TEST_EQUAL(meta.mUserIDLen, 10);
        APSARA_TEST_EQUAL(meta.GetRegionID().to_string(), "cn-hangzhou");
        APSARA_TEST_EQUAL(meta.mRegionIDLen, 11);
    }
    {
        ECSMeta meta;
        meta.SetInstanceID("");
        meta.SetUserID("1234567890");
        meta.SetRegionID("cn-hangzhou");
        APSARA_TEST_FALSE(meta.IsValid());
    }
    {
        ECSMeta meta;
        for (size_t i = 0; i < ID_MAX_LENGTH; ++i) {
            APSARA_TEST_EQUAL(meta.mInstanceID[i], '\0');
            APSARA_TEST_EQUAL(meta.mUserID[i], '\0');
            APSARA_TEST_EQUAL(meta.mRegionID[i], '\0');
        }
    }
    {
        // 测试设置字符串长度超过ID_MAX_LENGTH的情况
        ECSMeta meta;
        std::array<char, ID_MAX_LENGTH + 10> testString{};
        for (size_t i = 0; i < testString.size(); ++i) {
            testString[i] = 'a';
        }
        testString[testString.size() - 1] = '\0';
        meta.SetInstanceID(testString.data());
        meta.SetUserID(testString.data());
        meta.SetRegionID(testString.data());
        APSARA_TEST_TRUE(meta.IsValid());
        APSARA_TEST_EQUAL(meta.GetInstanceID().to_string(), StringView(testString.data(), ID_MAX_LENGTH - 1));
        APSARA_TEST_EQUAL(meta.GetUserID().to_string(), StringView(testString.data(), ID_MAX_LENGTH - 1));
        APSARA_TEST_EQUAL(meta.GetRegionID().to_string(), StringView(testString.data(), ID_MAX_LENGTH - 1));

        APSARA_TEST_EQUAL(meta.GetInstanceID().size(), ID_MAX_LENGTH - 1);
        APSARA_TEST_EQUAL(meta.GetUserID().size(), ID_MAX_LENGTH - 1);
        APSARA_TEST_EQUAL(meta.GetRegionID().size(), ID_MAX_LENGTH - 1);
    }
}

void InstanceIdentityUnittest::TestUpdateECSMeta() {
    STRING_FLAG(agent_host_id) = "test_host_id";
    {
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostID().to_string(), BOOL_FLAG(agent_host_id));
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostIdType(), Hostid::Type::CUSTOM);
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsInstanceID().to_string(), "");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsUserID().to_string(), "");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsRegionID().to_string(), "");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->IsECSValid(), false);
    }
    {
        // 更新合法的ecs meta
        ECSMeta meta;
        meta.SetInstanceID("i-1234567890");
        meta.SetUserID("1234567890");
        meta.SetRegionID("cn-hangzhou");
        InstanceIdentity::Instance()->UpdateInstanceIdentity(meta);
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostID().to_string(), "i-1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostIdType(), Hostid::Type::ECS);
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsInstanceID().to_string(), "i-1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsUserID().to_string(), "1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsRegionID().to_string(), "cn-hangzhou");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->IsECSValid(), true);
    }
    {
        // 更新不合法的ecs meta时，instanceIdentity不更新
        ECSMeta meta;
        InstanceIdentity::Instance()->UpdateInstanceIdentity(meta);
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostID().to_string(), "i-1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetHostIdType(), Hostid::Type::ECS);
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsInstanceID().to_string(), "i-1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsUserID().to_string(), "1234567890");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->GetEcsRegionID().to_string(), "cn-hangzhou");
        APSARA_TEST_EQUAL(InstanceIdentity::Instance()->GetEntity()->IsECSValid(), true);
    }
}

void InstanceIdentityUnittest::TestInitFromFileRandomHostId() {
    // 确保不会被自定义 host id 覆盖
    STRING_FLAG(agent_host_id) = "";

    const std::string dataDir = GetAgentDataDir();
    const std::string filePath = dataDir + "instance_identity";

    const std::string expectHostId = "68E47992-9600-11F0-8F6E-6D1B08DFD9DB";
    const std::string content = (boost::format(R"({
	"random-hostid" : "%1%"
}
)") % expectHostId)
                                    .str();

    std::string errMsg;
    APSARA_TEST_TRUE(WriteFile(filePath, content, errMsg));

    bool ok = InstanceIdentity::Instance()->InitFromFile();
    APSARA_TEST_TRUE(ok);
    APSARA_TEST_EQUAL(expectHostId, InstanceIdentity::Instance()->GetEntity()->GetHostID().to_string());
    APSARA_TEST_EQUAL(Hostid::Type::LOCAL, InstanceIdentity::Instance()->GetEntity()->GetHostIdType());
}

void InstanceIdentityUnittest::TestInitFromFileEcsMeta() {
    // 确保不会被自定义 host id 覆盖
    STRING_FLAG(agent_host_id) = "";

    const std::string dataDir = GetAgentDataDir();
    const std::string filePath = dataDir + "instance_identity";

    const std::string expectInstanceId = "i-abcdefghijklmnopqrst";
    const std::string expectUserId = "1234567890123456";
    const std::string expectRegion = "cn-hangzhou";
    const std::string content = (boost::format(R"({
	"instance-id" : "%1%",
	"owner-account-id" : "%2%",
	"region-id" : "%3%"
}
)") % expectInstanceId % expectUserId
                                 % expectRegion)
                                    .str();

    std::string errMsg;
    APSARA_TEST_TRUE(WriteFile(filePath, content, errMsg));

    bool ok = InstanceIdentity::Instance()->InitFromFile();
    APSARA_TEST_TRUE(ok);
    // HostId 应为 ECS，且为 instance-id
    APSARA_TEST_EQUAL(expectInstanceId, InstanceIdentity::Instance()->GetEntity()->GetHostID().to_string());
    APSARA_TEST_EQUAL(Hostid::Type::ECS, InstanceIdentity::Instance()->GetEntity()->GetHostIdType());
    // ECS 元数据应被填充
    APSARA_TEST_TRUE(InstanceIdentity::Instance()->GetEntity()->IsECSValid());
    APSARA_TEST_EQUAL(expectInstanceId, InstanceIdentity::Instance()->GetEntity()->GetEcsInstanceID().to_string());
    APSARA_TEST_EQUAL(expectUserId, InstanceIdentity::Instance()->GetEntity()->GetEcsUserID().to_string());
    APSARA_TEST_EQUAL(expectRegion, InstanceIdentity::Instance()->GetEntity()->GetEcsRegionID().to_string());
}

} // namespace logtail

UNIT_TEST_MAIN
