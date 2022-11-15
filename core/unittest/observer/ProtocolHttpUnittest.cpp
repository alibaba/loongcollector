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
#include "network/protocols/http/inner_parser.h"
#include "RawNetPacketReader.h"
#include "network/protocols/http/parser.h"
#include "observer/network/ProcessObserver.h"
#include "unittest/UnittestHelper.h"

namespace logtail {


class ProtocolHttpUnittest : public ::testing::Test {
public:
    std::string getHeaders(logtail::HTTPParser& http, std::string name) {
        for (size_t i = 0; i < http.packet.common.headersNum; ++i) {
            if (std::strncmp(http.packet.common.headers[i].name, name.c_str(), name.length()) == 0) {
                return std::string(http.packet.common.headers[i].value, http.packet.common.headers[i].value_len);
            }
        }
        return "";
    }

    // POST /a HTTP/1.1\r\n
    //     [Expert Info (Chat/Sequence): POST /a HTTP/1.1\r\n]
    //     Request Method: POST
    //     Request URI: /a
    //     Request Version: HTTP/1.1
    // Host: ocs-oneagent-server.alibaba.com\r\n
    // Content-Type: application/json\r\n
    // Connection: keep-alive\r\n
    // Accept: */*\r\n
    // User-Agent: CloudShell/5.0.42 (Mac OS X Version 12.2.1 (Build 21D62))\r\n
    // Accept-Language: zh-Hans-CN;q=1\r\n
    // Accept-Encoding: gzip, deflate\r\n
    // Content-Length: 1475\r\n
    //     [Content length: 1475]
    // \r\n
    // [Full request URI: http://ocs-oneagent-server.alibaba.com/a]
    // [HTTP request 3/24]
    // [Prev request in frame: 2962]
    // [Response in frame: 4580]
    // [Next request in frame: 5847]
    // File Data: 1475 bytes
    void TestCommonRequest() {
        const std::string hexString
            = "504f5354202f6120485454502f312e310d0a486f73743a206f63732d6f6e656167656e742d7365727665722e616c69626162612e"
              "636f6d0d0a436f6e74656e742d547970653a206170706c69636174696f6e2f6a736f6e0d0a436f6e6e656374696f6e3a206b6565"
              "702d616c6976650d0a4163636570743a202a2f2a0d0a557365722d4167656e743a20436c6f75645368656c6c2f352e302e343220"
              "284d6163204f5320582056657273696f6e2031322e322e3120284275696c6420323144363229290d0a4163636570742d4c616e67"
              "756167653a207a682d48616e732d434e3b713d310d0a4163636570742d456e636f64696e673a20677a69702c206465666c617465"
              "0d0a436f6e74656e742d4c656e6774683a20313437350d0a0d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::HTTPParser http;
        http.ParseRequest((const char*)data.data(), (size_t)data.size());
        APSARA_TEST_TRUE(http.status >= 0);
        APSARA_TEST_TRUE(http.packet.msg.req.method == "POST");
        APSARA_TEST_TRUE(http.packet.msg.req.url == "/a");
        APSARA_TEST_TRUE(http.packet.common.version == 1);
        APSARA_TEST_EQUAL(http.packet.common.headersNum, 8);
        APSARA_TEST_EQUAL(getHeaders(http, "Host"), "ocs-oneagent-server.alibaba.com");
        APSARA_TEST_EQUAL(getHeaders(http, "Content-Type"), "application/json");
        APSARA_TEST_EQUAL(getHeaders(http, "Connection"), "keep-alive");
        APSARA_TEST_EQUAL(getHeaders(http, "Accept"), "*/*");
        APSARA_TEST_EQUAL(getHeaders(http, "User-Agent"), "CloudShell/5.0.42 (Mac OS X Version 12.2.1 (Build 21D62))");
        APSARA_TEST_EQUAL(getHeaders(http, "Accept-Language"), "zh-Hans-CN;q=1");
        APSARA_TEST_EQUAL(getHeaders(http, "Accept-Encoding"), "gzip, deflate");
        APSARA_TEST_EQUAL(getHeaders(http, "Content-Length"), "1475");
    }


