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

#include <cstdlib>

#include <condition_variable>
#include <future>
#include <thread>
#include <vector>

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
    void TestSendHeartbeatRequest();
    void TestHeartbeatResponses();
    void TestHeartbeatFailureDropsScrapeDelay();
    void TestRegistrationAndHeartbeatLoop();
    void TestStopDuringHeartbeat();
    void TestStoppedTokenRejectsRequests();
    void TestRestartWaitsForOldLoop();
    void TestDestructorWaitsForWorker();
    void TestHeartbeatCadence();

protected:
    void SetUp() override {
        auto runner = PrometheusInputRunner::GetInstance();
        mOriginalAgentInfoSnapshot = runner->GetAgentInfoSnapshot();
        mOriginalUnRegisterMs = runner->mUnRegisterMs.load();
        runner->mSleepFor = [](chrono::steady_clock::duration) { this_thread::sleep_for(chrono::milliseconds(1)); };
        runner->mServiceHost = "127.0.0.1";
        runner->mServicePort = 8080;
        runner->mPodName = "test_pod";
        runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
            response.SetStatusCode(200);
            return true;
        };
    }

    void TearDown() override {
        auto runner = PrometheusInputRunner::GetInstance();
        runner->Stop();
        if (runner->mThreadRes.valid()) {
            runner->mThreadRes.wait();
        }
        runner->CancelAllTargetSubscriber();
        {
            WriteLock lock(runner->mSubscriberMapRWLock);
            runner->mTargetSubscriberSchedulerMap.clear();
        }
        {
            WriteLock lock(runner->mProjectRWLock);
            runner->mJobNameToProjectNameMap.clear();
        }
        runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
            response.SetStatusCode(200);
            return true;
        };
        runner->mSleepFor = nullptr;
        runner->mSteadyNow = nullptr;
        runner->mUnRegisterMs.store(mOriginalUnRegisterMs);
        WriteLock lock(runner->mAgentInfoRWLock);
        runner->mAgentInfoSnapshot = mOriginalAgentInfoSnapshot;
    }

private:
    PrometheusAgentInfo mOriginalAgentInfoSnapshot;
    uint64_t mOriginalUnRegisterMs = 0;
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
    auto runner = PrometheusInputRunner::GetInstance();
    runner->Init();

    // A running heartbeat must not count as a registered plugin, so empty-config Stop still applies.
    APSARA_TEST_TRUE(runner->mThreadRes.wait_for(chrono::seconds(0)) == future_status::timeout);
    APSARA_TEST_FALSE(runner->HasRegisteredPlugins());

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
    auto runner = PrometheusInputRunner::GetInstance();
    runner->Init();
    auto firstToken = runner->mStopToken;
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
    APSARA_TEST_TRUE(firstToken->mStopped.load());
    runner->Init();
    APSARA_TEST_TRUE(firstToken != runner->mStopToken);
    APSARA_TEST_TRUE(firstToken->mStopped.load());
    APSARA_TEST_FALSE(runner->mStopToken->mStopped.load());
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
    auto runner = PrometheusInputRunner::GetInstance();
    runner->mServiceHost.clear();
    atomic_int sendCount = 0;
    runner->mHttpRequestSender = [&sendCount](unique_ptr<HttpRequest>&&, HttpResponse&) {
        ++sendCount;
        return false;
    };
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
    runner->Stop();
    APSARA_TEST_EQUAL(0, sendCount.load());
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

    map<string, string> headers{
        {prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION, prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1},
        {prometheus::ACCEPT, prometheus::APPLICATION_JSON}};
    const string body = R"({"AgentInfo":{"Load":1024}})";
    auto response = runner->SendOperatorRequest(
        HTTP_POST, prometheus::HEARTBEAT_COLLECTOR_PATH, "collector_id=test_pod", headers, body, 3);

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
    APSARA_TEST_EQUAL(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1,
                      capturedRequest->mHeader.at(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION));
    APSARA_TEST_TRUE(capturedRequest->mBody.empty());
    APSARA_TEST_EQUAL(10U, capturedRequest->mTimeout);
    APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)), capturedRequest->mMaxTryCnt);
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

