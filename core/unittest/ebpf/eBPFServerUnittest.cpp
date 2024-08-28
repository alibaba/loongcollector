#include <json/json.h>
#include <iostream>
#include <random>
#include <algorithm>

#include "common/FileSystemUtil.h"
#include "unittest/Unittest.h"
#include "ebpf/include/export.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PipelineContext.h"
#include "ebpf/eBPFServer.h"
#include "ebpf/SourceManager.h"
#include "logger/Logger.h"
#include "ebpf/config.h"
#include "app_config/AppConfig.h"
#include "common/JsonUtil.h"
#include "ebpf/config.h"

namespace logtail {
namespace ebpf {
class eBPFServerUnittest : public testing::Test {
public:
    eBPFServerUnittest() {
        ebpf::eBPFServer::GetInstance()->Init();
    }
    void TestInit();

    void TestEnableNetworkPlugin();

    void TestEnableNetworkSecurePlugin();

    void TestEnableProcessPlugin();

    void TestEnableFileSecurePlugin();

    void TestUpdatePlugin();

    void TestLoadEbpfParametersV1();
    void TestLoadEbpfParametersV2();

    void TestDefaultAndLoadEbpfParameters();

    void TestDefaultEbpfParameters();

    void TestEbpfParameters();

protected:
    void SetUp() override {
        config_ = new eBPFAdminConfig;
        config_->mReceiveEventChanCap = 4096;
        config_->mAdminConfig.mDebugMode = false;
        config_->mAdminConfig.mLogLevel = "warn";
        config_->mAdminConfig.mPushAllSpan = false;
        config_->mAggregationConfig.mAggWindowSecond = 15;
        config_->mConverageConfig.mStrategy = "combine";
        config_->mSampleConfig.mStrategy = "fixedRate";
        config_->mSampleConfig.mConfig.mRate = 0.01;
        config_->mSocketProbeConfig.mSlowRequestThresholdMs = 500;
        config_->mSocketProbeConfig.mMaxConnTrackers = 10000;
        config_->mSocketProbeConfig.mMaxBandWidthMbPerSec = 30;
        config_->mSocketProbeConfig.mMaxRawRecordPerSec = 100000;
        config_->mProfileProbeConfig.mProfileSampleRate = 10;
        config_->mProfileProbeConfig.mProfileUploadDuration = 10;
        config_->mProcessProbeConfig.mEnableOOMDetect = false;
    }
private:
    template <typename T>
    void setJSON(Json::Value& v, const std::string& key, const T& value) {
        v[key] = value;
    }
    void InitSecurityOpts();
    void InitObserverOpts();
    void GenerateBatchMeasure(nami::NamiHandleBatchMeasureFunc cb);
    void GenerateBatchSpan(nami::NamiHandleBatchSpanFunc cb);
    void GenerateBatchEvent(nami::NamiHandleBatchDataEventFn cb, SecureEventType);
    void GenerateBatchAppEvent(nami::NamiHandleBatchEventFunc cb);
    void writeLogtailConfigJSON(const Json::Value& v) {
        LOG_INFO(sLogger, ("writeLogtailConfigJSON", v.toStyledString()));
        OverwriteFile(STRING_FLAG(ilogtail_config), v.toStyledString());
    }
    eBPFAdminConfig* config_;
    Pipeline p;
    PipelineContext ctx;
    SecurityOptions security_opts;
};

static int generateRandomInt(int bound) {
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<> dist(0, bound);
  return dist(generator);
}


void eBPFServerUnittest::TestLoadEbpfParametersV1() {
    Json::Value value;
    std::string configStr, errorMsg;
    // valid optional param, include all appconfig params
    configStr = R"(
        {
            "ebpf": {
                "receive_event_chan_cap": 1024,
                "admin_config": {
                    "debug_mode": true,
                    "log_level": "error",
                    "push_all_span": true
                },
                "aggregation_config": {
                    "agg_window_second": 8
                },
                "converage_config": {
                    "strategy": "combine1"
                },
                "sample_config": {
                    "strategy": "fixedRate1",
                    "config": {
                        "rate": 0.001
                    }
                },
                "socket_probe_config": {
                    "slow_request_threshold_ms": 5000,
                    "max_conn_trackers": 100000,
                    "max_band_width_mb_per_sec": 300,
                    "max_raw_record_per_sec": 1000000
                },
                "profile_probe_config": {
                    "profile_sample_rate": 100,
                    "profile_upload_duration": 100
                },
                "process_probe_config": {
                    "enable_oom_detect": true
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);

    AppConfig* app_config = AppConfig::GetInstance();
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    auto configjson = app_config->GetConfig();
    config_->LoadEbpfConfig(configjson);
    APSARA_TEST_EQUAL(config_->GetReceiveEventChanCap(), 1024);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mDebugMode, true);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mLogLevel, "error");
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mPushAllSpan, true);
    APSARA_TEST_EQUAL(config_->GetAggregationConfig().mAggWindowSecond, 8);
    APSARA_TEST_EQUAL(config_->GetConverageConfig().mStrategy, "combine1");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mStrategy, "fixedRate1");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mConfig.mRate, 0.001);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mSlowRequestThresholdMs, 5000);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxConnTrackers, 100000);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxBandWidthMbPerSec, 300);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxRawRecordPerSec, 1000000);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileSampleRate, 100);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileUploadDuration, 100);
    APSARA_TEST_EQUAL(config_->GetProcessProbeConfig().mEnableOOMDetect, true);
}