    void TestCommonRequest2() {
        const std::string hexString
            = "504f5354202f6c6f6773746f7265732f6c6f677461696c5f7374617475735f70726f66696c652f7368617264732f6c6220485454"
              "502f312e310d0a486f73743a616c692d636e2d7368616e676861692d636f72702d736c732d61646d696e2e636e2d7368616e6768"
              "61692d636f72702e736c732e616c6979756e63732e636f6d0d0a4163636570743a202a2f2a0d0a417574686f72697a6174696f6e"
              "3a4c4f47203934746f337a3431387975706936696b61777171643337303a69544b46686c36736f6e6a6a715447306437356d5433"
              "474d6438413d0d0a436f6e74656e742d4c656e6774683a313135340d0a436f6e74656e742d4d44353a3637463146384143333136"
              "4137383238304342304131343037313937464344340d0a436f6e74656e742d547970653a6170706c69636174696f6e2f782d7072"
              "6f746f6275660d0a446174653a5475652c203239204d617220323032322031313a32353a313620474d540d0a557365722d416765"
              "6e743a616c692d6c6f672d6c6f677461696c0d0a782d6c6f672d61706976657273696f6e3a302e362e300d0a782d6c6f672d626f"
              "647972617773697a653a313738300d0a782d6c6f672d636f6d7072657373747970653a6c7a340d0a782d6c6f672d6b657970726f"
              "76696465723a6d64352d736861312d73616c740d0a782d6c6f672d7369676e61747572656d6574686f643a686d61632d73686131"
              "0d0a4578706563743a203130302d636f6e74696e75650d0a0d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::HTTPParser http;
        http.ParseRequest((const char*)data.data(), (size_t)data.size());
        APSARA_TEST_TRUE(http.status >= 0);
        APSARA_TEST_EQUAL(http.packet.msg.req.method.ToString(), "POST");
        APSARA_TEST_EQUAL(http.packet.msg.req.url.ToString(), "/logstores/logtail_status_profile/shards/lb");
        APSARA_TEST_EQUAL(http.packet.common.version, 1);
        APSARA_TEST_EQUAL(http.packet.common.headersNum, 14);

        APSARA_TEST_EQUAL(getHeaders(http, "Date"), "Tue, 29 Mar 2022 11:25:16 GMT");
        APSARA_TEST_EQUAL(getHeaders(http, "Host"), "ali-cn-shanghai-corp-sls-admin.cn-shanghai-corp.sls.aliyuncs.com");
        APSARA_TEST_EQUAL(getHeaders(http, "Accept"), "*/*");
        APSARA_TEST_EQUAL(getHeaders(http, "Expect"), "100-continue");
        APSARA_TEST_EQUAL(getHeaders(http, "User-Agent"), "ali-log-logtail");
        APSARA_TEST_EQUAL(getHeaders(http, "Content-Type"), "application/x-protobuf");
    }


    //        HTTP/1.1 200 OK\r\n
    //                Server: Tengine\r\n
    //                Content-Length: 0\r\n
    //                Connection: close\r\n
    //                Access-Control-Allow-Origin: *\r\n
    //                Date: Sun, 19 Jun 2022 13:36:44 GMT\r\n
    //                x-log-append-meta: true\r\n
    //                x-log-time: 1655645804\r\n
    //                x-log-requestid: 62AF266CFF534E3CBFEB797C\r\n
    //        \r\n
    //        [HTTP response 2/2]
    //        [Time since request: 0.015786000 seconds]
    //        [Prev response in frame: 97700]
    //        [Request in frame: 97703]
    //        [Request URI:
    //        http://edr-project.cn-beijing.log.aliyuncs.com/logstores/bradar_performance_statistics_log/shards/lb]
    void TestCommonResponse() {
        const std::string hexString
            = "485454502f312e3120323030204f4b0d0a5365727665723a2054656e67696e650d0a436f6e74656e742d4c656e6774683a20300d"
              "0a436f6e6e656374696f6e3a20636c6f73650d0a4163636573732d436f6e74726f6c2d416c6c6f772d4f726967696e3a202a0d0a"
              "446174653a2053756e2c203139204a756e20323032322031333a33363a343420474d540d0a782d6c6f672d617070656e642d6d65"
              "74613a20747275650d0a782d6c6f672d74696d653a20313635353634353830340d0a782d6c6f672d7265717565737469643a2036"
              "32414632363643464635333445334342464542373937430d0a0d0a";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::HTTPParser http;
        http.ParseResp((const char*)data.data(), (size_t)data.size());
        APSARA_TEST_TRUE(http.status >= 0);
        APSARA_TEST_TRUE(http.packet.common.version == 1);
        APSARA_TEST_TRUE(http.packet.msg.resp.code == 200);
        APSARA_TEST_TRUE(http.packet.msg.resp.msg == "OK");

        APSARA_TEST_EQUAL(http.packet.common.headersNum, 8);
        APSARA_TEST_EQUAL(getHeaders(http, "Server"), "Tengine");
        APSARA_TEST_EQUAL(getHeaders(http, "Content-Length"), "0");
        APSARA_TEST_EQUAL(getHeaders(http, "Connection"), "close");
        APSARA_TEST_EQUAL(getHeaders(http, "Access-Control-Allow-Origin"), "*");
        APSARA_TEST_EQUAL(getHeaders(http, "x-log-append-meta"), "true");
        APSARA_TEST_EQUAL(getHeaders(http, "x-log-time"), "1655645804");
        APSARA_TEST_EQUAL(getHeaders(http, "x-log-requestid"), "62AF266CFF534E3CBFEB797C");
    }

