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

#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "unittest/Unittest.h"


using namespace std;

namespace logtail {

class CurlUnittest : public ::testing::Test {
public:
    void TestSendHttpRequest();
    void TestCurlTLS();
    void TestFollowRedirect();
    void TestSkipInterfaceBindForLoopback();
};


void CurlUnittest::TestSendHttpRequest() {
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;

    // OSS 使用 Host 头来识别要访问的存储桶（bucket）。如果 Host 头缺失或不正确，OSS 会返回 403 Forbidden。
    map<string, string> headers;
    headers["Host"] = "loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com";

    request = std::make_unique<HttpRequest>("GET",
                                            false,
                                            "loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com",
                                            80,
                                            "/status/404",
                                            "",
                                            headers,
                                            "",
                                            10,
                                            1);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(404, res.GetStatusCode());
}

void CurlUnittest::TestCurlTLS() {
    // this test should not crash
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;
    CurlTLS tls;
    tls.mInsecureSkipVerify = false;
    tls.mCaFile = "ca.crt";
    tls.mCertFile = "client.crt";
    tls.mKeyFile = "client.key";

    request = std::make_unique<HttpRequest>(
        "GET", true, "example.com", 443, "/path", "", map<string, string>(), "", 10, 3, false, tls);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_FALSE(success);
    APSARA_TEST_EQUAL(0, res.GetStatusCode());
}

void CurlUnittest::TestFollowRedirect() {
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;
    CurlTLS tls;
    tls.mInsecureSkipVerify = false;
    tls.mCaFile = "ca.crt";
    tls.mCertFile = "client.crt";
    tls.mKeyFile = "client.key";

    // OSS 使用 Host 头来识别要访问的存储桶（bucket）。如果 Host 头缺失或不正确，OSS 会返回 403 Forbidden。
    map<string, string> headers;
    headers["Host"] = "loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com";

    request = std::make_unique<HttpRequest>("GET",
                                            false,
                                            "loongcollector-community-edition.oss-cn-shanghai.aliyuncs.com",
                                            80,
                                            "/status/404",
                                            "",
                                            headers,
                                            "",
                                            10,
                                            1,
                                            true);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(404, res.GetStatusCode());
}

void CurlUnittest::TestSkipInterfaceBindForLoopback() {
    // "if!<name>" forces curl to treat the value as an interface name; a nonexistent one
    // makes curl_easy_perform fail with CURLE_INTERFACE_FAILED iff the binding is applied.
    const std::string badIntf = "if!nonexistent-intf-for-ut";
    const std::map<std::string, std::string> emptyHeader;

    auto performAndCheckInterfaceApplied = [&](const std::string& endpoint) -> bool {
        HttpResponse res;
        curl_slist* headers = nullptr;
        CURL* curl = CreateCurlHandler(
            "GET", false, endpoint, 80, "/", "", emptyHeader, "", res, headers, 1, badIntf);
        APSARA_TEST_NOT_EQUAL(nullptr, curl);
        CURLcode code = curl_easy_perform(curl);
        if (headers != nullptr) {
            curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);
        return code == CURLE_INTERFACE_FAILED;
    };

    // loopback endpoints: binding skipped, must not fail with CURLE_INTERFACE_FAILED
    APSARA_TEST_FALSE(performAndCheckInterfaceApplied("127.0.0.1"));
    APSARA_TEST_FALSE(performAndCheckInterfaceApplied("localhost"));
    APSARA_TEST_FALSE(performAndCheckInterfaceApplied("::1"));

    // non-loopback endpoint: binding applied, fails on the nonexistent interface
    APSARA_TEST_TRUE(performAndCheckInterfaceApplied("192.0.2.1"));
}

UNIT_TEST_CASE(CurlUnittest, TestSendHttpRequest)
UNIT_TEST_CASE(CurlUnittest, TestCurlTLS)
UNIT_TEST_CASE(CurlUnittest, TestFollowRedirect)
UNIT_TEST_CASE(CurlUnittest, TestSkipInterfaceBindForLoopback)

} // namespace logtail

UNIT_TEST_MAIN
