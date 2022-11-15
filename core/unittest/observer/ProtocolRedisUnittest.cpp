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
#include "network/protocols/utils.h"
#include "network/protocols/redis/inner_parser.h"
#include "RawNetPacketReader.h"
#include "unittest/UnittestHelper.h"
#include "observer/network/ProcessObserver.h"

namespace logtail {


class ProtocolRedisUnittest : public ::testing::Test {
public:
    void TestCommonRequest() {
        // set aa 1;
        const std::string hexString = "2a330d0a24330d0a7365740d0a24320d0a61610d0a24320d0a313b0d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::RedisParser redis((const char*)data.data(), (size_t)data.size());
        redis.parse();
        APSARA_TEST_EQUAL(redis.redisData.isError, false);
        APSARA_TEST_EQUAL(redis.redisData.GetCommands(), "set");
    }

    void TestCommonResponse() {
        const std::string hexString = "2b4f4b0d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::RedisParser redis((const char*)data.data(), (size_t)data.size());
        redis.parse();
        APSARA_TEST_EQUAL(redis.redisData.isError, false);
        APSARA_TEST_EQUAL(redis.redisData.GetCommands(), "OK");
    }

    void TestErrResponse() {
        const std::string hexString = "2d45525220756e6b6e6f776e20636f6d6d616e64202741414141270d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::RedisParser redis((const char*)data.data(), (size_t)data.size());
        redis.parse();
        APSARA_TEST_EQUAL(redis.redisData.isError, true);
        APSARA_TEST_EQUAL(redis.redisData.GetCommands(), "ERR unknown command 'AAAA'");
    }

    void TestRangeResponse() {
        const std::string hexString = "2a330d0a24330d0a6f6e650d0a24330d0a74776f0d0a24350d0a74687265650d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::RedisParser redis((const char*)data.data(), (size_t)data.size());
        redis.parse();
        APSARA_TEST_EQUAL(redis.redisData.isError, false);
        APSARA_TEST_EQUAL(redis.redisData.GetCommands(), "one");
    }

    void TestIntegerResponse() {
        const std::string hexString = "3a340d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::RedisParser redis((const char*)data.data(), (size_t)data.size());
        redis.parse();
        APSARA_TEST_EQUAL(redis.redisData.isError, false);
        APSARA_TEST_EQUAL(redis.redisData.GetCommands(), "4");
    }

    void TestRedisPacketReader() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_Redis, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_EQUAL(allData.size(), size_t(1));

        sls_logs::Log* log = &allData[0];
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "query", "set"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "status", "1"));
    }

    void TestRedisPacketReaderUnorder() {
        // response first
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_Redis, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (int i = packets.size() - 1; i >= 0; --i) {
            mObserver->OnPacketEvent(&packets[i].at(0), packets[i].size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_EQUAL(allData.size(), size_t(1));

        sls_logs::Log* log = &allData[0];
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "query", "set"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "status", "1"));
    }

    void TestRedisParserGC() {
        INT64_FLAG(sls_observer_network_process_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_connection_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_process_no_connection_timeout) = 0;
        BOOL_FLAG(sls_observer_network_protocol_stat) = true;

        NetworkStatistic* networkStatistic = NetworkStatistic::GetInstance();
        ProtocolDebugStatistic* protocolDebugStatistic = ProtocolDebugStatistic::GetInstance();
        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
        NetworkStatistic::Clear();
        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);

        {
            std::vector<std::string> rawHexs{rawHex1};
            RawNetPacketReader reader("30.43.120.215", false, ProtocolType_Redis, rawHexs);
            APSARA_TEST_TRUE(reader.OK());
            std::vector<std::string> packets;
            reader.GetAllNetPackets(packets);
            for (auto& packet : packets) {
                mObserver->OnPacketEvent(&packet.at(0), packet.size());
            }
            std::vector<sls_logs::Log> allData;
            mObserver->FlushOutMetrics(allData);
            APSARA_TEST_EQUAL(allData.size(), 0);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds() - 60000000000000ULL);
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 0);

            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionCachedSize, 0);
        }
        NetworkStatistic::Clear();
        {
            std::vector<std::string> rawHexs{rawHex2};
            RawNetPacketReader reader("30.43.120.215", false, ProtocolType_Redis, rawHexs);
            APSARA_TEST_TRUE(reader.OK());
            std::vector<std::string> packets;
            reader.GetAllNetPackets(packets);
            for (auto& packet : packets) {
                mObserver->OnPacketEvent(&packet.at(0), packet.size());
            }
            std::vector<sls_logs::Log> allData;
            mObserver->FlushOutMetrics(allData);
            APSARA_TEST_EQUAL(allData.size(), 0);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds() - 60000000000000ULL);
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mRedisConnectionCachedSize, 0);
        }
    }


    NetworkObserver* mObserver = NetworkObserver::GetInstance();
    const std::string rawHex1
        = "00749c945d39a07817a0852e08004500005b000040004006375a1e2b78d70b9f60a2e2ae18ebeec9ad5886a8e17980180800c6e80000"
          "0101080a4d49243fd112ef9f2a330d0a24330d0a7365740d0a24310d0a610d0a2431320d0a6861686167617367667361660d0a";
    const std::string rawHex2 = "a07817a0852e00749c945d39080045000039b7464000370689350b9f60a21e2b78d718ebe2ae86a8e179ee"
                                "c9ad7f80180039a65a00000101080ad11409da4d49243f2b4f4b0d0a";
};

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestCommonRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestCommonResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestErrResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestRangeResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestIntegerResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestRedisPacketReader, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestRedisPacketReaderUnorder, 0);

APSARA_UNIT_TEST_CASE(ProtocolRedisUnittest, TestRedisParserGC, 0);


} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}