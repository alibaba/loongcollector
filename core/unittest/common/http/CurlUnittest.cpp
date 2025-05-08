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

#include <fstream>
#include <zlib/zlib.h>
#include <sys/stat.h>


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

// 定义 tar 头部结构（USTAR 格式）
struct TarHeader {
    char name[100];        // 文件名
    char mode[8];          // 权限（八进制）
    char size[12];         // 文件大小（十进制）
    char typeflag;         // 文件类型（0=普通文件，5=目录）
    char linkname[100];    // 链接目标
};


// 辅助函数：递归创建目录（类似 mkdir -p）
int mkdir_p(const char *path, mode_t mode) {
    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return 0;
        }
        return -1;
    }
    char *p = strdup(path);
    char *slash;
    for (slash = p + 1; *slash; slash++) {
        if (*slash == '/') {
            *slash = '\0';
            mkdir(p, mode);
            *slash = '/';
        }
    }
    mkdir(p, mode);
    free(p);
    return 0;
}

bool extractTargz(const std::string& tarGzPath, const std::string& destDir) {
    gzFile gz = gzopen(tarGzPath.c_str(), "rb");
    if (!gz) {
        std::cerr << "Failed to open gzip file: " << tarGzPath << std::endl;
        return false;
    }

    TarHeader header;
    while (gzread(gz, &header, sizeof(TarHeader)) == sizeof(TarHeader)) {
        if (std::memcmp(header.name, "\0", 100) == 0) {
            break;  // 空头表示结束
        }

        std::string filename(header.name);
        filename.erase(filename.find_last_not_of(" \t") + 1);  // 移除空格填充

        // 处理文件类型
        switch (header.typeflag) {
            case '0': {  // 普通文件
                off_t size = std::strtoll(header.size, nullptr, 10);
                std::string full_path = destDir + "/" + filename;
                std::string dir = full_path.substr(0, full_path.find_last_of('/'));
                if (mkdir_p(dir.c_str(), 0755) != 0) {
                    gzclose(gz);
                    return false;
                }

                std::ofstream file(full_path, std::ios::binary);
                if (!file) {
                    std::cerr << "Failed to create file: " << full_path << std::endl;
                    gzclose(gz);
                    return false;
                }

                // 读取并写入文件内容
                std::vector<char> buffer(size);
                gzread(gz, buffer.data(), size);
                file.write(buffer.data(), size);
                break;
            }
            case '5': {  // 目录
                std::string full_path = destDir + "/" + filename;
                mode_t mode = std::strtol(header.mode, nullptr, 8);
                if (mkdir(full_path.c_str(), mode) != 0 && errno != EEXIST) {
                    std::cerr << "Failed to create directory: " << full_path << std::endl;
                    gzclose(gz);
                    return false;
                }
                break;
            }
            default:
                std::cerr << "Unsupported file type:" << header.typeflag << std::endl;
                break;
        }

        // 对齐到 512 字节边界（tar 块大小）
        auto pad = (512 - (gztell(gz) % 512)) % 512;
        gzseek(gz, pad, SEEK_CUR);
    }

    gzclose(gz);
    return true;
}

void CurlUnittest::TestDownload() {
    std::unique_ptr<HttpRequest> request;
    std::string path = "./aliyun-python-agent.tar.gz";
    FILE* file = fopen(path.c_str(), "wb");
    APSARA_TEST_TRUE(file != nullptr);
    HttpResponse res((void*)file, [](void* pf) {
        FILE* file = static_cast<FILE*>(pf);
        fclose(file);
    }, [](char *ptr, size_t size, size_t nmemb, void *stream){
        return fwrite((void*)ptr, size, nmemb, (FILE*)stream);
    });

    request = std::make_unique<HttpRequest>(
        "GET", true, "arms-apm-python.oss-cn-hangzhou.aliyuncs.com/1.3.0-eas/aliyun-python-agent.tar.gz", 443, "", "", map<string, string>(), "", 10, 3, true);
    bool success = SendHttpRequest(std::move(request), res);
    APSARA_TEST_TRUE(success);
    APSARA_TEST_EQUAL(200, res.GetStatusCode());
    // auto *body = res.GetBody<std::string>();
    // APSARA_TEST_GT(body->size(), 0);
    // uncompress ...
    extractTargz(path, "./");


}

UNIT_TEST_CASE(CurlUnittest, TestSendHttpRequest)
UNIT_TEST_CASE(CurlUnittest, TestCurlTLS)
UNIT_TEST_CASE(CurlUnittest, TestFollowRedirect)
UNIT_TEST_CASE(CurlUnittest, TestDownload)


} // namespace logtail

UNIT_TEST_MAIN
