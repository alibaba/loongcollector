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

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    bool IsValidHttpHeader(const std::string& name, const std::string& value) {
        return !name.empty() && name.find_first_of("()<>@,;:\\\"/[]?={} \t") == std::string::npos;
    }
};

void ProtocolParserUnittest::TestParseHttp() {
    const std::string input = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
                              "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    std::string_view buf(input);
    Message result;

    ParseState state = http::ParseRequest(&buf, &result);

    EXPECT_EQ(state, ParseState::kSuccess);
    EXPECT_EQ(result.minor_version, 1);
    EXPECT_EQ(result.req_method, "GET");
    EXPECT_EQ(result.req_path, "/index.html");
    EXPECT_EQ(result.headers_byte_size, input.size());
    EXPECT_EQ(result.body, "");
    EXPECT_EQ(result.body_size, result.body.size());

    // 检查头部信息
    EXPECT_EQ(result.headers.size(), 3);

    const std::string input2 = "GET /path HTTP/1.1\r\nHost: example.com"; // Incomplete header
    std::string_view buf2(input2);
    state = http::ParseRequest(&buf2, &result);
    EXPECT_EQ(state, ParseState::kNeedsMoreData);
}

void ProtocolParserUnittest::TestParseHttpResponse() {
    const std::string input = "HTTP/1.1 200 OK\r\n"
                              "Content-Type: text/html\r\n"
                              "Content-Length: 13\r\n"
                              "\r\n"
                              "Hello, World!";
    std::string_view buf(input);
    Message result;

    ParseState state = http::ParseResponse(&buf, &result, false);

    EXPECT_EQ(state, ParseState::kSuccess);
    EXPECT_EQ(result.minor_version, 1);
    EXPECT_EQ(result.resp_status, 200);
    EXPECT_EQ(result.resp_message, "OK");
    EXPECT_EQ(result.headers.size(), 2);
    EXPECT_EQ(result.body, "Hello, World!");

    // 测试404响应
    const std::string notFound = "HTTP/1.1 404 Not Found\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Content-Length: 9\r\n"
                                 "\r\n"
                                 "Not Found";
    std::string_view buf2(notFound);
    state = http::ParseResponse(&buf2, &result, false);

    EXPECT_EQ(state, ParseState::kSuccess);
    EXPECT_EQ(result.resp_status, 404);
    EXPECT_EQ(result.resp_message, "Not Found");
}

void ProtocolParserUnittest::TestParseHttpHeaders() {
    const std::string input = "GET /test HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "Content-Type: application/json\r\n"
                              "X-Custom-Header: value1, value2\r\n"
                              "Cookie: session=abc123; user=john\r\n"
                              "\r\n";
    std::string_view buf(input);
    Message result;

    ParseState state = http::ParseRequest(&buf, &result);
    EXPECT_EQ(state, ParseState::kSuccess);
    EXPECT_EQ(result.headers.size(), 4);

    // 验证特定头部
    EXPECT_TRUE(result.headers.find("host") != result.headers.end());
    EXPECT_TRUE(result.headers.find("content-type") != result.headers.end());
    EXPECT_TRUE(result.headers.find("x-custom-header") != result.headers.end());
    EXPECT_TRUE(result.headers.find("cookie") != result.headers.end());

    // 验证头部值
    auto host = result.headers.find("host");
    APSARA_TEST_NOT_EQUAL(host, result.headers.end());
    APSARA_TEST_EQUAL(host->second, "example.com");
    auto contentType = result.headers.find("content-type");
    APSARA_TEST_NOT_EQUAL(contentType, result.headers.end());
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
    Message result;

    ParseState state = http::ParseResponse(&buf, &result, false);
    EXPECT_EQ(state, ParseState::kSuccess);

    // 验证分块解码后的完整消息
    std::string expected = "MozillaDeveloperNetwork";
    EXPECT_EQ(result.body, expected);
}

