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
#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "ebpf/protocol/ProtocolParser.h"
#include "ebpf/protocol/http/HttpParser.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class ProtocolParserUnittest : public testing::Test {
public:
    void TestParseHttp();
    void TestParseHttpResponse();
    void TestParseHttpHeaders();
    void TestParseChunkedEncoding();
    void TestParseInvalidRequests();
    void TestParsePartialRequests();
    void TestProtocolParserManager();
    void TestHttpParserEdgeCases();

    void RequestBenchmark();
    void RequestWithoutBodyBenchmark();
    void ResponseBenchmark();
    void ResponseWithoutBodyBenchmark();
    void ChunkedResponseBenchmark();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    bool IsValidHttpHeader(const std::string& name, const std::string& value) {
        return !name.empty() && name.find_first_of("()<>@,;:\\\"/[]?={}t") == std::string::npos;
    }
};

void ProtocolParserUnittest::TestParseHttp() {
    const std::string input = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
                              "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string_view buf(input);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);

    ParseState state = http::ParseRequest(buf, result, true);

    APSARA_TEST_EQUAL(state, ParseState::kSuccess);
    APSARA_TEST_EQUAL(result->GetProtocolVersion(), "http1.1");
    APSARA_TEST_EQUAL(result->GetRealPath(), "/index.html");
    APSARA_TEST_EQUAL(result->GetPath(), "/index.html");
    APSARA_TEST_EQUAL(result->GetReqBody(), "");
    APSARA_TEST_EQUAL(result->GetReqBodySize(), 0UL);
    APSARA_TEST_EQUAL(result->GetReqHeaderMap().size(), 3UL);

    // APSARA_TEST_EQUAL(result->GetReqHeaderMap()_byte_size, input.size());
    // APSARA_TEST_EQUAL(result.body, "");
    // APSARA_TEST_EQUAL(result.body_size, result.body.size());

    // // 检查头部信息
    // APSARA_TEST_EQUAL(result->GetReqHeaderMap().size(), 3);

    const std::string input2 = "GET /path HTTP/1.1\r\nHost: example.com"; // Incomplete header
    std::string_view buf2(input2);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf2, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kNeedsMoreData);
}

void ProtocolParserUnittest::TestParseHttpResponse() {
    const std::string input = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 13\r\n"
                              "\r\n"
                              "Hello, World!";
    std::string_view buf(input);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);

    ParseState state = http::ParseResponse(buf, result, false, true);

    APSARA_TEST_EQUAL(state, ParseState::kSuccess);
    APSARA_TEST_EQUAL(result->GetStatusCode(), 200);
    APSARA_TEST_EQUAL(result->GetRespMsg(), "OK");
    APSARA_TEST_EQUAL(result->GetRespHeaderMap().size(), 2UL);
    APSARA_TEST_EQUAL(result->GetRespBody(), "Hello, World!");

    // 测试404响应
    const std::string notFound = "HTTP/1.1 404 Not Found\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 9\r\n"
                                 "\r\n"
                                 "Not Found";
    std::string_view buf2(notFound);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseResponse(buf2, result, false, true);
    APSARA_TEST_EQUAL(state, ParseState::kSuccess);
    APSARA_TEST_EQUAL(result->GetStatusCode(), 404);
    APSARA_TEST_EQUAL(result->GetRespMsg(), "Not Found");
}

void ProtocolParserUnittest::TestParseHttpHeaders() {
    const std::string input = "GET /test HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "Content-Type: application/json\r\n"
                              "X-Custom-Header: value1, value2\r\n"
                              "Cookie: session=abc123; user=john\r\n"
                              "\r\n";
    std::string_view buf(input);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);

    ParseState state = http::ParseRequest(buf, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kSuccess);
    APSARA_TEST_EQUAL(result->GetReqHeaderMap().size(), 4UL);

    // 验证特定头部
    APSARA_TEST_TRUE(result->GetReqHeaderMap().find("host") != result->GetReqHeaderMap().end());
    APSARA_TEST_TRUE(result->GetReqHeaderMap().find("content-type") != result->GetReqHeaderMap().end());
    APSARA_TEST_TRUE(result->GetReqHeaderMap().find("x-custom-header") != result->GetReqHeaderMap().end());
    APSARA_TEST_TRUE(result->GetReqHeaderMap().find("cookie") != result->GetReqHeaderMap().end());

    // 验证头部值
    auto host = result->GetReqHeaderMap().find("host");
    APSARA_TEST_NOT_EQUAL(host, result->GetReqHeaderMap().end());
    APSARA_TEST_EQUAL(host->second, "example.com");
    auto contentType = result->GetReqHeaderMap().find("content-type");
    APSARA_TEST_NOT_EQUAL(contentType, result->GetReqHeaderMap().end());
    APSARA_TEST_EQUAL(contentType->second, "application/json");
}

