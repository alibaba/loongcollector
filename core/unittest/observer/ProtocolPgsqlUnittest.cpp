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
#include "network/protocols/pgsql/inner_parser.h"
#include "RawNetPacketReader.h"
#include "unittest/UnittestHelper.h"
#include "network/protocols/pgsql/parser.h"
#include "observer/network/ProcessObserver.h"

namespace logtail {


class ProtocolPgSqlUnittest : public ::testing::Test {
public:
    void TestSimpleQueryRequest() {
        const char* data = "Q\000\000\000\033select * from account;\000";
        logtail::PgSQLParser pgsql(data, 28);
        pgsql.parse(MessageType::MessageType_Request);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Query);
        APSARA_TEST_TRUE(pgsql.query.sql == "select");
        APSARA_TEST_TRUE(pgsql.OK());
    }

    void TestExtendQueryRequest() {
        const std::string hexString = "50000000280053484f57205452414e53414354494f4e2049534f4c4154494f4e204c4556454c0000"
                                      "00420000000c000000000000000044000000065000450000000900000000005300000004";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType::MessageType_Request);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Query);
        APSARA_TEST_TRUE(pgsql.query.sql == "SHOW TRANSACTION ISOLATION LEVEL");
        APSARA_TEST_TRUE(pgsql.OK());
    }

    void TestSimpleQueryResponse() {
        const std::string hexString
            = "540000006e000475696400000040220001000000170004ffffffff0000757365726e616d650000004022000200000413ffff0000"
              "006800006465706172746e616d650000004022000300000413ffff000001f8000063726561746564000000402200040000043a00"
              "04ffffffff0000440000003c0004000000033331360000000d617374617869657570646174650000000ce7a094e58f91e983a8e9"
              "97a80000000a323031322d31322d3039430000000d53454c4543542031005a0000000549";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType::MessageType_Response);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Response);
        APSARA_TEST_TRUE(pgsql.resp.ok);
        APSARA_TEST_TRUE(pgsql.OK());
    }

    void TestExtendQueryResponse() {
        const std::string hexString
            = "31000000043200000004540000002e00017472616e73616374696f6e5f69736f6c6174696f6e0000000000000000000019ffffff"
              "ffffff0000440000001800010000000e7265616420636f6d6d6974746564430000000953484f57005a0000000549";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType::MessageType_Response);
        APSARA_TEST_TRUE(pgsql.OK());
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Response);
        APSARA_TEST_TRUE(pgsql.resp.ok);
    }

    void TestStartUp() {
        const std::string hexString
            = "000000650003000075736572006c6a70006461746162617365007465737400636c69656e745f656e636f64696e67005554463800"
              "446174655374796c650049534f0054696d655a6f6e65005554430065787472615f666c6f61745f64696769747300320000";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType::MessageType_Response);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::IgnoreQuery);
        APSARA_TEST_TRUE(pgsql.OK());
    }

    void TestErrorResp() {
        const std::string hexString
            = "450000005b534552524f5200564552524f5200433432363031004d73796e746178206572726f7220617420656e64206f6620696e"
              "7075740050343200467363616e2e6c004c3131373300527363616e6e65725f79796572726f720000";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType::MessageType_Response);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Response);
        APSARA_TEST_TRUE(!pgsql.resp.ok);
        APSARA_TEST_TRUE(pgsql.OK());
    }

    void TestPgSqlPacketReaderOrder() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.240.99.13", false, ProtocolType_PgSQL, rawHexs);
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        APSARA_TEST_TRUE(reader.OK());
        for (auto& packet : packets) {
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "query", "SHOW TRANSACTION ISOLATION LEVEL"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "status", "1"));
    }

    void TestPgSqlPacketReaderUnorder() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.240.99.13", false, ProtocolType_PgSQL, rawHexs);
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        APSARA_TEST_TRUE(reader.OK());
        for (int i = packets.size() - 1; i >= 0; --i) {
            mObserver->OnPacketEvent(&packets[i].at(0), packets[i].size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "query", "SHOW TRANSACTION ISOLATION LEVEL"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "status", "1"));
    }

    void TestPgSQLParserGC() {
        INT64_FLAG(sls_observer_network_process_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_connection_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_process_no_connection_timeout) = 0;
        BOOL_FLAG(sls_observer_network_protocol_stat) = true;

        NetworkStatistic* networkStatistic = NetworkStatistic::GetInstance();
        ProtocolDebugStatistic* protocolDebugStatistic = ProtocolDebugStatistic::GetInstance();
        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
        networkStatistic->Clear();
        {
            std::vector<std::string> rawHexs{rawHex1};
            RawNetPacketReader reader("30.240.99.13", false, ProtocolType_PgSQL, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionCachedSize, 0);
        }
        NetworkStatistic::Clear();
        {
            std::vector<std::string> rawHexs{rawHex2};
            RawNetPacketReader reader("30.240.99.13", false, ProtocolType_PgSQL, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mPgSQLConnectionCachedSize, 0);
        }
    }

    void TestSimplyQueryResponse() {
        //            const std::string hexString =
        //            "540000006e000475696400000040220001000000170004ffffffff0000757365726e616d650000004022000200000413ffff0000006800006465706172746e616d650000004022000300000413ffff000001f8000063726561746564000000402200040000043a0004ffffffff0000440000003c0004000000033330310000000d617374617869657570646174650000000ce7a094e58f91e983a8e997a80000000a323031322d31322d3039430000000d53454c4543542031005a0000000549";
        const std::string hexString = "31000000047400000012000300000413000004130000043a540000001c0001756964000000402200"
                                      "01000000170004ffffffff00005a0000000549";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::PgSQLParser pgsql((const char*)data.data(), (size_t)data.size());
        pgsql.parse(MessageType_Response);
        APSARA_TEST_EQUAL(pgsql.query.sql.ToString(), "");
        APSARA_TEST_EQUAL(pgsql.resp.ok, true);
        APSARA_TEST_TRUE(pgsql.type == PgSQLPacketType::Response);
    }


    NetworkObserver* mObserver = NetworkObserver::GetInstance();

    const std::string rawHex1
        = "00000000040088665a406acf080045000080000040004006c69b1ef0630d707c8163cbd31538ff44f48ea0c79df68018080058310000"
          "0101080a110926ce00c93c1650000000280053484f57205452414e53414354494f4e2049534f4c4154494f4e204c4556454c00000042"
          "0000000c000000000000000044000000065000450000000900000000005300000004";
    const std::string rawHex2 = "88665a406acf00000000040008004500009636e4400033069ca1707c81631ef0630d1538cbd3a0c79df6ff"
                                "44f4da8018004366df00000101080a00c93d1f1109277831000000043200000004540000002e0001747261"
                                "6e73616374696f6e5f69736f6c6174696f6e0000000000000000000019ffffffffffff0000440000001800"
                                "010000000e7265616420636f6d6d6974746564430000000953484f57005a0000000549";
};

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestStartUp, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestSimpleQueryRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestSimpleQueryResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestExtendQueryRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestExtendQueryResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestErrorResp, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestPgSqlPacketReaderOrder, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestPgSqlPacketReaderUnorder, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestPgSQLParserGC, 0);

APSARA_UNIT_TEST_CASE(ProtocolPgSqlUnittest, TestSimplyQueryResponse, 0);
} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}