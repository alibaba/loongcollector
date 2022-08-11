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

#include "Logger.h"
#include "unittest/Unittest.h"
#include "unittest/UnittestHelper.h"
#include "metas/ServiceMetaCache.h"

namespace logtail {

class HostnameMetaUnittest : public ::testing::Test {
public:
    void testLRUCache() {
        ServiceMetaCache cache(5);
        cache.Put("1", "host", ProtocolType_HTTP);
        cache.Put("2", "host", ProtocolType_HTTP);
        cache.Put("3", "host", ProtocolType_HTTP);
        cache.Put("4", "host", ProtocolType_HTTP);
        cache.Put("5", "host", ProtocolType_HTTP);
        cache.Put("6", "host", ProtocolType_HTTP);
        APSARA_TEST_TRUE(cache.Get("1").Host.empty());
        for (int i = 2; i < 7; ++i) {
            auto key = std::to_string(i);
            APSARA_TEST_TRUE(!cache.Get(key).Host.empty());
        }
        APSARA_TEST_EQUAL(cache.mData.begin()->first, "6");
        cache.Get("2");
        APSARA_TEST_EQUAL(cache.mData.begin()->first, "2");
    }

    void testHostnameMetaManager() {
        auto instance = ServiceMetaManager::GetInstance();
        instance->AddHostName(1, "host", "ip");
        APSARA_TEST_EQUAL(instance->mHostnameMetas.size(), 1);
        instance->OnProcessDestroy(1);
        APSARA_TEST_EQUAL(instance->mHostnameMetas.size(), 0);

        instance->AddHostName(1, "host", "ip");
        instance->AddHostName(1, "host2", "ip2");
        instance->AddHostName(1, "host3", "ip3");

        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.size(), 3);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mIndexMap.size(), 3);
        INT64_FLAG(sls_observer_network_hostname_timeout) = 1;
        sleep(2);
        instance->GarbageTimeoutHostname(time(NULL));

        APSARA_TEST_EQUAL(instance->mHostnameMetas.size(), 0);

        instance->GetOrPutServiceMeta(1, "ip", ProtocolType_MySQL);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.size(), 1);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mIndexMap.size(), 1);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.front().first, "ip");
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.front().second.Host, "");
        APSARA_TEST_EQUAL(ServiceCategoryToString(instance->mHostnameMetas[1]->mData.front().second.Category),
                          ServiceCategoryToString(DetectRemoteServiceCategory(ProtocolType_MySQL)));
        APSARA_TEST_TRUE(instance->mHostnameMetas[1]->mData.front().second.time != 0);

        instance->GetOrPutServiceMeta(1, "ip2", ProtocolType_DNS);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.size(), 2);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mIndexMap.size(), 2);
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.front().first, "ip2");
        APSARA_TEST_EQUAL(instance->mHostnameMetas[1]->mData.front().second.Host, "");
        APSARA_TEST_EQUAL(ServiceCategoryToString(instance->mHostnameMetas[1]->mData.front().second.Category),
                          ServiceCategoryToString(DetectRemoteServiceCategory(ProtocolType_DNS)));
        APSARA_TEST_TRUE(instance->mHostnameMetas[1]->mData.front().second.time != 0);
        APSARA_TEST_TRUE(instance->GetServiceMeta(1, "ip3").Empty());
        APSARA_TEST_EQUAL(ServiceCategoryToString(instance->GetServiceMeta(1, "ip2").Category),
                          ServiceCategoryToString(DetectRemoteServiceCategory(ProtocolType_DNS)));
    }
};

APSARA_UNIT_TEST_CASE(HostnameMetaUnittest, testLRUCache, 0);

APSARA_UNIT_TEST_CASE(HostnameMetaUnittest, testHostnameMetaManager, 0);
} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
