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

#include <sys/stat.h>
#include <zlib/zlib.h>

#include <fstream>

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
    void TestDownload();
};


void CurlUnittest::TestSendHttpRequest() {
    std::unique_ptr<HttpRequest> request;
    HttpResponse res;
    request
        = std::make_unique<HttpRequest>("GET", false, "httpstat.us", 80, "/404", "", map<string, string>(), "", 10, 3);
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

    request = std::make_unique<HttpRequest>(
        "GET", false, "httpstat.us", 80, "/404", "", map<string, string>(), "", 10, 3, true);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(404, res.GetStatusCode());
}

void CurlUnittest::TestDownload() {
    std::unique_ptr<HttpRequest> request;
    std::string path = "./aliyun-python-agent.tar.gz";
    FILE* file = fopen(path.c_str(), "wb");
    APSARA_TEST_TRUE(file != nullptr);
    HttpResponse res((void*)file,
                     [](void* pf) {
                         FILE* file = static_cast<FILE*>(pf);
                         fclose(file);
                     },
                     [](char* ptr, size_t size, size_t nmemb, void* stream) {
                         return fwrite((void*)ptr, size, nmemb, (FILE*)stream);
                     });

    request = std::make_unique<HttpRequest>(
        "GET",
        true,
        "arms-apm-python.oss-cn-hangzhou.aliyuncs.com/1.3.0-eas/aliyun-python-agent.tar.gz",
        443,
        "",
        "",
        map<string, string>(),
        "",
        10,
        3,
        true);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(200, res.GetStatusCode());
    auto headers = res.GetHeader();
    for (const auto& it : headers) {
        LOG_INFO(sLogger, (it.first, it.second));
    }

    // auto *body = res.GetBody<std::string>();
    // APSARA_TEST_GT(body->size(), 0);
    // uncompress ...
    // extractTargz(path, "./");
}

UNIT_TEST_CASE(CurlUnittest, TestSendHttpRequest)
UNIT_TEST_CASE(CurlUnittest, TestCurlTLS)
UNIT_TEST_CASE(CurlUnittest, TestFollowRedirect)
UNIT_TEST_CASE(CurlUnittest, TestDownload)


} // namespace logtail

UNIT_TEST_MAIN
