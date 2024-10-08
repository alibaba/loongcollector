/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <json/json.h>

#include "Common.h"
#include "JsonUtil.h"
#include "json/value.h"
#include "prometheus/PrometheusInputRunner.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PrometheusInputRunnerUnittest : public testing::Test {
public:
    void OnSuccessfulStartAndStop();
    void TestHasRegisteredPlugins();
    void TestMulitStartAndStop();

protected:
    void SetUp() override {
        PrometheusInputRunner::GetInstance()->mServiceHost = "127.0.0.1";
        PrometheusInputRunner::GetInstance()->mServicePort = 8080;
        PrometheusInputRunner::GetInstance()->mPodName = "test_pod";
    }

    void TearDown() override {}

private:
};

void PrometheusInputRunnerUnittest::OnSuccessfulStartAndStop() {
    // build scrape job and target
    string errorMsg;
    string configStr;
    Json::Value config;
    configStr = R"JSON(
    {
        "job_name": "test_job",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s"
    }
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));

    std::unique_ptr<TargetSubscriberScheduler> scrapeJobPtr = make_unique<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(scrapeJobPtr->Init(config));

    // update scrapeJob
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr));

    PrometheusInputRunner::GetInstance()->Init();
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     != PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());


    // remove
    PrometheusInputRunner::GetInstance()->RemoveScrapeInput("test_job");

    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     == PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());
    // stop
    PrometheusInputRunner::GetInstance()->Stop();
}

void PrometheusInputRunnerUnittest::TestHasRegisteredPlugins() {
    PrometheusInputRunner::GetInstance()->Init();

    // not in use
    APSARA_TEST_FALSE(PrometheusInputRunner::GetInstance()->HasRegisteredPlugins());

    // in use
    PrometheusInputRunner::GetInstance()->Init();
    string errorMsg;
    string configStr;
    Json::Value config;
    configStr = R"JSON(
    {
        "job_name": "test_job",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s"
    }
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));

    std::unique_ptr<TargetSubscriberScheduler> scrapeJobPtr = make_unique<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(scrapeJobPtr->Init(config));
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr));
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->HasRegisteredPlugins());
    PrometheusInputRunner::GetInstance()->Stop();
}

void PrometheusInputRunnerUnittest::TestMulitStartAndStop() {
    PrometheusInputRunner::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(PrometheusInputRunner::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mIsStarted);
    }
    PrometheusInputRunner::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(PrometheusInputRunner::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mIsStarted);
    }
    PrometheusInputRunner::GetInstance()->Stop();
    {
        std::lock_guard<mutex> lock(PrometheusInputRunner::GetInstance()->mStartMutex);
        APSARA_TEST_FALSE(PrometheusInputRunner::GetInstance()->mIsStarted);
    }
    PrometheusInputRunner::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(PrometheusInputRunner::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mIsStarted);
    }
    PrometheusInputRunner::GetInstance()->Stop();
    {
        std::lock_guard<mutex> lock(PrometheusInputRunner::GetInstance()->mStartMutex);
        APSARA_TEST_FALSE(PrometheusInputRunner::GetInstance()->mIsStarted);
    }
}

UNIT_TEST_CASE(PrometheusInputRunnerUnittest, OnSuccessfulStartAndStop)
// UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHasRegisteredPlugins)
// UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestMulitStartAndStop)

} // namespace logtail

UNIT_TEST_MAIN