void PrometheusInputRunnerUnittest::TestSendHeartbeatRequest() {
    auto runner = PrometheusInputRunner::GetInstance();
    {
        WriteLock lock(runner->mAgentInfoRWLock);
        runner->mAgentInfoSnapshot.mScrapeDelaySeconds = 999;
    }
    unique_ptr<HttpRequest> capturedRequest;
    int sendCount = 0;
    runner->mHttpRequestSender
        = [&capturedRequest, &sendCount](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
              ++sendCount;
              capturedRequest = std::move(request);
              response.SetStatusCode(204);
              response.AddHeader(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION,
                                 prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1);
              return true;
          };

    auto result = runner->SendHeartbeat();
    APSARA_TEST_EQUAL(PrometheusInputRunner::HeartbeatResult::Success, result);
    APSARA_TEST_EQUAL(1, sendCount);
    APSARA_TEST_EQUAL(HTTP_POST, capturedRequest->mMethod);
    APSARA_TEST_FALSE(capturedRequest->mHTTPSFlag);
    APSARA_TEST_EQUAL("127.0.0.1", capturedRequest->mHost);
    APSARA_TEST_EQUAL(8080, capturedRequest->mPort);
    APSARA_TEST_EQUAL(prometheus::HEARTBEAT_COLLECTOR_PATH, capturedRequest->mUrl);
    APSARA_TEST_EQUAL("collector_id=test_pod", capturedRequest->mQueryString);
    APSARA_TEST_EQUAL(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION_V1,
                      capturedRequest->mHeader.at(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION));
    APSARA_TEST_EQUAL(prometheus::APPLICATION_JSON, capturedRequest->mHeader.at(CONTENT_TYPE));
    APSARA_TEST_EQUAL(prometheus::APPLICATION_JSON, capturedRequest->mHeader.at(prometheus::ACCEPT));
    APSARA_TEST_EQUAL(3U, capturedRequest->mTimeout);
    APSARA_TEST_EQUAL(1U, capturedRequest->mMaxTryCnt);

    Json::Value body;
    string errorMsg;
    APSARA_TEST_TRUE(ParseJsonTable(capturedRequest->mBody, body, errorMsg));
    APSARA_TEST_EQUAL(1U, body.size());
    APSARA_TEST_TRUE(body[prometheus::AGENT_INFO] == runner->GetAgentInfoSnapshot().ToJson());
    APSARA_TEST_EQUAL(8U, body[prometheus::AGENT_INFO].size());
    APSARA_TEST_EQUAL(0UL, body[prometheus::AGENT_INFO][prometheus::SCRAPE_DELAY_SECONDS].asUInt64());
}

