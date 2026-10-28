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

#include "json/json.h"
#include "json/value.h"

#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "common/http/Constant.h"
#include "monitor/Monitor.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "prometheus/Constants.h"
#include "prometheus/PrometheusInputRunner.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class PrometheusInputRunnerUnittest : public testing::Test {
public:
    void OnSuccessfulStartAndStop();
    void TestHasRegisteredPlugins();
    void TestMulitStartAndStop();
    void TestHostOnlyStartAndStop();
    void TestGetAllProjects();
    void TestSendOperatorRequest();
    void TestSendRegisterMessage();
    void TestAgentInfoToJson();
    void TestRefreshAgentInfoSnapshot();

protected:
    void SetUp() override {
        auto runner = PrometheusInputRunner::GetInstance();
        runner->mServiceHost = "127.0.0.1";
        runner->mServicePort = 8080;
        runner->mPodName = "test_pod";
        runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
            response.SetStatusCode(200);
            return true;
        };
    }

    void TearDown() override {
        PrometheusInputRunner::GetInstance()->mHttpRequestSender
            = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
                  response.SetStatusCode(200);
                  return true;
              };
    }

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
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr), defaultLabels, defaultProject);

    PrometheusInputRunner::GetInstance()->Init();
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     != PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());
    APSARA_TEST_EQUAL(PrometheusInputRunner::GetInstance()->mJobNameToProjectNameMap["test_job"], defaultProject);

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
    auto defaultLabels = MetricLabels();
    string defaultProject = "default_project";
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr), defaultLabels, defaultProject);
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

void PrometheusInputRunnerUnittest::TestHostOnlyStartAndStop() {
    string errorMsg;
    string configStr;
    Json::Value config;
    configStr = R"JSON(
    {
        "job_name": "test_job",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s",
        "host_only_mode": true,
        "static_configs": [
            {
                "targets": ["localhost:9090"],
                "labels": {
                    "test_label": "test_value"
                }
            }
        ]
    }
    )JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));

    std::unique_ptr<TargetSubscriberScheduler> scrapeJobPtr = make_unique<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(scrapeJobPtr->Init(config));

    auto defaultLabels = MetricLabels();
    string defaultProject = "default_project";
    // update scrapeJob
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr), defaultLabels, defaultProject);

    PrometheusInputRunner::GetInstance()->Init();
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     != PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());
    APSARA_TEST_EQUAL(PrometheusInputRunner::GetInstance()->mJobNameToProjectNameMap["test_job"], defaultProject);

    // remove
    PrometheusInputRunner::GetInstance()->RemoveScrapeInput("test_job");

    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.find("test_job")
                     == PrometheusInputRunner::GetInstance()->mTargetSubscriberSchedulerMap.end());
    // stop
    PrometheusInputRunner::GetInstance()->Stop();
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
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr1), defaultLabels, defaultProject);

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
    PrometheusInputRunner::GetInstance()->UpdateScrapeInput(std::move(scrapeJobPtr2), defaultLabels, defaultProject);

    // Runner use map to store scrape job, so the order is test_job1, test_job2
    APSARA_TEST_TRUE(PrometheusInputRunner::GetInstance()->GetAllProjects() == "default_project default_project2");
}

void PrometheusInputRunnerUnittest::TestSendOperatorRequest() {
    auto runner = PrometheusInputRunner::GetInstance();
    unique_ptr<HttpRequest> capturedRequest;
    runner->mHttpRequestSender = [&capturedRequest](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        capturedRequest = std::move(request);
        response.SetStatusCode(204);
        return true;
    };

    map<string, string> headers{{prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION,
                                 prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1},
                                {prometheus::ACCEPT, prometheus::APPLICATION_JSON}};
    const string body = R"({"AgentInfo":{"Load":1024}})";
    auto response = runner->SendOperatorRequest(HTTP_POST,
                                                prometheus::HEARTBEAT_COLLECTOR_PATH,
                                                "collector_id=test_pod",
                                                headers,
                                                body,
                                                3);

    APSARA_TEST_EQUAL(204, response.GetStatusCode());
    APSARA_TEST_TRUE(capturedRequest != nullptr);
    APSARA_TEST_EQUAL(HTTP_POST, capturedRequest->mMethod);
    APSARA_TEST_FALSE(capturedRequest->mHTTPSFlag);
    APSARA_TEST_EQUAL("127.0.0.1", capturedRequest->mHost);
    APSARA_TEST_EQUAL(8080, capturedRequest->mPort);
    APSARA_TEST_EQUAL(prometheus::HEARTBEAT_COLLECTOR_PATH, capturedRequest->mUrl);
    APSARA_TEST_EQUAL("collector_id=test_pod", capturedRequest->mQueryString);
    APSARA_TEST_EQUAL(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1,
                      capturedRequest->mHeader.at(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION));
    APSARA_TEST_EQUAL(prometheus::APPLICATION_JSON, capturedRequest->mHeader.at(prometheus::ACCEPT));
    APSARA_TEST_EQUAL(body, capturedRequest->mBody);
    APSARA_TEST_EQUAL(3U, capturedRequest->mTimeout);
}