void eBPFServerUnittest::TestDefaultAndLoadEbpfParameters() {
    Json::Value value;
    std::string configStr, errorMsg;
    // valid optional param
    configStr = R"(
        {
            "ebpf": {
                "receive_event_chan_cap": 1024,
                "sample_config": {
                    "strategy": "fixedRate1",
                    "config": {
                        "rate": 0.001
                    }
                },
                "socket_probe_config": {
                    "slow_request_threshold_ms": 5000,
                    "max_conn_trackers": 100000,
                    "max_band_width_mb_per_sec": 300,
                    "max_raw_record_per_sec": 1000000
                },
                "profile_probe_config": {
                    "profile_sample_rate": 100,
                    "profile_upload_duration": 100
                },
                "process_probe_config": {
                    "enable_oom_detect": true
                }
            }
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, value, errorMsg));
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);

    AppConfig* app_config = AppConfig::GetInstance();
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    auto configjson = app_config->GetConfig();
    config_->LoadEbpfConfig(configjson);
    APSARA_TEST_EQUAL(config_->GetReceiveEventChanCap(), 1024);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mDebugMode, false);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mLogLevel, "warn");
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mPushAllSpan, false);
    APSARA_TEST_EQUAL(config_->GetAggregationConfig().mAggWindowSecond, 15);
    APSARA_TEST_EQUAL(config_->GetConverageConfig().mStrategy, "combine");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mStrategy, "fixedRate1");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mConfig.mRate, 0.001);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mSlowRequestThresholdMs, 5000);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxConnTrackers, 100000);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxBandWidthMbPerSec, 300);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxRawRecordPerSec, 1000000);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileSampleRate, 100);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileUploadDuration, 100);
    APSARA_TEST_EQUAL(config_->GetProcessProbeConfig().mEnableOOMDetect, true);
}

void eBPFServerUnittest::TestDefaultEbpfParameters() {
    Json::Value value;
    value["logtail_sys_conf_dir"] = GetProcessExecutionDir();
    writeLogtailConfigJSON(value);
    AppConfig* app_config = AppConfig::GetInstance();
    app_config->LoadAppConfig(STRING_FLAG(ilogtail_config));
    auto configjson = app_config->GetConfig();
    config_->LoadEbpfConfig(configjson);

    APSARA_TEST_EQUAL(config_->GetReceiveEventChanCap(), 4096);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mDebugMode, false);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mLogLevel, "warn");
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mPushAllSpan, false);
    APSARA_TEST_EQUAL(config_->GetAggregationConfig().mAggWindowSecond, 15);
    APSARA_TEST_EQUAL(config_->GetConverageConfig().mStrategy, "combine");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mStrategy, "fixedRate");
    APSARA_TEST_EQUAL(config_->GetSampleConfig().mConfig.mRate, 0.01);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mSlowRequestThresholdMs, 500);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxConnTrackers, 10000);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxBandWidthMbPerSec, 30);
    APSARA_TEST_EQUAL(config_->GetSocketProbeConfig().mMaxRawRecordPerSec, 100000);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileSampleRate, 10);
    APSARA_TEST_EQUAL(config_->GetProfileProbeConfig().mProfileUploadDuration, 10);
    APSARA_TEST_EQUAL(config_->GetProcessProbeConfig().mEnableOOMDetect, false);
}

