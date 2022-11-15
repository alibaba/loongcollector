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

#include <gtest/gtest.h>

#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"
#include "network/NetworkConfig.h"
#include "logger/Logger.h"

namespace logtail {

class ObserverConfigUnittest : public ::testing::Test {
public:
    void TestparseConfig() {
        auto cfg = NetworkConfig::GetInstance();

        cfg->mLastApplyedConfigDetail = "[\n"
                                        "    {\n"
                                        "        \"detail\":{\n"
                                        "            \"Common\":{\n"
                                        "                \"Sampling\":50,\n"
                                        "                \"FlushOutL4Interval\":5,\n"
                                        "                \"FlushOutL7Interval\":55,\n"
                                        "                \"FlushMetaInterval\":6,\n"
                                        "                \"FlushNetlinkInterval\":7,\n"
                                        "                \"ProtocolProcess\":true,\n"
                                        "                \"DropUnixSocket\":false,\n"
                                        "                \"DropLocalConnections\":false,\n"
                                        "                \"DropUnknownSocket\":false,\n"
                                        "                \"IncludeProtocols\":[\n"
                                        "                    \"MySQL\",\n"
                                        "                    \"PgSQL\"\n"
                                        "                ],\n"
                                        "                \"Tags\":{\n"
                                        "                    \"key\":\"val\"\n"
                                        "                },\n"
                                        "                \"ProtocolAggCfg\":{\n"
                                        "                    \"mysql\":{\n"
                                        "                        \"ClientSize\":1,\n"
                                        "                        \"ServerSize\":2\n"
                                        "                    },\n"
                                        "                    \"pgsql\":{\n"
                                        "                        \"ClientSize\":1,\n"
                                        "                        \"ServerSize\":2\n"
                                        "                    }\n"
                                        "                },\n"
                                        "                \"IncludeCmdRegex\":\"^in_cmd\",\n"
                                        "                \"ExcludeCmdRegex\":\"^ex_cmd$\",\n"
                                        "                \"IncludeContainerNameRegex\":\"^in_cname\",\n"
                                        "                \"ExcludeContainerNameRegex\":\"^ex_cname$\",\n"
                                        "                \"IncludePodNameRegex\":\"^in_pod\",\n"
                                        "                \"ExcludePodNameRegex\":\"^ex_pod$\",\n"
                                        "                \"IncludeNamespaceNameRegex\":\"^in_namespace\",\n"
                                        "                \"ExcludeNamespaceNameRegex\":\"^ex_namespace\",\n"
                                        "                \"IncludeContainerLabels\":{\n"
                                        "                    \"app3\":\"^test3$\"\n"
                                        "                },\n"
                                        "                \"ExcludeContainerLabels\":{\n"
                                        "                    \"app4\":\"^test4$\"\n"
                                        "                },\n"
                                        "                \"IncludeK8sLabels\":{\n"
                                        "                    \"app2\":\"^test2$\"\n"
                                        "                },\n"
                                        "                \"ExcludeK8sLabels\":{\n"
                                        "                    \"app1\":\"^test1$\"\n"
                                        "                },\n"
                                        "                \"IncludeEnvs\":{\n"
                                        "                    \"env1\":\"^env1\"\n"
                                        "                },\n"
                                        "                \"ExcludeEnvs\":{\n"
                                        "                    \"env2\":\"^env2\"\n"
                                        "                }\n"
                                        "            },\n"
                                        "            \"EBPF\":{\n"
                                        "                \"Enabled\":true\n"
                                        "            }\n"
                                        "        },\n"
                                        "        \"type\":\"observer_ilogtail_network_v1\"\n"
                                        "    }\n"
                                        "]";

        std::cout << cfg->mLastApplyedConfigDetail << std::endl;
        cfg->SetFromJsonString();

        APSARA_TEST_TRUE(cfg->mSampling == 50);
        APSARA_TEST_TRUE(cfg->mFlushOutL4Interval == 5);
        APSARA_TEST_TRUE(cfg->mFlushOutL7Interval == 55);
        APSARA_TEST_TRUE(cfg->mFlushMetaInterval == 6);
        APSARA_TEST_TRUE(cfg->mFlushNetlinkInterval == 7);
        APSARA_TEST_TRUE(cfg->mProtocolProcessFlag > 0);
        APSARA_TEST_TRUE(!cfg->mDropUnixSocket);
        APSARA_TEST_TRUE(!cfg->mDropLocalConnections);
        APSARA_TEST_TRUE(!cfg->mDropUnknownSocket);
        APSARA_TEST_TRUE(!cfg->mDropUnknownSocket);
        APSARA_TEST_TRUE(cfg->IsLegalProtocol(ProtocolType_MySQL));
        APSARA_TEST_TRUE(cfg->IsLegalProtocol(ProtocolType_PgSQL));
        APSARA_TEST_TRUE(!cfg->IsLegalProtocol(ProtocolType_DNS));
        APSARA_TEST_TRUE(!cfg->IsLegalProtocol(ProtocolType_Redis));
        APSARA_TEST_TRUE(cfg->mTags.size() == 1);
        APSARA_TEST_EQUAL(cfg->mTags[0].first, "__tag__:key");
        APSARA_TEST_EQUAL(cfg->mTags[0].second, "val");
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_Redis).first, 500);
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_Redis).second, 5000);
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_MySQL).first, 1);
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_MySQL).second, 2);
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_PgSQL).first, 1);
        APSARA_TEST_EQUAL(cfg->GetProtocolAggSize(ProtocolType_PgSQL).second, 2);
        APSARA_TEST_TRUE(!cfg->mExcludeCmdRegex.empty() && cfg->mExcludeCmdRegex.str() == "^ex_cmd$");
        APSARA_TEST_TRUE(!cfg->mExcludeContainerNameRegex.empty()
                         && cfg->mExcludeContainerNameRegex.str() == "^ex_cname$");
        APSARA_TEST_TRUE(!cfg->mExcludePodNameRegex.empty() && cfg->mExcludePodNameRegex.str() == "^ex_pod$");
        APSARA_TEST_TRUE(!cfg->mExcludeNamespaceNameRegex.empty()
                         && cfg->mExcludeNamespaceNameRegex.str() == "^ex_namespace");
        APSARA_TEST_TRUE(!cfg->mIncludeCmdRegex.empty() && cfg->mIncludeCmdRegex.str() == "^in_cmd");
        APSARA_TEST_TRUE(!cfg->mIncludeContainerNameRegex.empty()
                         && cfg->mIncludeContainerNameRegex.str() == "^in_cname");
        APSARA_TEST_TRUE(!cfg->mIncludePodNameRegex.empty() && cfg->mIncludePodNameRegex.str() == "^in_pod");
        APSARA_TEST_TRUE(!cfg->mIncludeNamespaceNameRegex.empty()
                         && cfg->mIncludeNamespaceNameRegex.str() == "^in_namespace");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mIncludeContainerLabels), "app3=^test3$,");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mExcludeContainerLabels), "app4=^test4$,");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mIncludeK8sLabels), "app2=^test2$,");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mExcludeK8sLabels), "app1=^test1$,");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mIncludeEnvs), "env1=^env1,");
        APSARA_TEST_EQUAL(cfg->label2String(cfg->mExcludeEnvs), "env2=^env2,");

        std::cout << cfg->ToString() << std::endl;
    }
};

APSARA_UNIT_TEST_CASE(ObserverConfigUnittest, TestparseConfig, 0);
} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
