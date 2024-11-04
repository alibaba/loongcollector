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

#include "protobuf/sls/LogGroupSerializer.h"
#include "protobuf/sls/sls_logs.pb.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class LogGroupSerializerUnittest : public ::testing::Test {
public:
    void TestSerialize();
};

void LogGroupSerializerUnittest::TestSerialize() {
    size_t groupSZ = 0;
    vector<size_t> logSZ(2);
    {
        auto contentSZ = 0;
        contentSZ += GetLogContentSize(strlen("key_1"), strlen("value_1"));
        contentSZ += GetLogContentSize(strlen("key_2"), strlen("value_2"));
        groupSZ += GetLogSize(contentSZ, true, logSZ[0]);
    }
    {
        auto contentSZ = 0;
        contentSZ += GetLogContentSize(strlen("key_3"), strlen("value_3"));
        contentSZ += GetLogContentSize(strlen("key_4"), strlen("value_4"));
        groupSZ += GetLogSize(contentSZ, false, logSZ[1]);
    }
    groupSZ += GetStringSize(strlen("topic"));
    groupSZ += GetStringSize(strlen("source"));
    groupSZ += GetStringSize(strlen("machine_uuid"));
    groupSZ += GetLogTagSize(strlen("key_5"), strlen("value_5"));
    groupSZ += GetLogTagSize(strlen("key_6"), strlen("value_6"));

    LogGroupSerializer logGroup;
    logGroup.Prepare(groupSZ);
    logGroup.StartToAddLog(logSZ[0]);
    logGroup.AddLogTime(1234567890);
    logGroup.AddLogContent("key_1", "value_1");
    logGroup.AddLogContent("key_2", "value_2");
    logGroup.AddLogTimeNs(135792468);
    logGroup.StartToAddLog(logSZ[1]);
    logGroup.AddLogTime(123456789);
    logGroup.AddLogContent("key_3", "value_3");
    logGroup.AddLogContent("key_4", "value_4");
    logGroup.AddTopic("topic");
    logGroup.AddSource("source");
    logGroup.AddMachineUUID("machine_uuid");
    logGroup.AddLogTag("key_5", "value_5");
    logGroup.AddLogTag("key_6", "value_6");
    APSARA_TEST_EQUAL(groupSZ, logGroup.GetResult().size());

    sls_logs::LogGroup logGroupPb;
    APSARA_TEST_TRUE(logGroupPb.ParseFromString(logGroup.GetResult()));
    APSARA_TEST_EQUAL(2L, logGroupPb.logs_size());
    APSARA_TEST_EQUAL(1234567890U, logGroupPb.logs(0).time());
    APSARA_TEST_TRUE(logGroupPb.logs(0).has_time_ns());
    APSARA_TEST_EQUAL(135792468U, logGroupPb.logs(0).time_ns());
    APSARA_TEST_EQUAL(2L, logGroupPb.logs(0).contents_size());
    APSARA_TEST_EQUAL("key_1", logGroupPb.logs(0).contents(0).key());
    APSARA_TEST_EQUAL("value_1", logGroupPb.logs(0).contents(0).value());
    APSARA_TEST_EQUAL("key_2", logGroupPb.logs(0).contents(1).key());
    APSARA_TEST_EQUAL("value_2", logGroupPb.logs(0).contents(1).value());
    APSARA_TEST_EQUAL(1L << 28, logGroupPb.logs(1).time());
    APSARA_TEST_FALSE(logGroupPb.logs(1).has_time_ns());
    APSARA_TEST_EQUAL(2L, logGroupPb.logs(1).contents_size());
    APSARA_TEST_EQUAL("key_3", logGroupPb.logs(1).contents(0).key());
    APSARA_TEST_EQUAL("value_3", logGroupPb.logs(1).contents(0).value());
    APSARA_TEST_EQUAL("key_4", logGroupPb.logs(1).contents(1).key());
    APSARA_TEST_EQUAL("value_4", logGroupPb.logs(1).contents(1).value());
    APSARA_TEST_TRUE(logGroupPb.has_topic());
    APSARA_TEST_EQUAL("topic", logGroupPb.topic());
    APSARA_TEST_TRUE(logGroupPb.has_source());
    APSARA_TEST_EQUAL("source", logGroupPb.source());
    APSARA_TEST_TRUE(logGroupPb.has_machineuuid());
    APSARA_TEST_EQUAL("machine_uuid", logGroupPb.machineuuid());
    APSARA_TEST_FALSE(logGroupPb.has_category());
    APSARA_TEST_EQUAL(2L, logGroupPb.logtags_size());
    APSARA_TEST_EQUAL("key_5", logGroupPb.logtags(0).key());
    APSARA_TEST_EQUAL("value_5", logGroupPb.logtags(0).value());
    APSARA_TEST_EQUAL("key_6", logGroupPb.logtags(1).key());
    APSARA_TEST_EQUAL("value_6", logGroupPb.logtags(1).value());
}

UNIT_TEST_CASE(LogGroupSerializerUnittest, TestSerialize)

} // namespace logtail

UNIT_TEST_MAIN