void eBPFServerUnittest::TestLoadEbpfParametersV2() {
    Json::Value value;
    setJSON(value, "ebpf_receive_event_chan_cap", 4096);
    setJSON(value, "ebpf_admin_config_debug_mode", false);

    writeLogtailConfigJSON(value);
    TestEbpfParameters();
}

void eBPFServerUnittest::TestEbpfParameters() {
    AppConfig* appConfig = AppConfig::GetInstance();
    appConfig->LoadAppConfig(STRING_FLAG(ilogtail_config));

    APSARA_TEST_EQUAL(config_->GetReceiveEventChanCap(), 4096);
    APSARA_TEST_EQUAL(config_->GetAdminConfig().mDebugMode, false);
}

void eBPFServerUnittest::GenerateBatchMeasure(nami::NamiHandleBatchMeasureFunc cb) {
    const std::vector<std::string> app_ids = {"60d360af9bb426c8a9c5aad4b0b21c06", // apm-http-server
                                            "16466f6d0782d6ae16d7ac1ccb673ca7" // apm-http-client
    };
    const std::vector<std::string> ips = {"172.16.0.207", "172.16.0.210", "172.16.0.209"};
    const std::vector<std::string> server_app_ids = {"60d360af9bb426c8a9c5aad4b0b21c06", // apm-http-server
    };
    const std::vector<std::string> client_app_ids = {"16466f6d0782d6ae16d7ac1ccb673ca7" // apm-http-client
    };
    const std::vector<std::string> client_ips = {"172.16.0.207", "172.16.0.210"};
    const std::vector<std::string> server_ips = {"172.16.0.209"};
        std::vector<std::unique_ptr<ApplicationBatchMeasure>> batch_app_measures;
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    // client side
    for (size_t i = 0 ; i < client_app_ids.size(); i ++ ) { // 1
        for (size_t j = 0; j < client_ips.size(); j ++) { // 2 * 6 = 12
            std::unique_ptr<ApplicationBatchMeasure> app_measure_ptr = std::make_unique<ApplicationBatchMeasure>();
            app_measure_ptr->app_id_ = client_app_ids[i];
            app_measure_ptr->ip_ = client_ips[j];
            // generate app metrics
            for (size_t z = 0 ; z < 5; z ++ ) { // 5
                std::unique_ptr<Measure> measure_ptr = std::make_unique<Measure>();
                measure_ptr->type_ = MEASURE_TYPE_APP;
                measure_ptr->tags_ = {
                    {"workloadName", "apm-http-client"},
                    {"workloadKind", "deployment"},
                    {"namespace", "default"},
                    {"source_ip", client_ips[j]},
                    {"host", client_ips[j]},
                    {"rpc", "/shoes/" + std::to_string(z)},
                    {"rpcType", "25"},
                    {"callType", "http_client"},
        //              {"statusCode", "200"},
                    {"version", "HTTP1.1"},
                    {"source", "ebpf"},
                    {"endpoint","/shoes/" + std::to_string(z)},
                    {"destId","apm-http-server"},
                };
                AppSingleMeasure* sm = new AppSingleMeasure;
                sm->request_total_ = 40 + generateRandomInt(20);
                sm->error_total_ = 4;
                sm->slow_total_ = 1;
                sm->duration_ms_sum_ = 12500 + generateRandomInt(2000);
                sm->status_2xx_count_ = sm->request_total_ - sm->error_total_;
                sm->status_3xx_count_ = 0;
                sm->status_4xx_count_ = 0;
                sm->status_5xx_count_ = 4;
                std::unique_ptr<AbstractSingleMeasure> sm_ptr(sm);
                measure_ptr->inner_measure_ = std::move(sm_ptr);
                app_measure_ptr->measures_.emplace_back(std::move(measure_ptr));
            }
            // generate tcp metrics
            std::unique_ptr<Measure> measure_ptr = std::make_unique<Measure>();
            measure_ptr->type_ = MEASURE_TYPE_NET;
            measure_ptr->tags_ = {
                {"src_name", "apm-http-client"},
                {"dst_name", "apm-http-server"},
                {"src_namespace", "default"},
                {"dst_namespace", "default"},
                {"src_type", "deployment"},
                {"src_type", "deployment"},
                {"workloadName", "apm-http-client"},
                {"workloadKind", "deployment"},
                {"namespace", "default"},
                {"source_ip", client_ips[j]},
                {"host", client_ips[j]},
                {"dest_ip", server_ips[0]},
                {"appType", "ebpf"},
                {"callType", "http_client"},
                {"source", "ebpf"},
            };
            NetSingleMeasure* sm = new NetSingleMeasure;
            sm->recv_byte_total_ = 15631 + generateRandomInt(500);
            sm->send_byte_total_ = 18676 + generateRandomInt(400);
            sm->send_pkt_total_ = 203 + generateRandomInt(50);
            sm->recv_pkt_total_ = 203 + generateRandomInt(40);
            sm->tcp_drop_total_ = 1 + generateRandomInt(10);
            sm->tcp_retran_total_ = 2 + generateRandomInt(20);
            std::unique_ptr<AbstractSingleMeasure> sm_ptr(sm);
            measure_ptr->inner_measure_ = std::move(sm_ptr);
            app_measure_ptr->measures_.emplace_back(std::move(measure_ptr)); // 1
            batch_app_measures.emplace_back(std::move(app_measure_ptr));
        }
    }
    // server side
    for (size_t i = 0 ; i < server_app_ids.size(); i ++ ) { // 1
        for (size_t j = 0; j < server_ips.size(); j ++) { // 1 * 7
            std::unique_ptr<ApplicationBatchMeasure> app_measure_ptr = std::make_unique<ApplicationBatchMeasure>();
            app_measure_ptr->app_id_ = server_app_ids[i];
            app_measure_ptr->ip_ = server_ips[j];
            // generate app metrics
            for (size_t z = 0 ; z < 5; z ++ ) { // 5
            std::unique_ptr<Measure> measure_ptr = std::make_unique<Measure>();
            measure_ptr->type_ = MEASURE_TYPE_APP;
            measure_ptr->tags_ = {
                {"workloadName", "apm-http-server"},
                {"workloadKind", "deployment"},
                {"namespace", "default"},
                {"source_ip", server_ips[j]},
                {"host", server_ips[j]},
                {"rpc", "/shoes/" + std::to_string(z)},
                {"rpcType", "0"},
                {"callType", "http"},
                {"destId","/shoes/" + std::to_string(z)},
                {"endpoint","apm-http-client"},
    //              {"statusCode", "200"},
                {"version", "HTTP1.1"},
                {"source", "ebpf"},
            };
            AppSingleMeasure* sm = new AppSingleMeasure;
            sm->request_total_ = 70 + generateRandomInt(20);
            sm->error_total_ = 8;
            sm->slow_total_ = 2;
            sm->duration_ms_sum_ = 25000 + generateRandomInt(2000);
            sm->status_2xx_count_ = sm->request_total_ - sm->error_total_;
            sm->status_3xx_count_ = 0;
            sm->status_4xx_count_ = 0;
            sm->status_5xx_count_ = 8;
            std::unique_ptr<AbstractSingleMeasure> sm_ptr(sm);
            measure_ptr->inner_measure_ = std::move(sm_ptr);
            app_measure_ptr->measures_.emplace_back(std::move(measure_ptr));
            }
            // generate tcp metrics
            for (size_t z = 0; z < client_ips.size(); z ++ ) { // 2
                std::unique_ptr<Measure> measure_ptr = std::make_unique<Measure>();
                measure_ptr->type_ = MEASURE_TYPE_NET;
                measure_ptr->tags_ = {
                    {"src_name", "apm-http-server"},
                    {"dst_name", "apm-http-client"},
                    {"src_namespace", "default"},
                    {"dst_namespace", "default"},
                    {"src_type", "deployment"},
                    {"src_type", "deployment"},
                    {"workloadName", "apm-http-server"},
                    {"workloadKind", "deployment"},
                    {"namespace", "default"},
                    {"source_ip", server_ips[j]},
                    {"host", server_ips[j]},
                    {"dest_ip", client_ips[z]},
                    {"appType", "ebpf"},
                    {"callType", "conn_stats"},
                    {"source", "ebpf"},
                };
                NetSingleMeasure* sm = new NetSingleMeasure;
                sm->recv_byte_total_ = 15631 + generateRandomInt(500);
                sm->send_byte_total_ = 18676 + generateRandomInt(400);
                sm->recv_pkt_total_ = 203 + generateRandomInt(50);
                sm->send_pkt_total_ = 203 + generateRandomInt(40);
                sm->tcp_drop_total_ = 1 + generateRandomInt(10);
                sm->tcp_retran_total_ = 2 + generateRandomInt(20);
                std::unique_ptr<AbstractSingleMeasure> sm_ptr(sm);
                measure_ptr->inner_measure_ = std::move(sm_ptr);
                app_measure_ptr->measures_.emplace_back(std::move(measure_ptr));
            }
            batch_app_measures.emplace_back(std::move(app_measure_ptr));
        }
    }
    cb(std::move(batch_app_measures), 100000);
}