    void TestHTTPPacketReaderOrder() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.83", false, ProtocolType_HTTP, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }

        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_domain", "baidu.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_type", "GET"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_resource", "/"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_code", "200"));
    }


    void TestHTTPPacketReaderUnorder() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2};
        RawNetPacketReader reader("30.43.120.83", false, ProtocolType_HTTP, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (int i = packets.size() - 1; i >= 0; --i) {
            mObserver->OnPacketEvent(&packets[i].at(0), packets[i].size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_domain", "baidu.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_type", "GET"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_resource", "/"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_code", "200"));
    }

    void TestMoreContentResponse() {
        std::vector<std::string> rawHexs{
            // req
            "00000000040088665a406acf08004500015100004000400640941ef065a6cb77a905f5430050f351495f1109db5d50181000343500"
            "00504f5354202f6120485454502f312e310d0a486f73743a206f63732d6f6e656167656e742d7365727665722e616c69626162612e"
            "636f6d0d0a436f6e74656e742d547970653a206170706c69636174696f6e2f6a736f6e0d0a436f6e6e656374696f6e3a206b656570"
            "2d616c6976650d0a4163636570743a202a2f2a0d0a557365722d4167656e743a20436c6f75645368656c6c2f382e312e343220284d"
            "6163204f5320582056657273696f6e2031322e3420284275696c6420323146373929290d0a4163636570742d4c616e67756167653a"
            "20656e2d434e3b713d312c207a682d48616e732d434e3b713d302e390d0a4163636570742d456e636f64696e673a20677a69702c20"
            "6465666c6174650d0a436f6e74656e742d4c656e6774683a20313536380d0a0d0a",
            "00000000040088665a406acf0800450005c80000400040063c1d1ef065a6cb77a905f5430050f3514a881109db5d501010002eb900"
            "007b2276657273696f6e223a332c2276616c7565223a7b2263736964223a2239433130463434362d313138462d343745342d413530"
            "422d363131443932464134373936222c226b6579223a2250576e775a2b5946435070492b73377238412b4e49457077494154413758"
            "713244363568376a67564c32396c384b4b5858666e49466b64445c2f4e7776447674544e67707048357a384e544370365963314545"
            "45706467314758534c36415674484f772b763674684965377972454f6e7a6a6c6f6865434f64305247734f39626a38684a4a2b5858"
            "4d4f35544a526f327a4b68413079684a777058315a2b466b6b53343661646558473730303d222c2276616c7565223a226575463576"
            "4c425c2f4669334c67346967304b627544387578343554517164464d674b62357249426b3245744e6b343853416e594a636264374a"
            "6a6469393548714f38365a6d5a494e65573074365268714f30656e554a70593242584738542b32657578787635524a37646a715a54"
            "674269684c4e30624a70314b4a732b653751384a6b497a53593334524670556278384c6f5579745758437765703061576f45787173"
            "6d565665656f2b6749632b7a334b543341766e5564314a7a4c5a334a375772596a6f57675456774b495644487456747a707a393333"
            "763058594d52774f3673684e51657749634839684c7462507078534e575537483130466b71783269476f48475733644a4a6d527168"
            "703161724d477974564961442b2b70666762387272584a486c48562b68396e64525c2f6e7349666d2b566c38714d72726338375459"
            "7338714e79584a453333325334304d595c2f4c4e50647967336e576a67594e2b4432386f5c2f4b335a3032366f574a7a5c2f74457a"
            "2b716558386b6f6b45706449587861526c754638706270486b41496268565435356d5a735c2f6d48444547526c786a31665678636a"
            "2b4f72577a535432655554734d7731795849586d504e4172333170417a4e47594a536445744962746d72785879746569654c39732b"
            "734842424c4255416a7241334e6f4a336c62376f626f696250526a51674c4d49615a4831687251654e497835476379583565364b67"
            "32717a304f5777654a31694f6241786971725c2f776441434c5a522b444151315c2f3871537a694746727172724d6c4a785472542b"
            "5a735045345442714f364e6d7a6d6b3350514e716e486c4d5736556d7951467275476265304b3558394e71417550706b7459427065"
            "594b38424c6f317463556c38707253315765306534586a41594b74374f6931446c53425568796c704273487a2b48645a586e574766"
            "4c494648434f615477762b45644938396675435163587762326157446c31474939515c2f44552b49313346457477706c3379716457"
            "6c476f7850315a3658345751366b4478786b48576d42696e4e73596d6a367665366c5533437741576d676f6d534f70426f5c2f3866"
            "57597a326d4f773939677155327a49526a787336352b53456e486b48736e4e5c2f714d57563341396e4c7974356835375c2f675732"
            "664a4a30474a4d39335661636f756d6c556a7448704c4d6c724f3173346a656b3942476e635346757a6e5a4e3166426d6a72757255"
            "4564637876566b765164746e2b324f417a684f6b5c2f57426a59614d7238373732363974624e6367615156374251376c4f54794774"
            "4a4e55495334386675723158547535707938694a756d6d674655684f4c747876585662646465344c545c2f6e357263763738724c31"
            "426d5274762b4347435930354859584a474468742b30514b6153465339415c2f36745c2f4d742b624145756a4f596f6e3442563741"
            "79796f47435a574d744b566f414876614a6b4469636e4773422b5369566e744a6239646978436b4f706c5469487139466e36705841"
            "456475516b6a6c695c2f6f4548396732426d4675695c2f665a4363345978687065395c2f4e702b5276587a474d5c2f6847734e596e"
            "4a374e32687352683771",
            "00000000040088665a406acf0800450000a8000040004006413d1ef065a6cb77a905f5430050f35150281109db5d5018100013a700"
            "006f67735a6f355c2f6f34734851554c56776b4568674231554e5c2f436641645039306a682b626e3139465a70352b6e327a396c77"
            "445559654e765a55666f684156414e5a6a433955446d30654261513d3d222c227273615f6b65795f76657273696f6e223a2233222c"
            "226165735f6b65795f76657273696f6e223a2233227d7d",
            // resp
            "88665a406acf00000000040008004500014cf4b74000340657e1cb77a9051ef065a60050f5431109db5df35150a85018001e877700"
            "00485454502f312e3120323030200d0a446174653a204d6f6e2c203230204a756e20323032322030383a32363a353720474d540d0a"
            "436f6e74656e742d547970653a206170706c69636174696f6e2f6a736f6e3b636861727365743d5554462d380d0a436f6e74656e74"
            "2d4c656e6774683a2035330d0a436f6e6e656374696f6e3a206b6565702d616c6976650d0a582d4170706c69636174696f6e2d436f"
            "6e746578743a206f63732d6f6e656167656e742d7365727665723a373030310d0a5365727665723a2054656e67696e652f41736572"
            "7665720d0a4561676c654579652d547261636549643a20323132626463353131363535373133363137353439333436356531653465"
            "0d0a54696d696e672d416c6c6f772d4f726967696e3a202a0d0a0d0a",
            "88665a406acf00000000040008004500005df4b84000340658cfcb77a9051ef065a60050f5431109dc81f35150a85018001e13e600"
            "007b22656e63727970746564223a747275652c2276616c7565223a22584f5865636476656c4c4471376d36422b57563149673d3d22"
            "7d",
        };


        std::vector<ParseResult> expectRes = {
            ParseResult_OK,
            ParseResult_Partial,
            ParseResult_Partial,
            ParseResult_OK,
            ParseResult_Partial,
        };

        RawNetPacketReader reader("30.240.101.166", false, ProtocolType_HTTP, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        auto aggregator = new HTTPProtocolEventAggregator(200, 1001);
        static HTTPProtocolParser* parser = nullptr;
        for (size_t i = 0; i < packets.size(); ++i) {
            PacketEventHeader* header = static_cast<PacketEventHeader*>((void*)&packets[i].at(0));
            PacketEventData* data
                = reinterpret_cast<PacketEventData*>((char*)&packets[i].at(0) + sizeof(PacketEventHeader));
            if (parser == nullptr) {
                parser = HTTPProtocolParser::Create(aggregator, header);
            }
            auto parserRes
                = parser->OnPacket(data->PktType, data->MsgType, header, data->Buffer, data->BufferLen, data->RealLen);
            APSARA_TEST_EQUAL(parserRes, expectRes[i]);
        }
        std::vector<sls_logs::Log> allData;
        google::protobuf::RepeatedPtrField<sls_logs::Log_Content> tags;
        aggregator->FlushLogs(allData, {}, tags, 1);
        APSARA_TEST_TRUE(allData.size() == 1);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "host", "ocs-oneagent-server.alibaba.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "method", "POST"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "url", "/a"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_code", "200"));

        for (size_t i = 0; i < packets.size(); ++i) {
            PacketEventHeader* header = static_cast<PacketEventHeader*>((void*)&packets[i].at(0));
            PacketEventData* data
                = reinterpret_cast<PacketEventData*>((char*)&packets[i].at(0) + sizeof(PacketEventHeader));
            if (parser == nullptr) {
                parser = HTTPProtocolParser::Create(aggregator, header);
            }
            auto parserRes
                = parser->OnPacket(data->PktType, data->MsgType, header, data->Buffer, data->BufferLen, data->RealLen);
            APSARA_TEST_EQUAL(parserRes, expectRes[i]);
        }
        aggregator->FlushLogs(allData, {}, tags, 1);
        APSARA_TEST_TRUE(allData.size() == 2);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "host", "ocs-oneagent-server.alibaba.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "method", "POST"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "url", "/a"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_code", "200"));
    }


    void TestChunkedResponse() {
        std::vector<std::string> rawHexs{

            // req
            "00000000040088665a406acf08004500028e00004000400630701ef065a628705af4fa330050eae1b69266ee936780180804386900"
            "000101080ac2fca775a2f7b3d5474554202f4368756e6b656420485454502f312e310d0a486f73743a20616e676c6573686172702e"
            "617a75726577656273697465732e6e65740d0a436f6e6e656374696f6e3a206b6565702d616c6976650d0a43616368652d436f6e74"
            "726f6c3a206d61782d6167653d300d0a557067726164652d496e7365637572652d52657175657374733a20310d0a557365722d4167"
            "656e743a204d6f7a696c6c612f352e3020284d6163696e746f73683b20496e74656c204d6163204f5320582031305f31355f372920"
            "4170706c655765624b69742f3533372e333620284b48544d4c2c206c696b65204765636b6f29204368726f6d652f3130322e302e30"
            "2e30205361666172692f3533372e33360d0a4163636570743a20746578742f68746d6c2c6170706c69636174696f6e2f7868746d6c"
            "2b786d6c2c6170706c69636174696f6e2f786d6c3b713d302e392c696d6167652f617669662c696d6167652f776562702c696d6167"
            "652f61706e672c2a2f2a3b713d302e382c6170706c69636174696f6e2f7369676e65642d65786368616e67653b763d62333b713d30"
            "2e390d0a526566657265723a2068747470733a2f2f7777772e676f6f676c652e636f6d2f0d0a4163636570742d456e636f64696e67"
            "3a20677a69702c206465666c6174650d0a4163636570742d4c616e67756167653a207a682d434e2c7a683b713d302e390d0a436f6f"
            "6b69653a20415252416666696e6974793d333837346637363566366132663137363766376339336435353237373764376461636636"
            "643365333232376132363066356530623131303935313036383032350d0a0d0a",
            // resp
            "88665a406acf0000000004000800451402ba21d440006406ea5b28705af41ef065a60050fa3366ee9367eae1b8ec80180405da9a00"
            "000101080aa2f7b556c2fca775485454502f312e3120323030204f4b0d0a436f6e74656e742d547970653a20746578742f68746d6c"
            "0d0a446174653a204d6f6e2c203230204a756e20323032322030343a31373a333820474d540d0a5365727665723a204d6963726f73"
            "6f66742d4949532f31302e300d0a43616368652d436f6e74726f6c3a20707269766174650d0a436f6e74656e742d456e636f64696e"
            "673a20677a69700d0a5365742d436f6f6b69653a20415252416666696e6974793d3338373466373635663661326631373637663763"
            "39336435353237373764376461636636643365333232376132363066356530623131303935313036383032353b506174683d2f3b48"
            "7474704f6e6c793b446f6d61696e3d616e676c6573686172702e617a75726577656273697465732e6e65740d0a5472616e73666572"
            "2d456e636f64696e673a206368756e6b65640d0a566172793a204163636570742d456e636f64696e670d0a582d4173704e65744d76"
            "632d56657273696f6e3a20352e300d0a582d4173704e65742d56657273696f6e3a20342e302e33303331390d0a582d506f77657265"
            "642d42793a204153502e4e45540d0a0d0a64340d0a1f8b0800000000000400edbd07601c499625262f6dca7b7f4af54ad7e074a108"
            "80601324d8904010ecc188cde692ec1d69472329ab2a81ca6556655d661640cced9dbcf7de7befbdf7de7befbdf7ba3b9d4e27f7df"
            "ff3f5c6664016cf6ce4adac99e2180aac81f3f7e7c1f3f221effae4fbf3c79f3fbbc3c4de7eda23cfa8d93c7f89996d9f2e2b37cc9"
            "7fe7d90c3f17799ba5d379563779fbd9c7ebf67cfbe0637cde166d991f9dccd7cbb7f92c6deb6cd99ce7759a2fa7d5ac585ea46dde"
            "b48fef4a2b6a7ed7c09b54b3eba3c7f3dd1b5f9def0d0a",
            "88665a406acf00000000040008004514005e21d540006406ecb628705af41ef065a60050fa3366ee95edeae1b8ec80180405e53f00"
            "000101080aa2f7b5c3c2fca77532340d0a1e3d9edf3f7a332f9a94fe0724e4853a6f56d5b2c9d3ecbca5d7767776d24533a617ee1f"
            "0d0a",
            "88665a406acf00000000040008004514008521d640006406ec8e28705af41ef065a60050fa3366ee9617eae1b8ec801804051ac200"
            "000101080aa2f7b9bdc2fca8fd33650d0addee85b4c9a7d572364edfcc73fabdbea40f9b79b52e67e9b26ad3695951db16dfb5759e"
            "2dd2497e5ed5f476590a48824d7f36f9b24ddb0afd9405fd2e180d0a380d0a3cbe2b03bc0b7a1e0d0a",
            "88665a406acf00000000040008004514004321d740006406eccf28705af41ef065a60050fa3366ee9668eae1b8ec80180405565900"
            "000101080aa2f7b9bdc2fca8fd610d0afd3f7a9bfb5d670100000d0a",
            "88665a406acf00000000040008004514003921d840006406ecd828705af41ef065a60050fa3366ee9677eae1b8ec80180405c22e00"
            "000101080aa2f7b9bdc2fca8fd300d0a0d0a",
        };

        std::vector<ParseResult> expectRes = {
            ParseResult_OK,
            ParseResult_OK,
            ParseResult_Partial,
            ParseResult_Partial,
            ParseResult_Partial,
            ParseResult_Partial,
        };
        RawNetPacketReader reader("30.240.101.166", false, ProtocolType_HTTP, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        auto aggregator = new HTTPProtocolEventAggregator(200, 1001);
        static HTTPProtocolParser* parser = nullptr;
        for (size_t i = 0; i < packets.size(); ++i) {
            PacketEventHeader* header = static_cast<PacketEventHeader*>((void*)&packets[i].at(0));
            PacketEventData* data
                = reinterpret_cast<PacketEventData*>((char*)&packets[i].at(0) + sizeof(PacketEventHeader));
            if (parser == nullptr) {
                parser = HTTPProtocolParser::Create(aggregator, header);
            }
            auto parserRes
                = parser->OnPacket(data->PktType, data->MsgType, header, data->Buffer, data->BufferLen, data->RealLen);
            APSARA_TEST_EQUAL(parserRes, expectRes[i]);
        }
        std::vector<sls_logs::Log> allData;
        ::google::protobuf::RepeatedPtrField<sls_logs::Log_Content> tagsContent;
        google::protobuf::RepeatedPtrField<sls_logs::Log_Content> tags;

        aggregator->FlushLogs(allData, {}, tags, 1);
        APSARA_TEST_TRUE(allData.size() == 1);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_domain", "anglesharp.azurewebsites.net"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_type", "GET"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_resource", "/Chunked"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_code", "200"));

        for (size_t i = 0; i < packets.size(); ++i) {
            PacketEventHeader* header = static_cast<PacketEventHeader*>((void*)&packets[i].at(0));
            PacketEventData* data
                = reinterpret_cast<PacketEventData*>((char*)&packets[i].at(0) + sizeof(PacketEventHeader));
            auto parserRes
                = parser->OnPacket(data->PktType, data->MsgType, header, data->Buffer, data->BufferLen, data->RealLen);
            APSARA_TEST_EQUAL(parserRes, expectRes[i]);
        }
        aggregator->FlushLogs(allData, {}, tags, 1);
        APSARA_TEST_TRUE(allData.size() == 2);
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_version", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_domain", "anglesharp.azurewebsites.net"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_type", "GET"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_resource", "/Chunked"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_code", "200"));
    }


    void TestHTTPParserGC() {
        INT64_FLAG(sls_observer_network_process_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_connection_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_process_no_connection_timeout) = 0;
        BOOL_FLAG(sls_observer_network_protocol_stat) = true;

        NetworkStatistic* networkStatistic = NetworkStatistic::GetInstance();
        ProtocolDebugStatistic* protocolDebugStatistic = ProtocolDebugStatistic::GetInstance();
        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
        networkStatistic->Clear();
        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);

        {
            std::vector<std::string> rawHexs{rawHex1};
            RawNetPacketReader reader("30.43.120.83", false, ProtocolType_HTTP, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionCachedSize, 1);

            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionCachedSize, 0);
        }
        networkStatistic->Clear();
        {
            std::vector<std::string> rawHexs{rawHex2};
            RawNetPacketReader reader("30.43.120.83", false, ProtocolType_HTTP, rawHexs);
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
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionNum, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionCachedSize, 1);


            mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
            APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
            APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 1);
            APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionNum, 0);
            APSARA_TEST_EQUAL(protocolDebugStatistic->mHTTPConnectionCachedSize, 0);
        }
    }


    NetworkObserver* mObserver = NetworkObserver::GetInstance();
    const std::string rawHex1 = "00749c945d39a07817a0852e080045000071000040004006a0581e2b7853dcb526fbcd4700506b7401eca5"
                                "91a5ce5018100037ad0000474554202f20485454502f312e310d0a486f73743a2062616964752e636f6d0d"
                                "0a557365722d4167656e743a206375726c2f372e37372e300d0a4163636570743a202a2f2a0d0a0d0a";
    const std::string rawHex2
        = "a07817a0852e00749c945d390800450001598c7240002a0628fedcb526fb1e2b78530050cd47a591a5ce6b74023550180304c2650000"
          "485454502f312e3120323030204f4b0d0a446174653a205468752c203237204a616e20323032322030393a34363a313320474d540d0a"
          "5365727665723a204170616368650d0a4c6173742d4d6f6469666965643a205475652c203132204a616e20323031302031333a34383a"
          "303020474d540d0a455461673a202235312d34376366376536656538343030220d0a4163636570742d52616e6765733a206279746573"
          "0d0a436f6e74656e742d4c656e6774683a2038310d0a43616368652d436f6e74726f6c3a206d61782d6167653d38363430300d0a4578"
          "70697265733a204672692c203238204a616e20323032322030393a34363a313320474d540d0a436f6e6e656374696f6e3a204b656570"
          "2d416c6976650d0a436f6e74656e742d547970653a20746578742f68746d6c0d0a0d0a";
};

APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestCommonRequest, 0);
APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestCommonResponse, 0);
APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestHTTPPacketReaderOrder, 0);
APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestHTTPPacketReaderUnorder, 0);
APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestHTTPParserGC, 0);
APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestCommonRequest2, 0);
// TODO : currently only accept the data starts with HTTP or special METHOD , such as GET, PUT and etc.
// APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestChunkedResponse, 0);
// APSARA_UNIT_TEST_CASE(ProtocolHttpUnittest, TestMoreContentResponse, 0);

} // namespace logtail


int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}