void PrometheusInputRunnerUnittest::TestSendRegisterMessage() {
    auto runner = PrometheusInputRunner::GetInstance();
    unique_ptr<HttpRequest> capturedRequest;
    runner->mHttpRequestSender = [&capturedRequest](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        capturedRequest = std::move(request);
        response.SetStatusCode(200);
        return true;
    };

    auto response = runner->SendRegisterMessage(prometheus::REGISTER_COLLECTOR_PATH);

    APSARA_TEST_EQUAL(200, response.GetStatusCode());
    APSARA_TEST_TRUE(capturedRequest != nullptr);
    APSARA_TEST_EQUAL(HTTP_GET, capturedRequest->mMethod);
    APSARA_TEST_EQUAL(prometheus::REGISTER_COLLECTOR_PATH, capturedRequest->mUrl);
    APSARA_TEST_EQUAL("pod_name=test_pod", capturedRequest->mQueryString);
    APSARA_TEST_TRUE(capturedRequest->mHeader.empty());
    APSARA_TEST_TRUE(capturedRequest->mBody.empty());
    APSARA_TEST_EQUAL(10U, capturedRequest->mTimeout);
}

void PrometheusInputRunnerUnittest::TestAgentInfoToJson() {
    PrometheusAgentInfo agentInfo;
    agentInfo.mCpuLimit = 2.0;
    agentInfo.mCpuUsage = 0.5;
    agentInfo.mMemLimit = 4096;
    agentInfo.mMemUsage = 1024;
    agentInfo.mLoad = 1048576;
    agentInfo.mScrapeDelaySeconds = 3;
    agentInfo.mHttpSinkInItemsTotal = 100;
    agentInfo.mHttpSinkOutFailed = 2;

    auto value = agentInfo.ToJson();

    APSARA_TEST_EQUAL(2.0, value[prometheus::CPU_LIMIT].asDouble());
    APSARA_TEST_EQUAL(0.5, value[prometheus::CPU_USAGE].asDouble());
    APSARA_TEST_EQUAL(4096, value[prometheus::MEM_LIMIT].asInt64());
    APSARA_TEST_EQUAL(1024, value[prometheus::MEM_USAGE].asDouble());
    APSARA_TEST_EQUAL(1048576UL, value[prometheus::LOAD].asUInt64());
    APSARA_TEST_EQUAL(3UL, value[prometheus::SCRAPE_DELAY_SECONDS].asUInt64());
    APSARA_TEST_EQUAL(100UL, value[prometheus::HTTP_SINK_IN_ITEMS_TOTAL].asUInt64());
    APSARA_TEST_EQUAL(2UL, value[prometheus::HTTP_SINK_OUT_FAILED].asUInt64());
}

