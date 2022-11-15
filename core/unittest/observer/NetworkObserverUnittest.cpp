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
#include "observer/network/NetworkObserver.h"
#include "JsonNetPacketReader.h"
#include "observer/interface/helper.h"
#include "observer/network/protocols/utils.h"
#include "RawNetPacketReader.h"
#include "observer/network/ConnectionObserver.h"
#include "observer/network/ProcessObserver.h"
#include "network/protocols/ProtocolEventAggregators.h"
#include "metas/ContainerProcessGroup.h"
#include "observer/network/protocols/infer.h"

namespace logtail {

class NetworkObserverUnittest : public ::testing::Test {
public:
    void TestToPB() {
        char packetType[sizeof(PacketEventHeader) + sizeof(PacketEventData)];
        PacketEventHeader* header = (PacketEventHeader*)packetType;
        header->EventType = PacketEventType_Data;
        header->PID = 8;
        PacketEventData* data = (PacketEventData*)(packetType + sizeof(PacketEventHeader));
        data->BufferLen = 0;
        data->RealLen = 1024;
        data->PtlType = ProtocolType_HTTP;
        mObserver->OnPacketEvent(packetType, sizeof(PacketEventHeader) + sizeof(PacketEventData));

        APSARA_TEST_EQUAL_FATAL(mObserver->mAllProcesses.size(), size_t(1));
        APSARA_TEST_EQUAL_FATAL(mObserver->mAllProcesses.begin()->first, 8);
        APSARA_TEST_EQUAL_FATAL(mObserver->mAllProcesses.begin()->second->mAllConnections.size(), size_t(1));
        ProtocolEventAggregators* agg = mObserver->mAllProcesses.begin()->second->GetAggregator();
        DNSProtocolEventAggregator* dnsAgg = agg->GetDNSAggregator();
        DNSProtocolEvent dnsEvent;
        dnsEvent.Info.ReqBytes = 100;
        dnsEvent.Info.RespBytes = 200;
        dnsEvent.Info.LatencyNs = 300;
        dnsEvent.Key.ReqResource = "cn-hangzhou.log.aliyuncs.com";
        dnsEvent.Key.RespStatus = 1;
        dnsEvent.Key.ConnKey.Role = PacketRoleType::Server;
        dnsAgg->AddEvent(std::move(dnsEvent));

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_EQUAL(allData.size(), size_t(1));

        std::cout << allData[0].DebugString();
        sls_logs::Log* log = &allData[0];
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(
            log, "local_info", "{\"_process_cmd_\":\"\",\"_process_pid_\":\"8\",\"_running_mode_\":\"host\"}"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "req_resource", "cn-hangzhou.log.aliyuncs.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "resp_status", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "protocol", "dns"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "role", "s"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "count", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "latency_ns", "300"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "req_bytes", "100"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(log, "resp_bytes", "200"));
    }


    void TestJsonPacketToPB() {
        JsonNetPacketReader reader("/tmp/wireshark.json", "30.43.121.41", false, ProtocolType_DNS);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            const char* data = packet.c_str();
            char* bData = &packet.at(0);
            PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));
            printf("%p %p %p %ld %ld \n", data, bData, pData->Buffer, pData->Buffer - bData, pData->Buffer - data);
            std::cout << PacketEventToString(&packet.at(0), packet.size()) << std::endl;
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_EQUAL(allData.size(), size_t(1));

        std::cout << allData[0].DebugString();
    }