void ProtocolParserUnittest::TestParseInvalidRequests() {
    const std::string invalidMethod = "INVALID /test HTTP/1.1\r\n\r\n";
    std::string_view buf1(invalidMethod);
    Message result;
    ParseState state = http::ParseRequest(&buf1, &result);
    EXPECT_EQ(state, ParseState::kSuccess);

    const std::string invalidVersion = "GET /test HTTP/2.0\r\n\r\n";
    std::string_view buf2(invalidVersion);
    state = http::ParseRequest(&buf2, &result);
    EXPECT_EQ(state, ParseState::kInvalid);

    const std::string invalidHeader = "GET /test HTTP/1.1\r\nInvalid Header\r\n\r\n";
    std::string_view buf3(invalidHeader);
    state = http::ParseRequest(&buf3, &result);
    EXPECT_EQ(state, ParseState::kInvalid);
}

void ProtocolParserUnittest::TestParsePartialRequests() {
    // 测试不完整的请求行
    const std::string partialRequestLine = "GET /test";
    std::string_view buf1(partialRequestLine);
    Message result;
    ParseState state = http::ParseRequest(&buf1, &result);
    EXPECT_EQ(state, ParseState::kNeedsMoreData);

    // 测试不完整的头部
    const std::string partialHeaders = "GET /test HTTP/1.1\r\nHost: example.com\r\n";
    std::string_view buf2(partialHeaders);
    state = http::ParseRequest(&buf2, &result);
    EXPECT_EQ(state, ParseState::kNeedsMoreData);

    // 测试不完整的消息体
    const std::string partialBody = "POST /test HTTP/1.1\r\n"
                                    "Content-Length: 10\r\n"
                                    "\r\n"
                                    "Part";
    std::string_view buf3(partialBody);
    state = http::ParseRequest(&buf3, &result);
    EXPECT_EQ(state, ParseState::kNeedsMoreData);
}

void ProtocolParserUnittest::TestProtocolParserManager() {
    auto& manager = ProtocolParserManager::GetInstance();

    // 测试添加解析器
    EXPECT_TRUE(manager.AddParser(ProtocolType::HTTP));

    // 测试重复添加解析器
    EXPECT_TRUE(manager.AddParser(ProtocolType::HTTP));

    // 测试移除解析器
    EXPECT_TRUE(manager.RemoveParser(ProtocolType::HTTP));

    // 测试移除不存在的解析器
    EXPECT_TRUE(manager.RemoveParser(ProtocolType::HTTP));
}

void ProtocolParserUnittest::TestHttpParserEdgeCases() {
    // 测试空请求
    const std::string emptyRequest = "";
    std::string_view buf1(emptyRequest);
    Message result;
    ParseState state = http::ParseRequest(&buf1, &result);
    EXPECT_EQ(state, ParseState::kNeedsMoreData);

    // 测试超长URL
    std::string longUrl = "GET /";
    longUrl.append(2048, 'a'); // 创建一个超长URL
    longUrl += " HTTP/1.1\r\n\r\n";
    std::string_view buf2(longUrl);
    state = http::ParseRequest(&buf2, &result);
    // EXPECT_EQ(state, ParseState::kError);

    // 测试超多请求头
    std::string manyHeaders = "GET /test HTTP/1.1\r\n";
    for (int i = 0; i < 200; i++) { // 添加大量头部
        manyHeaders += "X-Custom-Header-" + std::to_string(i) + ": value\r\n";
    }
    manyHeaders += "\r\n";
    std::string_view buf3(manyHeaders);
    state = http::ParseRequest(&buf3, &result);
    // EXPECT_EQ(state, ParseState::kError);
}

UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttp);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttpResponse);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttpHeaders);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseChunkedEncoding);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParseInvalidRequests);
UNIT_TEST_CASE(ProtocolParserUnittest, TestParsePartialRequests);
UNIT_TEST_CASE(ProtocolParserUnittest, TestProtocolParserManager);
UNIT_TEST_CASE(ProtocolParserUnittest, TestHttpParserEdgeCases);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
