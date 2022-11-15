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

#include "network/protocols/utils.h"
#include "observer/network/NetworkObserver.h"
#include "observer/network/ProcessObserver.h"
#include "network/protocols/dns/inner_parser.h"
#include "unittest/Unittest.h"
#include "RawNetPacketReader.h"
#include "unittest/UnittestHelper.h"

namespace logtail {

class ProtocolDnsUnittest : public ::testing::Test {
public:
    // Domain Name System (query)
    //     Transaction ID: 0x8f62
    //     Flags: 0x0100 Standard query
    //     Questions: 1
    //     Answer RRs: 0
    //     Authority RRs: 0
    //     Additional RRs: 0
    //     Queries
    //         sp1.baidu.com: type A, class IN
    void TestCommonRequest() {
        const std::string hexString = "8f62010000010000000000000373703105626169647503636f6d0000010001";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::DNSParser dns((const char*)data.data(), (size_t)data.size());
        dns.parse();
        APSARA_TEST_EQUAL(dns.dnsHeader.txid, 0x8f62);
        APSARA_TEST_EQUAL(dns.dnsHeader.flags, 0x0100);
        APSARA_TEST_EQUAL(dns.dnsHeader.numQueries, 1);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAnswers, 0);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAuth, 0);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAddl, 0);
        APSARA_TEST_EQUAL(dns.packetType, DNSPacketType::DNSPacketRequest);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests.size(), 1);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses.size(), 0);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryHost, "sp1.baidu.com");
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryType, DNSQueryTypeA);
    }

    // Domain Name System (response)
    //      Transaction ID: 0x661b
    //      Flags: 0x8180 Standard query response, No error
    //      Questions: 1
    //      Answer RRs: 3
    //      Authority RRs: 0
    //      Additional RRs: 0
    //      Queries
    //          www.baidu.com: type A, class IN
    //      Answers
    //          www.baidu.com: type CNAME, class IN, cname www.a.shifen.com
    //              Name: www.baidu.com
    //              Type: CNAME (Canonical NAME for an alias) (5)
    //              Class: IN (0x0001)
    //              Time to live: 1130 (18 minutes, 50 seconds)
    //              Data length: 15
    //              CNAME: www.a.shifen.com
    //         www.a.shifen.com: type A, class IN, addr 110.242.68.3
    //              Name: www.a.shifen.com
    //              Type: A (Host Address) (1)
    //              Class: IN (0x0001)
    //              Time to live: 51 (51 seconds)
    //              Data length: 4
    //              Address: 110.242.68.3
    //         www.a.shifen.com: type A, class IN, addr 110.242.68.4
    //              Name: www.a.shifen.com
    //              Type: A (Host Address) (1)
    //              Class: IN (0x0001)
    //              Time to live: 51 (51 seconds)
    //              Data length: 4
    //              Address: 110.242.68.4
    void TestCommonResponse() {
        const std::string hexString
            = "661b818000010003000000000377777705626169647503636f6d0000010001c00c000500010000046a000f037777770161067368"
              "6966656ec016c02b000100010000003300046ef24403c02b000100010000003300046ef24404";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::DNSParser dns((const char*)data.data(), (size_t)data.size());
        dns.parse();
        APSARA_TEST_EQUAL(dns.dnsHeader.txid, 0x661b);
        APSARA_TEST_EQUAL(dns.dnsHeader.flags, 0x8180);
        APSARA_TEST_EQUAL(dns.dnsHeader.numQueries, 1);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAnswers, 3);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAuth, 0);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAddl, 0);
        APSARA_TEST_EQUAL(dns.packetType, DNSPacketType::DNSPacketResponse);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests.size(), 1);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses.size(), 3);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryHost, "www.baidu.com");
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryType, DNSQueryTypeA);
        APSARA_TEST_EQUAL(dns.dnsResponse.answerCount, 3);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].queryType, DNSQueryTypeCNAME);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].queryHost, "www.baidu.com");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].ttlSeconds, 1130);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].value, "www.a.shifen.com");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].queryType, DNSQueryTypeA);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].queryHost, "www.a.shifen.com");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].ttlSeconds, 51);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].value, "110.242.68.3");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].queryType, DNSQueryTypeA);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].queryHost, "www.a.shifen.com");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].ttlSeconds, 51);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].value, "110.242.68.4");
    }

    // Domain Name System (response)
    // Transaction ID: 0x7eb0
    // Flags: 0x8180 Standard query response, No error
    // Questions: 1
    // Answer RRs: 9
    // Authority RRs: 0
    // Additional RRs: 0
    // Queries
    //      teamapp-sf.teamapp.com: type AAAA, class IN
    //         Name: teamapp-sf.teamapp.com
    //          [Name Length: 22]
    //          [Label Count: 3]
    //      Type: AAAA (IPv6 Address) (28)
    //      Class: IN (0x0001)
    // Answers
    //      teamapp-sf.teamapp.com: type CNAME, class IN, cname d363k6ozzqba28.cloudfront.net
    //          Name: teamapp-sf.teamapp.com
    //          Type: CNAME (Canonical NAME for an alias) (5)
    //          Class: IN (0x0001)
    //          Time to live: 194 (3 minutes, 14 seconds)
    //          Data length: 31
    //         CNAME: d363k6ozzqba28.cloudfront.net
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:aa00:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:aa00:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:8200:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:8200:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:5c00:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:5c00:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:ac00:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:ac00:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:a800:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:a800:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:b600:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:b600:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:800:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:800:2:307f:6c00:93a1
    //      d363k6ozzqba28.cloudfront.net: type AAAA, class IN, addr 2600:9000:21c4:b200:2:307f:6c00:93a1
    //          Name: d363k6ozzqba28.cloudfront.net
    //          Type: AAAA (IPv6 Address) (28)
    //          Class: IN (0x0001)
    //          Time to live: 54 (54 seconds)
    //          Data length: 16
    //          AAAA Address: 2600:9000:21c4:b200:2:307f:6c00:93a1
    void TestTypeAAAAResponse() {
        const std::string hexString
            = "7eb0818000010009000000000a7465616d6170702d7366077465616d61707003636f6d00001c0001c00c00050001000000c2001f"
              "0e643336336b366f7a7a71626132380a636c6f756466726f6e74036e657400c034001c00010000003600102600900021c4aa0000"
              "02307f6c0093a1c034001c00010000003600102600900021c482000002307f6c0093a1c034001c00010000003600102600900021"
              "c45c000002307f6c0093a1c034001c00010000003600102600900021c4ac000002307f6c0093a1c034001c000100000036001026"
              "00900021c4a8000002307f6c0093a1c034001c00010000003600102600900021c4b6000002307f6c0093a1c034001c0001000000"
              "3600102600900021c408000002307f6c0093a1c034001c00010000003600102600900021c4b2000002307f6c0093a1";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::DNSParser dns((const char*)data.data(), (size_t)data.size());
        dns.parse();
        APSARA_TEST_EQUAL(dns.dnsHeader.txid, 0x7eb0);
        APSARA_TEST_EQUAL(dns.dnsHeader.flags, 0x8180);
        APSARA_TEST_EQUAL(dns.dnsHeader.numQueries, 1);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAnswers, 9);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAuth, 0);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAddl, 0);
        APSARA_TEST_EQUAL(dns.packetType, DNSPacketType::DNSPacketResponse);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests.size(), 1);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses.size(), 9);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryHost, "teamapp-sf.teamapp.com");
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryType, DNSQueryTypeAAAA);
        APSARA_TEST_EQUAL(dns.dnsResponse.answerCount, 9);

        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].queryType, DNSQueryTypeCNAME);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].queryHost, "teamapp-sf.teamapp.com");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].ttlSeconds, 194);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].value, "d363k6ozzqba28.cloudfront.net");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].queryType, DNSQueryTypeAAAA);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].queryHost, "d363k6ozzqba28.cloudfront.net");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].ttlSeconds, 54);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[1].value, "2600:9000:21c4:aa00:2:307f:6c00:93a1");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].queryType, DNSQueryTypeAAAA);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].queryHost, "d363k6ozzqba28.cloudfront.net");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].ttlSeconds, 54);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[2].value, "2600:9000:21c4:8200:2:307f:6c00:93a1");
    }


    // Domain Name System (response)
    // Transaction ID: 0x588d
    // Flags: 0x8180 Standard query response, No error
    // Questions: 1
    // Answer RRs: 1
    // Authority RRs: 0
    // Additional RRs: 0
    //      Queries
    //          83.74.227.13.in-addr.arpa: type PTR, class IN
    //          Name: 83.74.227.13.in-addr.arpa
    //          [Name Length: 25]
    //          [Label Count: 6]
    //          Type: PTR (domain name PoinTeR) (12)
    //          Class: IN (0x0001)
    // Answers
    //          83.74.227.13.in-addr.arpa: type PTR, class IN, server-13-227-74-83.sfo20.r.cloudfront.net
    //              Name: 83.74.227.13.in-addr.arpa
    //              Type: PTR (domain name PoinTeR) (12)
    //              Class: IN (0x0001)
    //              Time to live: 66720 (18 hours, 32 minutes)
    //              Data length: 44
    //              Domain Name: server-13-227-74-83.sfo20.r.cloudfront.net
    void TestUnknownResponse() {
        const std::string hexString
            = "588d818000010001000000000238330237340332323702313307696e2d61646472046172706100000c0001c00c000c0001000104"
              "a0002c137365727665722d31332d3232372d37342d38330573666f323001720a636c6f756466726f6e74036e657400";
        std::vector<uint8_t> data;
        hexstring_to_bin(hexString, data);
        logtail::DNSParser dns((const char*)data.data(), (size_t)data.size());
        dns.parse();
        APSARA_TEST_EQUAL(dns.dnsHeader.txid, 0x588d);
        APSARA_TEST_EQUAL(dns.dnsHeader.flags, 0x8180);
        APSARA_TEST_EQUAL(dns.dnsHeader.numQueries, 1);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAnswers, 1);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAuth, 0);
        APSARA_TEST_EQUAL(dns.dnsHeader.numAddl, 0);
        APSARA_TEST_EQUAL(dns.packetType, DNSPacketType::DNSPacketResponse);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests.size(), 1);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses.size(), 1);
        APSARA_TEST_EQUAL(dns.dnsRequest.requests[0].queryHost, "83.74.227.13.in-addr.arpa");
        APSARA_TEST_EQUAL(ToString(dns.dnsRequest.requests[0].queryType), "UnKnown");
        APSARA_TEST_EQUAL(dns.dnsResponse.answerCount, 1);
        APSARA_TEST_EQUAL(ToString(dns.dnsResponse.responses[0].queryType), "UnKnown");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].queryHost, "83.74.227.13.in-addr.arpa");
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].ttlSeconds, 66720);
        APSARA_TEST_EQUAL(dns.dnsResponse.responses[0].value, "UnKnown");
    }


    void TestDNSPacketReader() {
        std::vector<std::string> rawHexs{rawHex1, rawHex2, rawHex3, rawHex4};
        RawNetPacketReader reader("30.43.120.83", false, ProtocolType_DNS, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            // std::cout << PacketEventToString(&packet.at(0), packet.size()) << std::endl;
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_resource", "xxxxaawefafadsfasfasf.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_type", "A"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_status", "0"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "count", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::GetLogKey(&allData[0], "latency_ns").first != "0");
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_bytes", "54"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_bytes", "127"));

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_resource", "baidu.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_type", "A"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_status", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "count", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::GetLogKey(&allData[0], "latency_ns").first != "0");
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_bytes", "38"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_bytes", "70"));
    }


    void TestDNSPacketReaderUnorder() {
        std::vector<std::string> rawHexs{rawHex4, rawHex3, rawHex2, rawHex1};
        RawNetPacketReader reader("30.43.120.83", false, ProtocolType_DNS, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            // std::cout << PacketEventToString(&packet.at(0), packet.size()) << std::endl;
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);

        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_resource", "xxxxaawefafadsfasfasf.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_type", "A"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_status", "0"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "latency_ns", "0"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "req_bytes", "54"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[0], "resp_bytes", "127"));


        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_resource", "baidu.com"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_type", "A"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_status", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "count", "1"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "latency_ns", "0"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "req_bytes", "38"));
        APSARA_TEST_TRUE(UnitTestHelper::LogKeyMatched(&allData[1], "resp_bytes", "70"));
    }

    void TestDNSParserGC() {
        INT64_FLAG(sls_observer_network_process_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_connection_timeout) = 18446744073;
        INT64_FLAG(sls_observer_network_process_no_connection_timeout) = 0;
        BOOL_FLAG(sls_observer_network_protocol_stat) = true;
        // clear history
        NetworkStatistic* networkStatistic = NetworkStatistic::GetInstance();
        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
        networkStatistic->Clear();

        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);

        std::vector<std::string> rawHexs{rawHex1, rawHex4};
        RawNetPacketReader reader("30.30.30.30", false, ProtocolType_DNS, rawHexs);
        APSARA_TEST_TRUE(reader.OK());
        std::vector<std::string> packets;
        reader.GetAllNetPackets(packets);
        for (auto& packet : packets) {
            mObserver->OnPacketEvent(&packet.at(0), packet.size());
        }
        std::vector<sls_logs::Log> allData;
        mObserver->FlushOutMetrics(allData);
        APSARA_TEST_EQUAL(allData.size(), 0);

        // GC
        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds() - 60000000000000ULL);
        ProtocolDebugStatistic* statistic = ProtocolDebugStatistic::GetInstance();
        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 1);
        APSARA_TEST_EQUAL(networkStatistic->mGCCount, 1);
        APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 0);
        APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 0);
        APSARA_TEST_EQUAL(statistic->mDNSConnectionNum, 2);
        APSARA_TEST_EQUAL(statistic->mDNSConnectionCachedSize, 2);
        statistic->Clear();

        mObserver->GarbageCollection(GetCurrentTimeInNanoSeconds());
        APSARA_TEST_EQUAL(mObserver->mAllProcesses.size(), 0);
        APSARA_TEST_EQUAL(networkStatistic->mGCCount, 2);
        APSARA_TEST_EQUAL(networkStatistic->mGCReleaseConnCount, 2);
        APSARA_TEST_EQUAL(networkStatistic->mGCReleaseProcessCount, 1);
        APSARA_TEST_EQUAL(statistic->mDNSConnectionNum, 0);
        APSARA_TEST_EQUAL(statistic->mDNSConnectionCachedSize, 0);
    }

    NetworkObserver* mObserver = NetworkObserver::GetInstance();

    const std::string rawHex1 = "00749c945d39a07817a0852e080045000042f72100004011b0cf1e2b78531e1e1e1ef7530035002ea4c43f"
                                "2e0120000100000000000105626169647503636f6d00000100010000291000000000000000";
    const std::string rawHex2 = "a07817a0852e00749c945d3908004500006286f500007a11e6db1e1e1e1e1e2b78530035f753004ec1f83f"
                                "2e8180000100020000000105626169647503636f6d0000010001c00c00010001000001210004dcb52694c0"
                                "0c00010001000001210004dcb526fb0000290fa0000000000000";
    const std::string rawHex3
        = "00749c945d39a07817a0852e080045000052ccbf00004011db211e2b78531e1e1e1ee0800035003e48f33e5701200001000000000001"
          "1578787878616177656661666164736661736661736603636f6d00000100010000291000000000000000";
    const std::string rawHex4 = "a07817a0852e00749c945d3908004500009b09bd00007a1163db1e1e1e1e1e2b78530035e08000874c1e3e"
                                "57818300010000000100011578787878616177656661666164736661736661736603636f6d0000010001c0"
                                "220006000100000384003d01610c67746c642d73657276657273036e657400056e73746c640c7665726973"
                                "69676e2d677273c02261f25b8e000007080000038400093a80000151800000290fa0000000000000";
};

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestCommonRequest, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestCommonResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestTypeAAAAResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestUnknownResponse, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestDNSPacketReader, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestDNSPacketReaderUnorder, 0);

APSARA_UNIT_TEST_CASE(ProtocolDnsUnittest, TestDNSParserGC, 0);
} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}