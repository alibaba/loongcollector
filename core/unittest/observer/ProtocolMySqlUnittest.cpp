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
#include "RawNetPacketReader.h"
#include "unittest/UnittestHelper.h"
#include "network/protocols/mysql/inner_parser.h"
#include "network/protocols/mysql/parser.h"
#include "observer/network/ProcessObserver.h"

namespace logtail {

class ProtocolMySqlUnittest : public ::testing::Test {
public:
    // Packet Length: 74
    // Packet Number: 0
    // Server Greeting
    //     Protocol: 10
    //     Version: 5.7.37
    //     Thread ID: 16
    //     Salt: !I\030\004\035u#'
    //     Server Capabilities: 0xffff
    //     Server Language: latin1 COLLATE latin1_swedish_ci (8)
    //     Server Status: 0x0002
    //     Extended Server Capabilities: 0xc1ff
    //     Authentication Plugin Length: 21
    //     Unused: 00000000000000000000
    //     Salt: \vgbuc@\036\001\005T^}
    //     Authentication Plugin: mysql_native_password
    void TestGreetingRequest() {
        const std::string hexString = "4a0000000a352e372e33370010000000214918041d75232700ffff080200ffc11500000000000000"
                                      "0000000b67627563401e0105545e7d006d7973716c5f6e61746976655f70617373776f726400";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeServerGreeting);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == false);
        APSARA_TEST_TRUE(mysql.OK());
    }

    // Packet Length: 32
    // Packet Number: 1
    // Login Request
    //     Client Capabilities: 0xaa07
    //     Extended Client Capabilities: 0x013e
    //     MAX Packet: 16777215
    //     Charset: utf8 COLLATE utf8_general_ci (33)
    //     Unused: 0000000000000000000000000000000000000000000000
    //     Username:
    void TestLoginRequest() {
        const std::string hexString = "2000000107aa3e01ffffff00210000000000000000000000000000000000000000000000";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeClientLogin);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == false);
        APSARA_TEST_TRUE(mysql.OK());
    }


    void TestQueryRequest() {
        const std::string hexString = "210000000373656c65637420404076657273696f6e5f636f6d6d656e74206c696d69742031";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeCollectStatement);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "select");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == false);
        APSARA_TEST_TRUE(mysql.OK());
    }

    void TestOkResponse() {
        const std::string hexString = "0700000100010102000000";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeResponse);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == true);
        APSARA_TEST_TRUE(mysql.OK());
    }

    void TestErrorResponse() {
        const std::string hexString
            = "a1000001ff2804233432303030596f75206861766520616e206572726f7220696e20796f75722053514c2073796e7461783b2063"
              "6865636b20746865206d616e75616c207468617420636f72726573706f6e647320746f20796f7572204d7953514c207365727665"
              "722076657273696f6e20666f72207468652072696768742073796e74617820746f20757365206e65617220272261616122292720"
              "6174206c696e652031";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeResponse);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == false);
        APSARA_TEST_TRUE(mysql.OK());
    }

    void Test5_6ResultSet() {
        const std::string hexString
            = "01000001022a00000203646566066d79746573740568656c6c6f0568656c6c6f0263310263310c080020000000fd00000000002a"
              "00000303646566066d79746573740568656c6c6f0568656c6c6f0263320263320c080020000000fd000000000005000004fe0000"
              "22000a000005046161613104616161320a0000060462626231046262623205000007fe00002200";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeResponse);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == true);
        APSARA_TEST_TRUE(mysql.OK());
    }

    void Test8_xResultSet() {
        const std::string hexString
            = "010000010a2d000002036465660765746c5f6e673206776f726b657206776f726b65720269640269640c3f001400000008034200"
              "000039000003036465660765746c5f6e673206776f726b657206776f726b657208486f73746e616d6508486f73746e616d650cff"
              "0000020000fd044000000037000004036465660765746c5f6e673206776f726b657206776f726b65720741646472657373074164"
              "64726573730cff0000010000fd00000000003f000005036465660765746c5f6e673206776f726b657206776f726b65720b536f75"
              "72636554797065730b536f7572636554797065730c3f00fffffffff590000000003b000006036465660765746c5f6e673206776f"
              "726b657206776f726b65720953696e6b54797065730953696e6b54797065730c3f00fffffffff590000000003500000703646566"
              "0765746c5f6e673206776f726b657206776f726b657206537461747573065374617475730c3f00080000000100000000003b0000"
              "08036465660765746c5f6e673206776f726b657206776f726b65720947726f75704e616d650947726f75704e616d650cff008000"
              "0000fd000000000031000009036465660765746c5f6e673206776f726b657206776f726b6572045461677304546167730c3f00ff"
              "fffffff590000000003d00000a036465660765746c5f6e673206776f726b657206776f726b65720a43726561746554696d650a43"
              "726561746554696d650c3f00140000000800000000004500000b036465660765746c5f6e673206776f726b657206776f726b6572"
              "0e4c6173744d6f6469667954696d650e4c6173744d6f6469667954696d650c3f00140000000800000000000500000cfe00002200"
              "0500000dfe00002200";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketTypeResponse);
        APSARA_TEST_TRUE(mysql.mysqlPacketQuery.sql == "");
        APSARA_TEST_TRUE(mysql.mysqlPacketResponse.ok == true);
        APSARA_TEST_TRUE(mysql.OK());
    }


    void TestMySQLPacketReader() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.121.61", false, ProtocolType_MySQL, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "query", "select"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "status", "1"));
    }


    void TestMySQLPacketReaderUnorder() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.121.61", false, ProtocolType_MySQL, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (int i = packets.size() - 1; i >= 0; --i) {
            mObserver->OnPacketEvent(&packets[i].at(0), packets[i].size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "query", "select"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "status", "1"));
    }

    // 0100000001
    void TestMySQLQuitPacket() {
        // mysql quit command
        const std::string pkt("0100000001");
        std::vector<uint8_t> data;
        logtail::hexstring_to_bin(pkt, data);

        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
        APSARA_TEST_EQUAL(mysql.mysqlPacketType, MySQLPacketNoResponseStatement);
    }


    void TestMysqlUnknownPacket() {
        const std::string pkt("0700000100000022080000");
        std::vector<uint8_t> data;
        logtail::hexstring_to_bin(pkt, data);
        logtail::MySQLParser mysql((const char*)data.data(), (size_t)data.size());
        mysql.parse();
    }

    void TestMysqlParserGC() {
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
            RawNetPacketReader reader("30.43.121.61", false, ProtocolType_MySQL, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionCachedSize, 0);
        }
        NetworkStatistic::Clear();
        {
            std::vector<std::string> rawHexs{rawHex2};
            RawNetPacketReader reader("30.43.121.61", false, ProtocolType_MySQL, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mMySQLConnectionCachedSize, 0);
        }
    }

    void TestMysqlCache() { MysqlCache cache(nullptr); }


    NetworkObserver* mObserver = NetworkObserver::GetInstance();
    const std::string rawHex1
        = "00749c945d39a07817a0852e08004500004c00004000400637031e2b793d0b9f60a2cb7b0cea933e3190a670a97080180801920c0000"
          "0101080a08d1f5fd7fc82492140000000373656c656374202a2066726f6d2068656c6c6f";
    const std::string rawHex2
        = "a07817a0852e00749c945d390800450000c330164000360610760b9f60a21e2b793d0ceacb7ba670a970933e31a880180039dc630000"
          "0101080a7fc848ca08d1f5fd01000001022a00000203646566066d79746573740568656c6c6f0568656c6c6f0263310263310c080020"
          "000000fd00000000002a00000303646566066d79746573740568656c6c6f0568656c6c6f0263320263320c080020000000fd00000000"
          "0005000004fe000022000a000005046161613104616161320a0000060462626231046262623205000007fe00002200";
};

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestGreetingRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestLoginRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestQueryRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestOkResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestErrorResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, Test5_6ResultSet, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, Test8_xResultSet, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestMySQLPacketReader, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestMySQLPacketReaderUnorder, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestMySQLQuitPacket, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestMysqlParserGC, 0);

APSARA_UNIT_TEST_CASE(ProtocolMySqlUnittest, TestMysqlCache, 0);
} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}