    void TestJsonNetPacketReader() {
        JsonNetPacketReader reader("/tmp/wireshark.json", "30.43.121.41", false, ProtocolType_HTTP);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            std::cout << PacketEventToString(&packet.at(0), packet.size()) << std::endl;
        }
    }


    void TestRawPacketUDPReader() {
        std::string rawHex1
            = "a07817a0852e00749c945d39080045000057518900007a111c121e1e1e1e1e2b78940035c09800430bab1dd68180000100020000"
              "000005626169647503636f6d0000010001c00c00010001000001300004dcb52694c00c00010001000001300004dcb526fb";
        std::vector<std::string> rawHexs{rawHex1};
        RawNetPacketReader reader("30.43.120.148", true, ProtocolType_DNS, rawHexs);
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        if (!reader.OK()) {
            std::cerr << reader.GetParseFailMsg() << std::endl;
        }
    }

    void TestRawPacketTCPReader() {
        std::string rawHex1 = "00749c945d39a07817a0852e080045000071000040004006a0581e2b7853dcb526fbcd4700506b7401eca591"
                              "a5ce5018100037ad0000474554202f20485454502f312e310d0a486f73743a2062616964752e636f6d0d0a55"
                              "7365722d4167656e743a206375726c2f372e37372e300d0a4163636570743a202a2f2a0d0a0d0a";
        std::vector<std::string> rawHexs{rawHex1};
        RawNetPacketReader reader("30.43.120.83", true, ProtocolType_HTTP, rawHexs);
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        if (!reader.OK()) {
            std::cerr << reader.GetParseFailMsg() << std::endl;
        }
    }


    void inferRedis() {
        std::string rawHex1 = "00749c945d39a07817a0852e08004500005b000040004006375a1e2b78d70b9f60a2e2ae18ebeec9ad5886a8"
                              "e17980180800c6e800000101080a4d49243fd112ef9f2a330d0a24330d0a7365740d0a24310d0a610d0a2431"
                              "320d0a6861686167617367667361660d0a";
        std::string rawHex2 = "a07817a0852e00749c945d39080045000039b7464000370689350b9f60a21e2b78d718ebe2ae86a8e179eec9"
                              "ad7f80180039a65a00000101080ad11409da4d49243f2b4f4b0d0a";
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_None, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            char* bData = &packet.at(0);
            PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));

            PacketEventHeader header;
            auto ret = infer_protocol(&header, PacketType_In, pData->Buffer, pData->BufferLen, pData->BufferLen);
            APSARA_TEST_TRUE(std::get<0>(ret) == ProtocolType_Redis);
        }
    }

    void inferDNS() {
        std::string rawHex1
            = "a07817a0852e00749c945d39080045000057518900007a111c121e1e1e1e1e2b78940035c09800430bab1dd68180000100020000"
              "000005626169647503636f6d0000010001c00c00010001000001300004dcb52694c00c00010001000001300004dcb526fb";
        std::vector<std::string> rawHexs{rawHex1};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_None, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            char* bData = &packet.at(0);
            PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));

            PacketEventHeader header;
            auto ret = infer_protocol(&header, PacketType_In, pData->Buffer, pData->BufferLen, pData->BufferLen);
            APSARA_TEST_TRUE(std::get<0>(ret) == ProtocolType_DNS);
        }
    }

    void inferHTTP() {
        std::string rawHex1 = "00749c945d39a07817a0852e080045000071000040004006a0581e2b7853dcb526fbcd4700506b7401eca591"
                              "a5ce5018100037ad0000474554202f20485454502f312e310d0a486f73743a2062616964752e636f6d0d0a55"
                              "7365722d4167656e743a206375726c2f372e37372e300d0a4163636570743a202a2f2a0d0a0d0a";
        std::string rawHex2
            = "a07817a0852e00749c945d390800450001598c7240002a0628fedcb526fb1e2b78530050cd47a591a5ce6b74023550180304c265"
              "0000485454502f312e3120323030204f4b0d0a446174653a205468752c203237204a616e20323032322030393a34363a31332047"
              "4d540d0a5365727665723a204170616368650d0a4c6173742d4d6f6469666965643a205475652c203132204a616e203230313020"
              "31333a34383a303020474d540d0a455461673a202235312d34376366376536656538343030220d0a4163636570742d52616e6765"
              "733a2062797465730d0a436f6e74656e742d4c656e6774683a2038310d0a43616368652d436f6e74726f6c3a206d61782d616765"
              "3d38363430300d0a457870697265733a204672692c203238204a616e20323032322030393a34363a313320474d540d0a436f6e6e"
              "656374696f6e3a204b6565702d416c6976650d0a436f6e74656e742d547970653a20746578742f68746d6c0d0a0d0a";
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_None, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            char* bData = &packet.at(0);
            PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));

            PacketEventHeader header;
            auto ret = infer_protocol(&header, PacketType_In, pData->Buffer, pData->BufferLen, pData->BufferLen);
            APSARA_TEST_TRUE(std::get<0>(ret) == ProtocolType_HTTP);
        }
    }

    void inferMySQL() {
        std::string rawHex1
            = "00749c945d39a07817a0852e08004500004c00004000400637031e2b793d0b9f60a2cb7b0cea933e3190a670a97080180801920c"
              "00000101080a08d1f5fd7fc82492140000000373656c656374202a2066726f6d2068656c6c6f";
        std::string rawHex2 = "a07817a0852e00749c945d390800450000c330164000360610760b9f60a21e2b793d0ceacb7ba670a970933e"
                              "31a880180039dc6300000101080a7fc848ca08d1f5fd01000001022a00000203646566066d79746573740568"
                              "656c6c6f0568656c6c6f0263310263310c080020000000fd00000000002a00000303646566066d7974657374"
                              "0568656c6c6f0568656c6c6f0263320263320c080020000000fd000000000005000004fe000022000a000005"
                              "046161613104616161320a0000060462626231046262623205000007fe00002200";
        // login request
        std::string rawHex3 = "00749c945d39a07817a0852e0800450000f800004000400692a01e2b78cc0b9f04cae5290cea916ada492b07"
                              "2bc9801808090f9100000101080abb07480dccbb2a2bc00000018da6ff0900000001ff000000000000000000"
                              "0000000000000000000000000000736c73000065746c5f6e67320063616368696e675f736861325f70617373"
                              "776f7264007c035f6f73086f737831302e3136095f706c6174666f726d067838365f36340f5f636c69656e74"
                              "5f76657273696f6e06382e302e32330c5f636c69656e745f6e616d65086c69626d7973716c045f7069640532"
                              "39373237076f735f757365720a73756e796f6e676875610c70726f6772616d5f6e616d65056d7973716c";
        std::vector<std::string> rawHexs{rawHex1, rawHex2, rawHex3};
        RawNetPacketReader reader("30.43.120.215", false, ProtocolType_None, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            char* bData = &packet.at(0);
            PacketEventData* pData = (PacketEventData*)(bData + sizeof(PacketEventHeader));
            PacketEventHeader header;
            auto ret = infer_protocol(&header, PacketType_In, pData->Buffer, pData->BufferLen, pData->BufferLen);
            APSARA_TEST_TRUE(std::get<0>(ret) == ProtocolType_MySQL);
        }
    }

    void TestInferProtocol() {
        inferRedis();
        inferDNS();
        inferHTTP();
        inferMySQL();
    }

    NetworkObserver* mObserver = NetworkObserver::GetInstance();
};


APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestToPB, 0);
//    APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestJsonNetPacketReader, 0);
//    APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestJsonPacketToPB, 0);
APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestRawPacketUDPReader, 0);
APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestRawPacketTCPReader, 0);
APSARA_UNIT_TEST_CASE(NetworkObserverUnittest, TestInferProtocol, 0);
} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