void PrometheusInputRunnerUnittest::TestHeartbeatResponses() {
    using Result = PrometheusInputRunner::HeartbeatResult;
    struct TestCase {
        int status;
        string version;
        string body;
        NetworkCode networkCode;
        Result expected;
    };
    const TestCase cases[] = {
        {204, "v1", "", NetworkCode::Ok, Result::Success},
        {204, "", "", NetworkCode::Ok, Result::Success},
        {404, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::NotRegistered},
        {404, "", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {404, "v2", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {404, "v1", "", NetworkCode::Ok, Result::Failed},
        {404, "v1", "invalid JSON", NetworkCode::Ok, Result::Failed},
        {404, "v1", "null", NetworkCode::Ok, Result::Failed},
        {404, "v1", "[]", NetworkCode::Ok, Result::Failed},
        {404, "v1", "42", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"("NotRegistered")", NetworkCode::Ok, Result::Failed},
        {404, "v1", "{}", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"reason":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"msg":"OtherError"})", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"msg":null})", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"msg":404})", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"msg":true})", NetworkCode::Ok, Result::Failed},
        {405, "", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {405, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {400, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {500, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {503, "v1", "", NetworkCode::Ok, Result::Failed},
        {200, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Ok, Result::Failed},
        {0, "", "", NetworkCode::Timeout, Result::Failed},
        {0, "", "", NetworkCode::ConnectionFailed, Result::Failed},
        {0, "", "", NetworkCode::Ok, Result::Failed},
        {404, "v1", R"({"msg":"NotRegistered"})", NetworkCode::Timeout, Result::Failed},
    };
    auto runner = PrometheusInputRunner::GetInstance();
    for (const auto& testCase : cases) {
        SCOPED_TRACE(to_string(testCase.status) + ":" + testCase.version + ":" + testCase.body);
        int sendCount = 0;
        runner->mHttpRequestSender
            = [&testCase, &sendCount](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
                  ++sendCount;
                  APSARA_TEST_EQUAL(prometheus::HEARTBEAT_COLLECTOR_PATH, request->mUrl);
                  response.SetStatusCode(testCase.status);
                  response.SetNetworkStatus(testCase.networkCode, "test network status");
                  *response.GetBody<string>() = testCase.body;
                  if (!testCase.version.empty()) {
                      response.AddHeader("x-prometheus-collector-protocol-version", testCase.version);
                  }
                  return testCase.networkCode == NetworkCode::Ok && testCase.status != 0;
              };
        APSARA_TEST_EQUAL(testCase.expected, runner->SendHeartbeat());
        APSARA_TEST_EQUAL(1, sendCount);
    }
}

void PrometheusInputRunnerUnittest::TestHeartbeatFailureDropsScrapeDelay() {
    auto runner = PrometheusInputRunner::GetInstance();
    Json::Value config;
    config[prometheus::JOB_NAME] = "__heartbeat_delay_job";
    config[prometheus::SCRAPE_INTERVAL] = "30s";
    config[prometheus::SCRAPE_TIMEOUT] = "10s";
    auto subscriber = make_shared<TargetSubscriberScheduler>();
    APSARA_TEST_TRUE(subscriber->Init(config));
    PromTargetInfo targetInfo;
    targetInfo.mHash = "heartbeat-target";
    auto scheduler = make_shared<ScrapeScheduler>(
        subscriber->mScrapeConfigPtr, "localhost", 80, prometheus::HTTP, "/metrics", 30, 10, 0, 0, targetInfo);
    scheduler->mScrapeResponseSizeBytes = 90;
    scheduler->mExecDelayCount = 7;
    subscriber->mScrapeSchedulerMap.emplace(targetInfo.mHash, scheduler);
    {
        WriteLock lock(runner->mSubscriberMapRWLock);
        runner->mTargetSubscriberSchedulerMap[subscriber->GetId()] = subscriber;
    }

    runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
        response.SetNetworkStatus(NetworkCode::Timeout, "heartbeat timeout");
        return false;
    };
    APSARA_TEST_EQUAL(PrometheusInputRunner::HeartbeatResult::Failed, runner->SendHeartbeat());
    APSARA_TEST_EQUAL(0, scheduler->mExecDelayCount.load());
    APSARA_TEST_EQUAL(7UL, runner->GetAgentInfoSnapshot().mScrapeDelaySeconds);

    scheduler->mExecDelayCount = 2;
    runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        Json::Value body;
        string errorMsg;
        APSARA_TEST_TRUE(ParseJsonTable(request->mBody, body, errorMsg));
        APSARA_TEST_EQUAL(2UL, body[prometheus::AGENT_INFO][prometheus::SCRAPE_DELAY_SECONDS].asUInt64());
        response.SetStatusCode(204);
        return true;
    };
    APSARA_TEST_EQUAL(PrometheusInputRunner::HeartbeatResult::Success, runner->SendHeartbeat());
    APSARA_TEST_EQUAL(2UL, runner->GetAgentInfoSnapshot().mScrapeDelaySeconds);
    runner->RemoveScrapeInput(subscriber->GetId());
}

