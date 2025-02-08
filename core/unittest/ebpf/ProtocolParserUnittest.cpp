#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "ebpf/protocol/http/HttpParser.h"
#include "logger/Logger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class ProtocolParserUnittest : public testing::Test {
public:
    void TestParseHttp();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
};

void ProtocolParserUnittest::TestParseHttp() {
    const std::string input = "GET /index.html HTTP/1.1\r\nHost: www.cmonitor.ai\r\nAccept: image/gif, image/jpeg, "
                              "*/*\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64)\r\n\r\n";
    ;
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

UNIT_TEST_CASE(ProtocolParserUnittest, TestParseHttp);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
