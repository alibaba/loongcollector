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
#include <json/value.h>

#include "common/JsonUtil.h"
#include "prometheus/PrometheusInputRunner.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PrometheusInputRunnerUnittest : public testing::Test {
public:
    void OnSuccessfulStartAndStop();
    void TestHasRegisteredPlugins();
    void TestMulitStartAndStop();
    void TestGetAllProjects();

protected:
    void SetUp() override {
        prom::PrometheusServer::GetInstance()->mServiceHost = "127.0.0.1";
        prom::PrometheusServer::GetInstance()->mServicePort = 8080;
        prom::PrometheusServer::GetInstance()->mPodName = "test_pod";
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

    auto defaultLabels = MetricLabels();
    string defaultProject = "default_project";
    // update scrapeJob
    prom::PrometheusServer::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr), defaultLabels, defaultProject);

    prom::PrometheusServer::GetInstance()->Init();
    APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     != prom::PrometheusServer::GetInstance()->mTargetSubscriberSchedulerMap.end());
    APSARA_TEST_EQUAL(prom::PrometheusServer::GetInstance()->mJobNameToProjectNameMap["test_job"], defaultProject);

    // remove
    prom::PrometheusServer::GetInstance()->RemoveScrapeInput("test_job");

    APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     == prom::PrometheusServer::GetInstance()->mTargetSubscriberSchedulerMap.end());
    // stop
    prom::PrometheusServer::GetInstance()->Stop();
}

void PrometheusInputRunnerUnittest::TestHasRegisteredPlugins() {
    prom::PrometheusServer::GetInstance()->Init();

    // not in use
    APSARA_TEST_FALSE(prom::PrometheusServer::GetInstance()->HasRegisteredPlugins());

    // in use
    prom::PrometheusServer::GetInstance()->Init();
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
    auto defaultLabels = MetricLabels();
    string defaultProject = "default_project";
    prom::PrometheusServer::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr), defaultLabels, defaultProject);
    APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->HasRegisteredPlugins());
    prom::PrometheusServer::GetInstance()->Stop();
}

void PrometheusInputRunnerUnittest::TestMulitStartAndStop() {
    prom::PrometheusServer::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(prom::PrometheusServer::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->mIsStarted);
    }
    prom::PrometheusServer::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(prom::PrometheusServer::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->mIsStarted);
    }
    prom::PrometheusServer::GetInstance()->Stop();
    {
        std::lock_guard<mutex> lock(prom::PrometheusServer::GetInstance()->mStartMutex);
        APSARA_TEST_FALSE(prom::PrometheusServer::GetInstance()->mIsStarted);
    }
    prom::PrometheusServer::GetInstance()->Init();
    {
        std::lock_guard<mutex> lock(prom::PrometheusServer::GetInstance()->mStartMutex);
        APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->mIsStarted);
    }
    prom::PrometheusServer::GetInstance()->Stop();
    {
        std::lock_guard<mutex> lock(prom::PrometheusServer::GetInstance()->mStartMutex);
        APSARA_TEST_FALSE(prom::PrometheusServer::GetInstance()->mIsStarted);
    }
}

void PrometheusInputRunnerUnittest::TestGetAllProjects() {
    // build scrape job and target
    string errorMsg;
    string configStr;
    Json::Value config;

    // test_job1
    configStr = R"JSON(
    {
        "job_name": "test_job1",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s"
    }
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));

    std::unique_ptr<TargetSubscriberScheduler> scrapeJobPtr1 = make_unique<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(scrapeJobPtr1->Init(config));
    auto defaultLabels = MetricLabels();
    string defaultProject = "default_project";
    // update scrapeJob
    prom::PrometheusServer::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr1), defaultLabels, defaultProject);

    // test_job2
    configStr = R"JSON(
    {
        "job_name": "test_job2",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s"
    }
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    std::unique_ptr<TargetSubscriberScheduler> scrapeJobPtr2 = make_unique<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(scrapeJobPtr2->Init(config));
    defaultProject = "default_project2";
    // update scrapeJob
    prom::PrometheusServer::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr2), defaultLabels, defaultProject);

    // Runner use map to store scrape job, so the order is test_job1, test_job2
    APSARA_TEST_TRUE(prom::PrometheusServer::GetInstance()->GetAllProjects() == "default_project default_project2");
}

UNIT_TEST_CASE(PrometheusInputRunnerUnittest, OnSuccessfulStartAndStop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHasRegisteredPlugins)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestMulitStartAndStop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestGetAllProjects)

} // namespace logtail

UNIT_TEST_MAIN