void eBPFServerUnittest::GenerateBatchAppEvent(nami::NamiHandleBatchEventFunc cb) {
    std::vector<std::unique_ptr<ApplicationBatchEvent>> batch_app_events;
    std::vector<std::string> apps = {"a6rx69e8me@582846f37273cf8", "a6rx69e8me@582846f37273cf9", "a6rx69e8me@582846f37273c10"};
    
    for (int i = 0 ; i < apps.size(); i ++) { // 3 apps
        std::vector<std::pair<std::string, std::string>> appTags = {{"hh", "hh"}, {"e", "e"}, {"f", std::to_string(i)}};
        std::unique_ptr<ApplicationBatchEvent> appEvent = std::make_unique<ApplicationBatchEvent>(apps[i], std::move(appTags));
        for (int j = 0; j < 1000; j ++) {
            std::vector<std::pair<std::string, std::string>> tags = {{"1", "1"}, {"2", "2"}, {"3",std::to_string(j)}};
            std::unique_ptr<SingleEvent> se = std::make_unique<SingleEvent>(std::move(tags), 0);
            appEvent->AppendEvent(std::move(se));
        }
        batch_app_events.emplace_back(std::move(appEvent));
    }

    if (cb) cb(std::move(batch_app_events));

    return;
}

void eBPFServerUnittest::GenerateBatchSpan(nami::NamiHandleBatchSpanFunc cb) {
    std::vector<std::unique_ptr<ApplicationBatchSpan>> batch_app_spans;
    // agg for app level
    std::unique_ptr<ApplicationBatchSpan> batch_spans = std::make_unique<ApplicationBatchSpan>();
    for (int i = 0 ; i < 5; i ++) { // 5
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        uint64_t current_time_nano = static_cast<uint64_t>(nano);
        uint64_t end_time_nano = static_cast<uint64_t>(nano) + 200000000;
        batch_spans->app_id_ = "a6rx69e8me@582846f37273cf8";
        std::unique_ptr<SingleSpan> single_span = std::make_unique<SingleSpan>();
        // set single span
        single_span->tags_ = {
            {"workloadName", "apm-http-client"},
            {"workloadKind", "deployment"},
            {"namespace", "default"},
            {"source_ip", "172.16.57.0"},
            {"host", "172.16.57.0"},
            {"rpc", "/shoes/" + std::to_string(i)},
            {"rpcType", "25"},
            {"callType", "http_client"},
            {"statusCode", "200"},
            {"version", "HTTP1.1"},
            {"source", "ebpf"},
        };
        single_span->span_name_ = "/shoes/" + std::to_string(i);
        single_span->span_kind_ = SpanKindInner::Client;
        single_span->trace_id_ = "23u927398719379";
        single_span->span_id_ = "2121382973984";
        single_span->start_timestamp_ = current_time_nano;
        single_span->end_timestamp_ = end_time_nano;
        batch_spans->single_spans_.emplace_back(std::move(single_span));
    }
    batch_app_spans.emplace_back(std::move(batch_spans));
    cb(std::move(batch_app_spans));
}