void PrometheusInputRunnerUnittest::TestRegistrationAndHeartbeatLoop() {
    auto runner = PrometheusInputRunner::GetInstance();
    auto stop = make_shared<PrometheusInputRunner::StopToken>();
    auto subscriber = make_shared<TargetSubscriberScheduler>();
    Json::Value config;
    config[prometheus::JOB_NAME] = "__loop_job";
    APSARA_TEST_TRUE(subscriber->Init(config));
    {
        WriteLock lock(runner->mSubscriberMapRWLock);
        runner->mTargetSubscriberSchedulerMap[subscriber->GetId()] = subscriber;
    }

    int registerCount = 0;
    int heartbeatCount = 0;
    vector<string> requests;
    vector<chrono::steady_clock::duration> waits;
    auto now = chrono::steady_clock::time_point{};
    runner->mSteadyNow = [&]() { return now; };
    shared_ptr<PromFuture<HttpResponse&, uint64_t>> initialSubscription;
    runner->mHttpRequestSender = [&](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        requests.push_back(request->mUrl);
        if (request->mUrl == prometheus::REGISTER_COLLECTOR_PATH) {
            APSARA_TEST_EQUAL(1U, request->mMaxTryCnt);
            ++registerCount;
            response.SetStatusCode(registerCount == 1 || registerCount == 3 ? 500 : 200);
            *response.GetBody<string>()
                = registerCount == 2 ? R"({"unRegisterMs":"12000"})" : R"({"unRegisterMs":"99000"})";
        } else {
            APSARA_TEST_EQUAL(prometheus::HEARTBEAT_COLLECTOR_PATH, request->mUrl);
            ++heartbeatCount;
            if (heartbeatCount == 1) {
                initialSubscription = subscriber->mFuture;
                APSARA_TEST_TRUE(initialSubscription != nullptr);
                response.SetStatusCode(404);
                response.AddHeader(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION, "v1");
                *response.GetBody<string>() = R"({"msg":"NotRegistered"})";
            } else {
                APSARA_TEST_TRUE(initialSubscription == subscriber->mFuture);
                response.SetStatusCode(heartbeatCount == 2 ? 204 : 503);
            }
        }
        return true;
    };
    runner->mSleepFor = [&](chrono::steady_clock::duration interval) {
        waits.push_back(interval);
        now += interval;
        if (waits.size() == 4) {
            stop->mStopped.store(true);
        }
    };
    runner->RunRegistrationAndHeartbeat(stop);

    APSARA_TEST_EQUAL(4, registerCount);
    APSARA_TEST_EQUAL(3, heartbeatCount);
    APSARA_TEST_EQUAL(11000UL, runner->mUnRegisterMs.load());
    const vector<string> expectedRequests{prometheus::REGISTER_COLLECTOR_PATH,
                                          prometheus::REGISTER_COLLECTOR_PATH,
                                          prometheus::HEARTBEAT_COLLECTOR_PATH,
                                          prometheus::REGISTER_COLLECTOR_PATH,
                                          prometheus::REGISTER_COLLECTOR_PATH,
                                          prometheus::HEARTBEAT_COLLECTOR_PATH,
                                          prometheus::HEARTBEAT_COLLECTOR_PATH};
    APSARA_TEST_TRUE(requests == expectedRequests);
    const vector<chrono::steady_clock::duration> expectedWaits{chrono::seconds(1),
                                                               chrono::seconds(1),
                                                               chrono::seconds(prometheus::RefeshIntervalSeconds),
                                                               chrono::seconds(prometheus::RefeshIntervalSeconds)};
    APSARA_TEST_TRUE(waits == expectedWaits);
    runner->RemoveScrapeInput(subscriber->GetId());
}

void PrometheusInputRunnerUnittest::TestStopDuringHeartbeat() {
    auto runner = PrometheusInputRunner::GetInstance();
    mutex stateMutex;
    condition_variable cv;
    bool heartbeatStarted = false;
    bool releaseHeartbeat = false;
    bool unregistered = false;
    int registerCount = 0;
    int heartbeatCount = 0;
    runner->mHttpRequestSender = [&](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        unique_lock<mutex> lock(stateMutex);
        if (request->mUrl == prometheus::REGISTER_COLLECTOR_PATH) {
            ++registerCount;
            response.SetStatusCode(200);
        } else if (request->mUrl == prometheus::HEARTBEAT_COLLECTOR_PATH) {
            ++heartbeatCount;
            heartbeatStarted = true;
            cv.notify_all();
            cv.wait(lock, [&]() { return releaseHeartbeat; });
            response.SetStatusCode(404);
            response.AddHeader(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION, "v1");
            *response.GetBody<string>() = R"({"msg":"NotRegistered"})";
        } else {
            APSARA_TEST_EQUAL(prometheus::UNREGISTER_COLLECTOR_PATH, request->mUrl);
            APSARA_TEST_EQUAL("v1", request->mHeader.at(prometheus::PROMETHEUS_COLLECTOR_PROTOCOL_VERSION));
            APSARA_TEST_EQUAL(static_cast<uint32_t>(INT32_FLAG(default_http_request_max_try_cnt)), request->mMaxTryCnt);
            unregistered = true;
            response.SetStatusCode(200);
            cv.notify_all();
        }
        return true;
    };
    runner->Init();
    {
        unique_lock<mutex> lock(stateMutex);
        APSARA_TEST_TRUE(cv.wait_for(lock, chrono::seconds(2), [&]() { return heartbeatStarted; }));
    }
    auto stopping = async(launch::async, [runner]() { runner->Stop(); });
    bool unregisterBeforeHeartbeatCompletion;
    {
        unique_lock<mutex> lock(stateMutex);
        unregisterBeforeHeartbeatCompletion = cv.wait_for(lock, chrono::seconds(2), [&]() { return unregistered; });
        releaseHeartbeat = true;
    }
    cv.notify_all();
    stopping.wait();
    runner->mThreadRes.wait();
    APSARA_TEST_TRUE(unregisterBeforeHeartbeatCompletion);
    APSARA_TEST_TRUE(runner->mStopToken->mStopped.load());
    APSARA_TEST_EQUAL(1, registerCount);
    APSARA_TEST_EQUAL(1, heartbeatCount);
}

