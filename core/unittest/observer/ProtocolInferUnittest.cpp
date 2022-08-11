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

#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"
#include "observer/network/protocols/infer.h"


namespace logtail {

class ProtocolUtilUnittest : public ::testing::Test {
public:
    void TestInferPgSql() {
        // whole request
        char s[] = "\x42\x00\x00\x00\x0f\x00\x53\x5f\x31\x00\x00\x00\x00\x00\x00\x00\x45\x00\x00\x00\x09\x00\x00\x00"
                   "\x00\x01\x53\x00\x00\x00\x04";
        APSARA_TEST_TRUE(is_pgsql_message(s, 31, 0, 0));
        char passwd[] = "\x70\x00\x00\x00\x28\x6d\x64\x35\x64\x39\x65\x63\x38\x62\x39\x34"
                        "\x33\x30\x38\x65\x39\x62\x34\x33\x32\x39\x31\x30\x33\x62\x39\x37"
                        "\x32\x37\x37\x66\x61\x64\x65\x32\x00";
        APSARA_TEST_TRUE(is_pgsql_message(passwd, 41, 0, 0));

        // whole response
        char resp[] = "\x32\x00\x00\x00\x04\x49\x00\x00\x00\x04\x5a\x00\x00\x00\x05\x49";
        APSARA_TEST_TRUE(is_pgsql_message(resp, 16, 0, 0));
        char auth[] = "\x52\x00\x00\x00\x0c\x00\x00\x00\x05\x13\xbd\x41\x85";
        APSARA_TEST_TRUE(is_pgsql_message(auth, 13, 0, 0));

        char start_up[] = "\x00\x00\x00\x61\x00\x03\x00\x00\x64\x61\x74\x65\x73\x74\x79\x6c"
                          "\x65\x00\x49\x53\x4f\x2c\x20\x4d\x44\x59\x00\x63\x6c\x69\x65\x6e"
                          "\x74\x5f\x65\x6e\x63\x6f\x64\x69\x6e\x67\x00\x55\x54\x46\x38\x00"
                          "\x65\x78\x74\x72\x61\x5f\x66\x6c\x6f\x61\x74\x5f\x64\x69\x67\x69"
                          "\x74\x73\x00\x32\x00\x75\x73\x65\x72\x00\x6c\x6a\x70\x00\x64\x61"
                          "\x74\x61\x62\x61\x73\x65\x00\x70\x6f\x73\x74\x67\x72\x65\x73\x00\x00";
        APSARA_TEST_TRUE(is_pgsql_message(start_up, 97, 0, 0));
    }
};

APSARA_UNIT_TEST_CASE(ProtocolUtilUnittest, TestInferPgSql, 0);
} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}