void eBPFServerUnittest::GenerateBatchEvent(nami::NamiHandleBatchDataEventFn cb, SecureEventType type) {
    std::vector<std::unique_ptr<AbstractSecurityEvent>> events;
    for (int i = 0 ; i< 1000; i ++ ) {
        std::vector<std::pair<std::string, std::string>> tags;
        tags.push_back({"hh", "hh"});
        tags.push_back({"ee", "hh"});
        tags.push_back({"zz", "hh"});
        tags.push_back({"tt", "hh"});
        tags.push_back({"aa", "hh"});

        auto event = std::make_unique<AbstractSecurityEvent> (std::move(tags), type, 1000);
        events.emplace_back(std::move(event));
    }
    cb(std::move(events));
}

void eBPFServerUnittest::InitSecurityOpts() {
    
}

void eBPFServerUnittest::InitObserverOpts() {

}

void eBPFServerUnittest::TestInit() {
    auto host_name = ebpf::eBPFServer::GetInstance()->mSourceManager->mHostName;
    auto host_ip = ebpf::eBPFServer::GetInstance()->mSourceManager->mHostIp;
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager != nullptr);
    EXPECT_TRUE(host_name.length());
    EXPECT_TRUE(host_ip.length());
}

void eBPFServerUnittest::TestEnableNetworkPlugin() {
    std::string configStr = R"(
        {
            "Type": "input_ebpf_sockettraceprobe_observer",
            "ProbeConfig": 
            {
                "EnableLog": true,
                "EnableMetric": true,
                "EnableSpan": true,
                "EnableProtocols": [
                    "http"
                ],
                "DisableProtocolParse": 1,
                "DisableConnStats": false,
                "EnableConnTrackerDump": false,
                "EnableEvent": true,
            }
        }
    )";
    std::string errorMsg;
    Json::Value configJson;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    
    nami::ObserverNetworkOption network_option;
    bool res = ebpf::InitObserverNetworkOption(configJson, network_option, &ctx, "test");
    EXPECT_TRUE(res);
    // observer_options.Init(ObserverType::NETWORK, configJson, &ctx, "test");
    res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "test", 1,
        nami::PluginType::NETWORK_OBSERVE,
        &ctx,
        &network_option);
    
    EXPECT_TRUE(res);
    auto conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    auto network_conf = std::get<nami::NetworkObserveConfig>(conf->config_);
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::NETWORK_OBSERVE);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE);
    std::cout << "3" << std::endl;
    EXPECT_TRUE(network_conf.measure_cb_ != nullptr);
    EXPECT_TRUE(network_conf.span_cb_ != nullptr);
    EXPECT_TRUE(network_conf.so_.size());
    EXPECT_TRUE(network_conf.so_.find("libnetwork_observer.so") != std::string::npos);
    EXPECT_TRUE(network_conf.so_size_ != 0);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mPluginIdx, 1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mPluginIdx, 1);
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::NETWORK_OBSERVE)]);

    // do suspend
    ebpf::eBPFServer::GetInstance()->SuspendPlugin("test", nami::PluginType::NETWORK_OBSERVE);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mQueueKey, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mQueueKey, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mPluginIdx, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mPluginIdx, -1);
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::NETWORK_OBSERVE)]);

    // do update
    res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "test", 8,
        nami::PluginType::NETWORK_OBSERVE,
        &ctx,
        &network_option);
    EXPECT_TRUE(res);
    conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::NETWORK_OBSERVE);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_CONFIG_CHAGE);

    GenerateBatchMeasure(network_conf.measure_cb_);
    GenerateBatchSpan(network_conf.span_cb_);
    GenerateBatchAppEvent(network_conf.event_cb_);
    auto after_conf = std::get<nami::NetworkObserveConfig>(conf->config_);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mEventCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mPluginIdx, 8);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mPluginIdx, 8);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mEventCB->mPluginIdx, 8);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mMeterCB->mProcessTotalCnt, 19);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mProcessTotalCnt, 5);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mEventCB->mProcessTotalCnt, 3000);

    // do stop
    ebpf::eBPFServer::GetInstance()->DisablePlugin("test", nami::PluginType::NETWORK_OBSERVE);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mSpanCB->mQueueKey,-1);
    EXPECT_TRUE(!ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::NETWORK_OBSERVE)]);
}