void PrometheusInputRunnerUnittest::TestStoppedTokenRejectsRequests() {
    auto runner = PrometheusInputRunner::GetInstance();
    auto stop = make_shared<PrometheusInputRunner::StopToken>();
    stop->mStopped.store(true);
    int sendCount = 0;
    runner->mHttpRequestSender = [&](unique_ptr<HttpRequest>&&, HttpResponse&) {
        ++sendCount;
        return true;
    };
    auto response = runner->SendRegisterMessage(prometheus::REGISTER_COLLECTOR_PATH, stop);
    APSARA_TEST_EQUAL(0, response.GetStatusCode());
    APSARA_TEST_EQUAL(PrometheusInputRunner::HeartbeatResult::Failed, runner->SendHeartbeat(stop));
    runner->RunRegistrationAndHeartbeat(stop);
    APSARA_TEST_EQUAL(0, sendCount);
}

void PrometheusInputRunnerUnittest::TestRestartWaitsForOldLoop() {
    auto runner = PrometheusInputRunner::GetInstance();
    mutex stateMutex;
    condition_variable cv;
    bool oldLoopSleeping = false;
    bool releaseOldLoop = false;
    int registerCount = 0;
    runner->mHttpRequestSender = [&](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        lock_guard<mutex> lock(stateMutex);
        if (request->mUrl == prometheus::REGISTER_COLLECTOR_PATH) {
            ++registerCount;
            cv.notify_all();
        }
        response.SetStatusCode(request->mUrl == prometheus::HEARTBEAT_COLLECTOR_PATH ? 204 : 200);
        return true;
    };
    runner->mSleepFor = [&](chrono::steady_clock::duration) {
        unique_lock<mutex> lock(stateMutex);
        if (!oldLoopSleeping) {
            oldLoopSleeping = true;
            cv.notify_all();
            cv.wait(lock, [&]() { return releaseOldLoop; });
        }
        lock.unlock();
        this_thread::sleep_for(chrono::milliseconds(1));
    };

    runner->Init();
    {
        unique_lock<mutex> lock(stateMutex);
        APSARA_TEST_TRUE(cv.wait_for(lock, chrono::seconds(2), [&]() { return oldLoopSleeping; }));
    }
    auto oldToken = runner->mStopToken;
    runner->Stop();
    APSARA_TEST_TRUE(oldToken->mStopped.load());
    auto restarting = async(launch::async, [runner]() { runner->Init(); });
    APSARA_TEST_TRUE(restarting.wait_for(chrono::milliseconds(20)) == future_status::timeout);
    {
        lock_guard<mutex> lock(stateMutex);
        releaseOldLoop = true;
    }
    cv.notify_all();
    APSARA_TEST_TRUE(restarting.wait_for(chrono::seconds(2)) == future_status::ready);
    restarting.wait();
    APSARA_TEST_TRUE(oldToken != runner->mStopToken);
    APSARA_TEST_TRUE(oldToken->mStopped.load());
    {
        unique_lock<mutex> lock(stateMutex);
        APSARA_TEST_TRUE(cv.wait_for(lock, chrono::seconds(2), [&]() { return registerCount == 2; }));
    }
    runner->Stop();
    runner->mThreadRes.wait();
}

