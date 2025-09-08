// Copyright 2025 iLogtail Authors
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

#include "ebpf/util/Converger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);
DECLARE_FLAG_INT32(ebpf_apm_default_url_threshold);

namespace logtail::ebpf {

class ConvergerUnittest : public testing::Test {
public:
    void BasicFunctionality();
    void RegisterAndDeregister();
    void DoConvergeWithThreshold();
    void DoConvergeNonExistentApp();
    void Benchmark10w();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
    std::shared_ptr<AppDetail> GenerateAppDetail(const std::string& appId) {
        ObserverNetworkOption options;
        options.mApmConfig
            = {.mWorkspace = "test-workspace", .mAppName = "test-app", .mAppId = appId, .mServiceId = appId};
        options.mL4Config.mEnable = true;
        options.mL7Config
            = {.mEnable = true, .mEnableSpan = true, .mEnableMetric = true, .mEnableLog = true, .mSampleRate = 1.0};
        options.mSelectors = {};
        return std::make_shared<AppDetail>(&options, nullptr);
    }
};

void ConvergerUnittest::BasicFunctionality() {
    // 测试用例：阈值为 2
    Converger converger(2);

    // Case 1: 插入两个 URL，不应替换为默认值
    std::string val1 = "url1";
    converger.DoConverge(ConvType::kUrl, val1);
    APSARA_TEST_EQUAL(val1, "url1");

    std::string val2 = "url2";
    converger.DoConverge(ConvType::kUrl, val2);
    APSARA_TEST_EQUAL(val2, "url2");

    // Case 2: 插入第三个 URL，应替换为默认值
    std::string val3 = "url3";
    converger.DoConverge(ConvType::kUrl, val3);
    APSARA_TEST_EQUAL(val3, "{DEFAULT}");

    // Case 3: 插入已存在的 URL，不应替换
    std::string val1Again = "url1";
    converger.DoConverge(ConvType::kUrl, val1Again);
    APSARA_TEST_EQUAL(val1Again, "url1");

    // Case 4: 非 kUrl 类型不应触发替换
    std::string val4 = "url4";
    converger.DoConverge(static_cast<ConvType>(1), val4); // 非 kUrl 类型
    APSARA_TEST_EQUAL(val4, "url4");
}

void ConvergerUnittest::RegisterAndDeregister() {
    AppConvergerManager manager;

    // 注册应用
    auto app1 = GenerateAppDetail("test-1");
    app1->mConfigName = "haha";
    manager.RegisterApp(app1);
    APSARA_TEST_EQUAL(manager.mAppConvergers.size(), 1);

    // 注销应用
    manager.DeregisterApp(app1);
    APSARA_TEST_EQUAL(manager.mAppConvergers.size(), 0);
}

void ConvergerUnittest::DoConvergeWithThreshold() {
    AppConvergerManager manager;

    // 设置阈值为 1
    FLAGS_ebpf_apm_default_url_threshold = 1;

    auto app = GenerateAppDetail("test-1");
    app->mConfigName = "haha";
    manager.RegisterApp(app);

    // 测试 URL 超过阈值后替换为默认值
    std::string url1 = "url1";
    std::string url2 = "url2";

    // 第一次调用，未超过阈值
    manager.DoConverge(app, ConvType::kUrl, url1);
    APSARA_TEST_EQUAL(url1, "url1");

    // 第二次调用，超过阈值，应替换为默认值
    manager.DoConverge(app, ConvType::kUrl, url2);
    APSARA_TEST_EQUAL(url2, "{DEFAULT}");
}

void ConvergerUnittest::DoConvergeNonExistentApp() {
    AppConvergerManager manager;
    auto app = GenerateAppDetail("test-1");
    app->mConfigName = "haha";
    std::string val = "url";
    manager.DoConverge(app, ConvType::kUrl, val);
    APSARA_TEST_EQUAL(val, "url"); // 未注册应用时不做处理
}

std::string GenerateRandomString(size_t length) {
    static const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<> dist(0, chars.size() - 1);
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += chars[dist(generator)];
    }

    return result;
}

void ConvergerUnittest::Benchmark10w() {
    Converger converger(1024);
    std::vector<std::string> urls;
    for (size_t i = 0; i < 100000; i++) {
        urls.push_back(GenerateRandomString(10));
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < urls.size(); ++i) {
        std::string val = urls[i];
        converger.DoConverge(ConvType::kUrl, val);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "[converge] elapsed: " << elapsed.count() << " seconds" << std::endl;
}

UNIT_TEST_CASE(ConvergerUnittest, BasicFunctionality);
UNIT_TEST_CASE(ConvergerUnittest, RegisterAndDeregister);
UNIT_TEST_CASE(ConvergerUnittest, DoConvergeWithThreshold);
UNIT_TEST_CASE(ConvergerUnittest, DoConvergeNonExistentApp);
UNIT_TEST_CASE(ConvergerUnittest, Benchmark10w);

} // namespace logtail::ebpf


UNIT_TEST_MAIN