void eBPFServerUnittest::TestEnableProcessPlugin() {
    std::string configStr = R"(
        {
            "Type": "input_ebpf_processprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": [
                        "sys_enter_execve",
                        "disassociate_ctty",
                        "acct_process",
                        "wake_up_new_task"
                    ]
                }
            ]
        }
    )";

    std::string errorMsg;
    Json::Value configJson;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    std::cout << "1" << std::endl;
    SecurityOptions security_options;
    security_options.Init(SecurityProbeType::PROCESS, configJson, &ctx, "input_ebpf_processprobe_security");
    bool res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "test", 0,
        nami::PluginType::PROCESS_SECURITY,
        &ctx,
        &security_options);
    EXPECT_TRUE(res);
    auto conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::PROCESS_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE);
    auto process_conf = std::get<nami::ProcessConfig>(conf->config_);
    EXPECT_TRUE(process_conf.process_security_cb_ != nullptr);
    LOG_WARNING(sLogger, ("process_conf.options_ size", process_conf.options_.size()));
    EXPECT_EQ(process_conf.options_.size(), 1);
    EXPECT_EQ(process_conf.options_[0].call_names_.size(), 4);

    // do suspend
    ebpf::eBPFServer::GetInstance()->SuspendPlugin("test", nami::PluginType::PROCESS_SECURITY);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mProcessSecureCB->mQueueKey, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mProcessSecureCB->mPluginIdx, -1);
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::PROCESS_SECURITY)]);

    res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "test", 0,
        nami::PluginType::PROCESS_SECURITY,
        &ctx,
        &security_options);
    EXPECT_TRUE(res);
    conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::PROCESS_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_CONFIG_CHAGE);
    auto after_conf = std::get<nami::ProcessConfig>(conf->config_);

    GenerateBatchEvent(after_conf.process_security_cb_, SecureEventType::SECURE_EVENT_TYPE_PROCESS_SECURE);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mProcessSecureCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mProcessSecureCB->mProcessTotalCnt, 1000);
}