void PrometheusInputRunnerUnittest::TestHeartbeatCadence() {
    auto runner = PrometheusInputRunner::GetInstance();
    auto stop = make_shared<PrometheusInputRunner::StopToken>();
    auto now = chrono::steady_clock::time_point{};
    runner->mSteadyNow = [&]() { return now; };
    const vector<chrono::milliseconds> requestDurations{chrono::milliseconds(3000),
                                                        chrono::milliseconds(500),
                                                        chrono::milliseconds(12000),
                                                        chrono::milliseconds(5000),
                                                        chrono::milliseconds(3000)};
    vector<chrono::steady_clock::duration> starts;
    vector<chrono::steady_clock::duration> waits;
    runner->mHttpRequestSender = [&](unique_ptr<HttpRequest>&& request, HttpResponse& response) {
        if (request->mUrl == prometheus::REGISTER_COLLECTOR_PATH) {
            response.SetStatusCode(200);
            return true;
        }
        starts.push_back(now.time_since_epoch());
        now += requestDurations.at(starts.size() - 1);
        response.SetStatusCode(starts.size() == 2 ? 503 : 204);
        return true;
    };
    runner->mSleepFor = [&](chrono::steady_clock::duration duration) {
        waits.push_back(duration);
        now += duration;
        if (waits.size() == requestDurations.size()) {
            stop->mStopped.store(true);
        }
    };
    runner->RunRegistrationAndHeartbeat(stop);

    const vector<chrono::steady_clock::duration> expectedStarts{
        chrono::seconds(0), chrono::seconds(5), chrono::seconds(10), chrono::seconds(25), chrono::seconds(30)};
    const vector<chrono::steady_clock::duration> expectedWaits{
        chrono::seconds(2), chrono::milliseconds(4500), chrono::seconds(3), chrono::seconds(0), chrono::seconds(2)};
    APSARA_TEST_TRUE(starts == expectedStarts);
    APSARA_TEST_TRUE(waits == expectedWaits);
}

void PrometheusInputRunnerUnittest::TestDestructorWaitsForWorker() {
#if GTEST_HAS_DEATH_TEST
    for (bool alreadyStopped : {false, true}) {
        EXPECT_EXIT(
            {
                auto runner = new PrometheusInputRunner();
                auto stop = make_shared<PrometheusInputRunner::StopToken>();
                runner->mStopToken = stop;
                runner->mAgentInfoSnapshot.mLoad = 42;
                runner->mHttpRequestSender = [](unique_ptr<HttpRequest>&&, HttpResponse& response) {
                    response.SetStatusCode(204);
                    return true;
                };
                promise<void> workerStarted;
                atomic_bool workerFinished = false;
                runner->mThreadRes = async(launch::async, [runner, stop, &workerStarted, &workerFinished]() {
                    workerStarted.set_value();
                    auto deadline = chrono::steady_clock::now() + chrono::seconds(2);
                    while (!stop->mStopped.load()) {
                        if (chrono::steady_clock::now() >= deadline) {
                            std::_Exit(2);
                        }
                        this_thread::sleep_for(chrono::milliseconds(1));
                    }
                    if (runner->GetAgentInfoSnapshot().mLoad != 42) {
                        std::_Exit(3);
                    }
                    HttpResponse response;
                    if (!runner->mHttpRequestSender(nullptr, response) || response.GetStatusCode() != 204) {
                        std::_Exit(4);
                    }
                    workerFinished.store(true);
                });
                workerStarted.get_future().wait();
                if (alreadyStopped) {
                    stop->mStopped.store(true);
                }
                delete runner;
                std::_Exit(workerFinished.load() ? 0 : 1);
            },
            ::testing::ExitedWithCode(0),
            "");
    }
#else
    GTEST_SKIP() << "Subprocess tests are not supported on this platform";
#endif
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
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestSendHeartbeatRequest)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHeartbeatResponses)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHeartbeatFailureDropsScrapeDelay)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestRegistrationAndHeartbeatLoop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestStopDuringHeartbeat)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestStoppedTokenRejectsRequests)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestRestartWaitsForOldLoop)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestDestructorWaitsForWorker)
UNIT_TEST_CASE(PrometheusInputRunnerUnittest, TestHeartbeatCadence)

} // namespace logtail

UNIT_TEST_MAIN
