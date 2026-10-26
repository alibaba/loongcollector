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

#if !defined(_MSC_VER)
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstddef>
#include <cstring>
#include <thread>
#endif

#include "common/http/Curl.h"
#include "common/http/HttpRequest.h"
#include "common/http/HttpResponse.h"
#include "unittest/Unittest.h"


using namespace std;

namespace logtail {

#if !defined(_MSC_VER)
namespace {

class UnixSocketTestServer {
public:
    explicit UnixSocketTestServer(string body) : mBody(std::move(body)) {}

    ~UnixSocketTestServer() {
        Join();
        if (mConnectionFd >= 0) {
            close(mConnectionFd);
        }
        if (mListenFd >= 0) {
            close(mListenFd);
        }
        if (!mDirectory.empty()) {
            if (mSocketPath.front() != '@') {
                unlink(mSocketPath.c_str());
            }
            rmdir(mDirectory.c_str());
        }
    }

    bool Start(bool abstractSocket = false) {
        // Keep socket paths below the macOS sun_path limit.
        char directory[] = "/tmp/lc-curl-XXXXXX";
        if (mkdtemp(directory) == nullptr) {
            return false;
        }
        mDirectory = directory;
        mSocketPath = (abstractSocket ? "@" : "") + mDirectory + "/metrics.sock";
        mListenFd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (mListenFd < 0) {
            return false;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        size_t addressLength = sizeof(addr);
        if (abstractSocket) {
            memcpy(addr.sun_path + 1, mSocketPath.c_str() + 1, mSocketPath.size() - 1);
            addressLength = offsetof(sockaddr_un, sun_path) + mSocketPath.size();
        } else {
            memcpy(addr.sun_path, mSocketPath.c_str(), mSocketPath.size() + 1);
        }
        if (::bind(mListenFd, reinterpret_cast<sockaddr*>(&addr), addressLength) != 0 || listen(mListenFd, 1) != 0) {
            return false;
        }
        mThread = thread([this]() {
            pollfd listenPoll{mListenFd, POLLIN, 0};
            if (poll(&listenPoll, 1, 5000) <= 0) {
                return;
            }
            mConnectionFd = accept(mListenFd, nullptr, nullptr);
            if (mConnectionFd < 0) {
                return;
            }
            timeval timeout{5, 0};
            setsockopt(mConnectionFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(mConnectionFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#if defined(SO_NOSIGPIPE)
            int noSigpipe = 1;
            setsockopt(mConnectionFd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(noSigpipe));
#endif
            char buf[4096];
            while (mRequest.find("\r\n\r\n") == string::npos) {
                ssize_t n = read(mConnectionFd, buf, sizeof(buf));
                if (n <= 0) {
                    return;
                }
                mRequest.append(buf, n);
            }
            const string response = "HTTP/1.1 200 OK\r\nContent-Length: " + to_string(mBody.size()) + "\r\n\r\n" + mBody;
            int sendFlags = 0;
#if defined(MSG_NOSIGNAL)
            sendFlags = MSG_NOSIGNAL;
#endif
            for (size_t offset = 0; offset < response.size();) {
                ssize_t n = send(mConnectionFd, response.data() + offset, response.size() - offset, sendFlags);
                if (n <= 0) {
                    return;
                }
                offset += n;
            }
            // Leave the connection alive to exercise curl's shared connection cache.
        });
        return true;
    }

    void Join() {
        if (mThread.joinable()) {
            mThread.join();
        }
    }

    const string& GetSocketPath() const { return mSocketPath; }
    const string& GetRequest() const { return mRequest; }

private:
    string mBody;
    string mDirectory;
    string mSocketPath;
    string mRequest;
    int mListenFd = -1;
    int mConnectionFd = -1;
    thread mThread;
};

class UnixSocketTestRequest : public AsynHttpRequest {
public:
    explicit UnixSocketTestRequest(const string& socketPath)
        : AsynHttpRequest("GET", false, "localhost", 9100, "/metrics", "", {}, "", HttpResponse(), 2, 1) {
        mUnixSocketPath = socketPath;
    }

    bool IsContextValid() const override { return true; }
    void OnSendDone(HttpResponse& response) override {
        mOnDone(response);
    }

    std::function<void(HttpResponse&)> mOnDone;
};

} // namespace
#endif

class CurlUnittest : public ::testing::Test {
public:
    void TestSendHttpRequest();
    void TestCurlTLS();
    void TestFollowRedirect();
#if !defined(_MSC_VER)
    void TestUnixSocket();
    void TestUnixSocketNotExist();
    void TestUnixSocketAsyncIsolation(bool abstractSocket = false);
#if defined(__linux__)
    void TestUnixAbstractSocket();
#endif
#endif
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

#if !defined(_MSC_VER)
void CurlUnittest::TestUnixSocket() {
    UnixSocketTestServer server("metric_a 1\n");
    APSARA_TEST_TRUE_FATAL(server.Start());
    auto request = std::make_unique<HttpRequest>(
        "GET", false, "localhost", 9100, "/custom/metrics", "name=value", map<string, string>(), "", 2, 1);
    request->mUnixSocketPath = server.GetSocketPath();
    HttpResponse res;
    bool success = SendHttpRequest(std::move(request), res);
    server.Join();

    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(200, res.GetStatusCode());
    APSARA_TEST_EQUAL("metric_a 1\n", *res.GetBody<string>());
    APSARA_TEST_TRUE(server.GetRequest().find("GET /custom/metrics?name=value HTTP/1.1\r\n") == 0);
    APSARA_TEST_TRUE(server.GetRequest().find("Host: localhost:9100\r\n") != string::npos);
}

void CurlUnittest::TestUnixSocketNotExist() {
    UnixSocketTestServer server("");
    APSARA_TEST_TRUE_FATAL(server.Start());
    auto request = std::make_unique<HttpRequest>(
        "GET", false, "localhost", 9100, "/metrics", "", map<string, string>(), "", 2, 1);
    request->mUnixSocketPath = server.GetSocketPath() + ".missing";
    HttpResponse res;
    APSARA_TEST_FALSE(SendHttpRequest(std::move(request), res));
    APSARA_TEST_EQUAL(NetworkCode::ConnectionFailed, res.GetNetworkStatus().mCode);
    APSARA_TEST_EQUAL(0, res.GetStatusCode());
}

void CurlUnittest::TestUnixSocketAsyncIsolation(bool abstractSocket) {
    UnixSocketTestServer firstServer("metric_a 1\n");
    UnixSocketTestServer secondServer("metric_a 2\n");
    APSARA_TEST_TRUE_FATAL(firstServer.Start(abstractSocket));
    APSARA_TEST_TRUE_FATAL(secondServer.Start(abstractSocket));
    unique_ptr<CURLM, decltype(&curl_multi_cleanup)> multiCurl(curl_multi_init(), curl_multi_cleanup);
    APSARA_TEST_TRUE_FATAL(multiCurl != nullptr);
    for (const auto* server : {&firstServer, &secondServer}) {
        bool completed = false;
        const string expected = server == &firstServer ? "metric_a 1\n" : "metric_a 2\n";
        auto request = make_unique<UnixSocketTestRequest>(server->GetSocketPath());
        request->mOnDone = [&completed, expected](HttpResponse& response) {
            completed = true;
            APSARA_TEST_EQUAL(NetworkCode::Ok, response.GetNetworkStatus().mCode);
            APSARA_TEST_EQUAL(200, response.GetStatusCode());
            APSARA_TEST_EQUAL(expected, *response.GetBody<string>());
        };
        APSARA_TEST_TRUE_FATAL(AddRequestToMultiCurlHandler(multiCurl.get(), std::move(request)));
        SendAsynRequests(multiCurl.get());
        APSARA_TEST_TRUE(completed);
    }
}
#if defined(__linux__)
void CurlUnittest::TestUnixAbstractSocket() {
    TestUnixSocketAsyncIsolation(true);
}
#endif
#endif

UNIT_TEST_CASE(CurlUnittest, TestSendHttpRequest)
UNIT_TEST_CASE(CurlUnittest, TestCurlTLS)
UNIT_TEST_CASE(CurlUnittest, TestFollowRedirect)
#if !defined(_MSC_VER)
UNIT_TEST_CASE(CurlUnittest, TestUnixSocket)
UNIT_TEST_CASE(CurlUnittest, TestUnixSocketNotExist)
UNIT_TEST_CASE(CurlUnittest, TestUnixSocketAsyncIsolation)
#if defined(__linux__)
UNIT_TEST_CASE(CurlUnittest, TestUnixAbstractSocket)
#endif
#endif

} // namespace logtail

UNIT_TEST_MAIN