void eBPFServerUnittest::TestEnableNetworkSecurePlugin() {
    std::string configStr = R"(
        {
            "Type": "input_ebpf_sockettraceprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["tcp_connect", "tcp_close"],
                    "AddrFilter": {
                        "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                        "DestPortList": [80],
                        "SourceAddrBlackList": ["127.0.0.1/8"],
                        "SourcePortBlackList": [9300]
                    }
                },
                {
                    "CallNameFilter": ["tcp_sendmsg"],
                    "AddrFilter": {
                        "DestAddrList": ["10.0.0.0/8","92.168.0.0/16"],
                        "DestPortList": [80]
                    }
                }
            ]
        }
    )";
    
    std::string errorMsg;
    Json::Value configJson;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    SecurityOptions security_options;
    security_options.Init(SecurityProbeType::NETWORK, configJson, &ctx, "input_ebpf_sockettraceprobe_security");
    bool res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "input_ebpf_sockettraceprobe_security", 5,
        nami::PluginType::NETWORK_SECURITY,
        &ctx,
        &security_options);
    EXPECT_TRUE(res);
    auto conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::NETWORK_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE);
    auto inner_conf = std::get<nami::NetworkSecurityConfig>(conf->config_);
    EXPECT_TRUE(inner_conf.network_security_cb_ != nullptr);
    EXPECT_EQ(inner_conf.options_.size(), 2);
    EXPECT_EQ(inner_conf.options_[0].call_names_.size(), 2);
    auto filter = std::get<nami::SecurityNetworkFilter>(inner_conf.options_[0].filter_);
    EXPECT_EQ(filter.mDestAddrList.size(), 2);
    EXPECT_EQ(filter.mDestPortList.size(), 1);
    EXPECT_EQ(filter.mSourceAddrBlackList.size(), 1);
    EXPECT_EQ(filter.mSourcePortBlackList.size(), 1);

    // do suspend
    ebpf::eBPFServer::GetInstance()->SuspendPlugin("test", nami::PluginType::NETWORK_SECURITY);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mNetworkSecureCB->mQueueKey, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mNetworkSecureCB->mPluginIdx, -1);
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::NETWORK_SECURITY)]);

    res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "input_ebpf_sockettraceprobe_security", 0,
        nami::PluginType::NETWORK_SECURITY,
        &ctx,
        &security_options);
    EXPECT_TRUE(res);
    conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::NETWORK_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_CONFIG_CHAGE);

    auto after_conf = std::get<nami::NetworkSecurityConfig>(conf->config_);

    GenerateBatchEvent(after_conf.network_security_cb_, SecureEventType::SECURE_EVENT_TYPE_SOCKET_SECURE);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mNetworkSecureCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mNetworkSecureCB->mProcessTotalCnt, 1000);
}