void PrometheusInputRunnerUnittest::TestRefreshAgentInfoSnapshot() {
    auto runner = PrometheusInputRunner::GetInstance();
    auto baseline = runner->RefreshAgentInfoSnapshot();

    auto makeSubscriber = [](const string& jobName,
                             const string& interval,
                             uint64_t intervalSeconds,
                             const string& hash,
                             int scrapeSize,
                             int delaySeconds) {
        Json::Value config;
        config[prometheus::JOB_NAME] = jobName;
        config[prometheus::SCRAPE_INTERVAL] = interval;
        config[prometheus::SCRAPE_TIMEOUT] = "10s";
        auto subscriber = make_shared<TargetSubscriberScheduler>();
        APSARA_TEST_TRUE(subscriber->Init(config));

        PromTargetInfo targetInfo;
        targetInfo.mHash = hash;
        auto scheduler = make_shared<ScrapeScheduler>(subscriber->mScrapeConfigPtr,
                                                      "localhost",
                                                      80,
                                                      prometheus::HTTP,
                                                      "/metrics",
                                                      intervalSeconds,
                                                      10,
                                                      0,
                                                      0,
                                                      targetInfo);
        scheduler->mScrapeResponseSizeBytes = scrapeSize;
        scheduler->mExecDelayCount = delaySeconds;
        subscriber->mScrapeSchedulerMap.emplace(hash, scheduler);
        return subscriber;
    };

    auto firstSubscriber = makeSubscriber("__snapshot_job_1", "30s", 30, "target-1", 90, 2);
    auto secondSubscriber = makeSubscriber("__snapshot_job_2", "20s", 20, "target-2", 99, 5);
    {
        WriteLock lock(runner->mSubscriberMapRWLock);
        runner->mTargetSubscriberSchedulerMap[firstSubscriber->GetId()] = firstSubscriber;
        runner->mTargetSubscriberSchedulerMap[secondSubscriber->GetId()] = secondSubscriber;
    }

    SelfMonitorMetricEvent agentMetric;
    LoongCollectorMonitor::GetInstance()->GetAgentMetric(agentMetric);
    SelfMonitorMetricEvent httpSinkMetric;
    LoongCollectorMonitor::GetInstance()->GetRunnerMetric(METRIC_LABEL_VALUE_RUNNER_NAME_HTTP_SINK, httpSinkMetric);

    auto refreshed = runner->RefreshAgentInfoSnapshot();
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetCpuUsageUpLimit(), refreshed.mCpuLimit);
    APSARA_TEST_EQUAL(agentMetric.GetGauge(METRIC_AGENT_CPU), refreshed.mCpuUsage);
    APSARA_TEST_EQUAL(AppConfig::GetInstance()->GetMemUsageUpLimit(), refreshed.mMemLimit);
    APSARA_TEST_EQUAL(agentMetric.GetGauge(METRIC_AGENT_MEMORY), refreshed.mMemUsage);
    APSARA_TEST_EQUAL(baseline.mLoad + 7, refreshed.mLoad);
    APSARA_TEST_EQUAL(7UL, refreshed.mScrapeDelaySeconds);
    APSARA_TEST_EQUAL(httpSinkMetric.GetCounter(METRIC_RUNNER_IN_ITEMS_TOTAL), refreshed.mHttpSinkInItemsTotal);
    APSARA_TEST_EQUAL(httpSinkMetric.GetCounter(METRIC_RUNNER_SINK_OUT_FAILED_ITEMS_TOTAL),
                      refreshed.mHttpSinkOutFailed);

    auto published = runner->GetAgentInfoSnapshot();
    APSARA_TEST_EQUAL(refreshed.mLoad, published.mLoad);
    APSARA_TEST_EQUAL(refreshed.mScrapeDelaySeconds, published.mScrapeDelaySeconds);
    published.mLoad = 0;
    APSARA_TEST_EQUAL(refreshed.mLoad, runner->GetAgentInfoSnapshot().mLoad);

    firstSubscriber->mScrapeSchedulerMap.begin()->second->mExecDelayCount = 4;
    refreshed = runner->RefreshAgentInfoSnapshot();
    APSARA_TEST_EQUAL(4UL, refreshed.mScrapeDelaySeconds);

    runner->RemoveScrapeInput(firstSubscriber->GetId());
    runner->RemoveScrapeInput(secondSubscriber->GetId());
}

UNIT_TEST_CASE(PrometheusInputRunnerUnittest, OnSuccessfulStartAndStop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHasRegisteredPlugins)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestMulitStartAndStop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHostOnlyStartAndStop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestGetAllProjects)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestSendOperatorRequest)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestSendRegisterMessage)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestAgentInfoToJson)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestRefreshAgentInfoSnapshot)

} // namespace logtail

UNIT_TEST_MAIN