void ProtocolParserUnittest::TestParseChunkedEncoding() {
    const std::string input = "HTTP/1.1 200 OK\r\n"
                              "Transfer-Encoding: chunked\r\n"
                              "\r\n"
                              "7\r\n"
                              "Mozilla\r\n"
                              "9\r\n"
                              "Developer\r\n"
                              "7\r\n"
                              "Network\r\n"
                              "0\r\n"
                              "\r\n";
    std::string_view buf(input);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);

    ParseState state = http::ParseResponse(buf, result, false, true);
    APSARA_TEST_EQUAL(state, ParseState::kSuccess);

    // 验证分块解码后的完整消息
    std::string expected = "MozillaDeveloperNetwork";
    APSARA_TEST_EQUAL(result->GetRespBody(), expected);
}

void ProtocolParserUnittest::TestParseInvalidRequests() {
    const std::string invalidMethod = "INVALID /test HTTP/1.1\r\n\r\n";
    std::string_view buf1(invalidMethod);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);
    ParseState state = http::ParseRequest(buf1, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kSuccess);

    const std::string invalidVersion = "GET /test HTTP/2.0\r\n\r\n";
    std::string_view buf2(invalidVersion);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf2, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kInvalid);

    const std::string invalidHeader = "GET /test HTTP/1.1\r\nInvalid Header\r\n\r\n";
    std::string_view buf3(invalidHeader);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf3, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kInvalid);
}

void ProtocolParserUnittest::TestParsePartialRequests() {
    // 测试不完整的请求行
    const std::string partialRequestLine = "GET /test";
    std::string_view buf1(partialRequestLine);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);
    ParseState state = http::ParseRequest(buf1, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kNeedsMoreData);

    // 测试不完整的头部
    const std::string partialHeaders = "GET /test HTTP/1.1\r\nHost: example.com\r\n";
    std::string_view buf2(partialHeaders);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf2, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kNeedsMoreData);

    const std::string partialBody = "POST /test HTTP/1.1\r\n"
                                    "Content-Length: 10\r\n"
                                    "\r\n"
                                    "Part";
    std::string_view buf3(partialBody);
    state = http::ParseRequest(buf3, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kNeedsMoreData);
}

void ProtocolParserUnittest::TestProtocolParserManager() {
    auto& manager = ProtocolParserManager::GetInstance();

    APSARA_TEST_TRUE(manager.AddParser(support_proto_e::ProtoHTTP));

    APSARA_TEST_TRUE(manager.AddParser(support_proto_e::ProtoHTTP));

    APSARA_TEST_TRUE(manager.RemoveParser(support_proto_e::ProtoHTTP));

    APSARA_TEST_TRUE(manager.RemoveParser(support_proto_e::ProtoHTTP));
}

void ProtocolParserUnittest::TestHttpParserEdgeCases() {
    // 测试空请求
    const std::string emptyRequest;
    std::string_view buf1(emptyRequest);
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);
    ParseState state = http::ParseRequest(buf1, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kNeedsMoreData);

    std::string longUrl = "GET /";
    longUrl.append(2048, 'a');
    longUrl += " HTTP/1.1\r\n\r\n";
    std::string_view buf2(longUrl);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf2, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kSuccess);

    std::string manyHeaders = "GET /test HTTP/1.1\r\n";
    for (int i = 0; i < 200; i++) {
        manyHeaders += "X-Custom-Header-" + std::to_string(i) + ": value\r\n";
    }
    manyHeaders += "\r\n";
    std::string_view buf3(manyHeaders);
    result = std::make_shared<HttpRecord>(nullptr);
    state = http::ParseRequest(buf3, result, true);
    APSARA_TEST_EQUAL(state, ParseState::kInvalid);
}