void eBPFServerUnittest::TestEnableFileSecurePlugin() {
    std::string configStr = R"(
        {
            "Type": "input_ebpf_fileprobe_security",
            "ProbeConfig": [
                {
                    "CallNameFilter": ["security_file_permission"],
                    "FilePathFilter": [
                        "/etc/passwd",
                        "/etc/shadow",
                        "/bin"
                    ]
                }
            ]
        }
    )";

    std::string errorMsg;
    Json::Value configJson;
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));
    std::cout << "1" << std::endl;
    SecurityOptions security_options;
    security_options.Init(SecurityProbeType::FILE, configJson, &ctx, "input_ebpf_fileprobe_security");
    bool res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "input_ebpf_fileprobe_security", 0,
        nami::PluginType::FILE_SECURITY,
        &ctx,
        &security_options);
    EXPECT_EQ(std::get<nami::SecurityFileFilter>(security_options.mOptionList[0].filter_).mFilePathList.size(), 3);
    EXPECT_TRUE(res);
    auto conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::FILE_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_ENABLE_PROBE);
    auto inner_conf = std::get<nami::FileSecurityConfig>(conf->config_);
    EXPECT_TRUE(inner_conf.file_security_cb_ != nullptr);
    EXPECT_EQ(inner_conf.options_.size(), 1);
    EXPECT_EQ(inner_conf.options_[0].call_names_.size(), 1);
    auto filter = std::get<nami::SecurityFileFilter>(inner_conf.options_[0].filter_);
    EXPECT_EQ(filter.mFilePathList.size(), 3);
    EXPECT_EQ(filter.mFilePathList[0], "/etc/passwd");
    EXPECT_EQ(filter.mFilePathList[1], "/etc/shadow");

    // do suspend
    ebpf::eBPFServer::GetInstance()->SuspendPlugin("test", nami::PluginType::FILE_SECURITY);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mFileSecureCB->mQueueKey, -1);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mFileSecureCB->mPluginIdx, -1);
    EXPECT_TRUE(ebpf::eBPFServer::GetInstance()->mSourceManager->mRunning[int(nami::PluginType::FILE_SECURITY)]);

    res = ebpf::eBPFServer::GetInstance()->EnablePlugin(
        "input_ebpf_fileprobe_security", 0,
        nami::PluginType::FILE_SECURITY,
        &ctx,
        &security_options);
    EXPECT_TRUE(res);
    conf = ebpf::eBPFServer::GetInstance()->mSourceManager->mConfig;
    EXPECT_EQ(conf->plugin_type_, nami::PluginType::FILE_SECURITY);
    EXPECT_EQ(conf->type, UpdataType::SECURE_UPDATE_TYPE_CONFIG_CHAGE);

    auto after_conf = std::get<nami::FileSecurityConfig>(conf->config_);

    GenerateBatchEvent(after_conf.file_security_cb_, SecureEventType::SECURE_EVENT_TYPE_FILE_SECURE);
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mFileSecureCB->mQueueKey, ctx.GetProcessQueueKey());
    EXPECT_EQ(ebpf::eBPFServer::GetInstance()->mFileSecureCB->mProcessTotalCnt, 1000);
}

UNIT_TEST_CASE(eBPFServerUnittest, TestDefaultEbpfParameters);
UNIT_TEST_CASE(eBPFServerUnittest, TestDefaultAndLoadEbpfParameters);
UNIT_TEST_CASE(eBPFServerUnittest, TestLoadEbpfParametersV1);
UNIT_TEST_CASE(eBPFServerUnittest, TestLoadEbpfParametersV2);
UNIT_TEST_CASE(eBPFServerUnittest, TestInit)
UNIT_TEST_CASE(eBPFServerUnittest, TestEnableNetworkPlugin)
UNIT_TEST_CASE(eBPFServerUnittest, TestEnableProcessPlugin)
UNIT_TEST_CASE(eBPFServerUnittest, TestEnableNetworkSecurePlugin)
UNIT_TEST_CASE(eBPFServerUnittest, TestEnableFileSecurePlugin)
}
}

UNIT_TEST_MAIN