const std::string REQ
    = "GET /wp-content/uploads/2010/03/hello-kitty-darth-vader-pink.jpg HTTP/1.1\r\n"
      "Host: www.kittyhell.com\r\n"
      "User-Agent: Mozilla/5.0 (Macintosh; U; Intel Mac OS X 10.6; ja-JP-mac; rv:1.9.2.3) Gecko/20100401 Firefox/3.6.3 "
      "Pathtraq/0.9\r\n"
      "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
      "Accept-Language: ja,en-us;q=0.7,en;q=0.3\r\n"
      "Accept-Encoding: gzip,deflate\r\n"
      "Accept-Charset: Shift_JIS,utf-8;q=0.7,*;q=0.7\r\n"
      "Keep-Alive: 115\r\n"
      "Connection: keep-alive\r\n"
      "Cookie: wp_ozh_wsa_visits=2; wp_ozh_wsa_visit_lasttime=xxxxxxxxxx; "
      "__utma=xxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.xxxxxxxxxx.x; "
      "__utmz=xxxxxxxxx.xxxxxxxxxx.x.x.utmccn=(referral)|utmcsr=reader.livedoor.com|utmcct=/reader/|utmcmd=referral\r\n"
      "\r\n";

const std::string CHUNKED_RESP_MSG = "HTTP/1.1 200 OK\r\n"
                                     "Transfer-Encoding: chunked\r\n"
                                     "\r\n"
                                     "9\r\n"
                                     "pixielabs\r\n"
                                     "C\r\n"
                                     " is awesome!\r\n"
                                     "100\r\n"
                                     "0000000000000000000000000000000000000000000000000000000000000000"
                                     "1111111111111111111111111111111111111111111111111111111111111111"
                                     "2222222222222222222222222222222222222222222222222222222222222222"
                                     "3333333333333333333333333333333333333333333333333333333333333333"
                                     "\r\n"
                                     "0\r\n"
                                     "\r\n";

const std::string RESP_MSG = "HTTP/1.1 200 OK\r\n"
                             "Content-Length: 320\r\n"
                             "\r\n"
                             "0000000000000000000000000000000000000000000000000000000000000000"
                             "1111111111111111111111111111111111111111111111111111111111111111"
                             "2222222222222222222222222222222222222222222222222222222222222222"
                             "3333333333333333333333333333333333333333333333333333333333333333"
                             "4444444444444444444444444444444444444444444444444444444444444444\r\n\r\n";

void ProtocolParserUnittest::RequestBenchmark() {
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000000; i++) {
        std::string_view reqBuf(REQ);
        http::ParseRequest(reqBuf, result, true);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[request] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

void ProtocolParserUnittest::RequestWithoutBodyBenchmark() {
    HTTPRequest result;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000000; i++) {
        std::string_view reqBuf(REQ);
        http::ParseHttpRequest(reqBuf, result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[request] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

void ProtocolParserUnittest::ResponseBenchmark() {
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        std::string_view respBuf(RESP_MSG);
        http::ParseResponse(respBuf, result, false, true);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[response] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

void ProtocolParserUnittest::ChunkedResponseBenchmark() {
    std::shared_ptr<HttpRecord> result = std::make_shared<HttpRecord>(nullptr);
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; i++) {
        std::string_view respBuf(CHUNKED_RESP_MSG);
        http::ParseResponse(respBuf, result, false, true);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[response][chunked] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttp);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttpResponse);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttpHeaders);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseChunkedEncoding);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseInvalidRequests);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParsePartialRequests);
UNIT_TEST_CASE(ProtocolParserUnittest, TestProtocolParserManager);
UNIT_TEST_CASE(ProtocolParserUnittest, TestHttpParserEdgeCases);
UNIT_TEST_CASE(ProtocolParserUnittest, RequestBenchmark);
UNIT_TEST_CASE(ProtocolParserUnittest, RequestWithoutBodyBenchmark);
UNIT_TEST_CASE(ProtocolParserUnittest, ResponseBenchmark);
UNIT_TEST_CASE(ProtocolParserUnittest, ChunkedResponseBenchmark);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
