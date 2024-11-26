
#include <cstdio>
#include <string>

#include "FileSystemUtil.h"
#include "JsonUtil.h"
#include "json/value.h"
#include "prometheus/schedulers/ScrapeConfig.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ScrapeConfigUnittest : public testing::Test {
public:
    void TestInit();
    void TestAuth();
    void TestBasicAuth();
    void TestAuthorization();
    void TestScrapeProtocols();
    void TestEnableCompression();
    void TestTLS();

private:
    void SetUp() override;
    void TearDown() override;

    string mFilePath = "prom_password.file";
    string mKey = "test_password.file";
};

void ScrapeConfigUnittest::SetUp() {
    // create test_password.file
    OverwriteFile(mFilePath, mKey);
}

void ScrapeConfigUnittest::TearDown() {
    remove(mFilePath.c_str());
}

void ScrapeConfigUnittest::TestInit() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    // error config
    configStr = R"JSON({
        
    })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_FALSE(scrapeConfig.Init(config));

    // all useful config
    {
        configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scrape_protocols": [
                "PrometheusText0.0.4",
                "PrometheusProto",
                "OpenMetricsText0.0.1"
            ],
            "follow_redirects": false,
            "tls_config": {
                "ca_file": "ca_file",
                "cert_file": "cert_file",
                "key_file": "key_file",
                "insecure_skip_verify": true
            },
            "enable_compression": false,
            "scheme": "http",
            "honor_labels": true,
            "honor_timestamps": false,
            "basic_auth": {
                "username": "test_user",
                "password": "test_password"
            },
            "max_scrape_size": "1024MiB",
            "sample_limit": 10000,
            "series_limit": 10000,
            "relabel_configs": [
                {
                    "action": "keep",
                    "regex": "kube-state-metrics",
                    "replacement": "$1",
                    "separator": ";",
                    "source_labels": [
                        "__meta_kubernetes_pod_label_k8s_app"
                    ]
                }
            ],
            "params" : {
                "__param_query": [
                    "test_query"
                ],
                "__param_query_1": [
                    "test_query_1"
                ]
            }
        })JSON";
    }
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mJobName, "test_job");
    APSARA_TEST_EQUAL(scrapeConfig.mScrapeIntervalSeconds, 30);
    APSARA_TEST_EQUAL(scrapeConfig.mScrapeTimeoutSeconds, 30);
    APSARA_TEST_EQUAL(scrapeConfig.mMetricsPath, "/metrics");
    APSARA_TEST_EQUAL(scrapeConfig.mScheme, "http");
    APSARA_TEST_EQUAL(scrapeConfig.mHonorLabels, true);
    APSARA_TEST_EQUAL(scrapeConfig.mHonorTimestamps, false);

    // scrape protocols
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Accept"],
                      "text/plain;version=0.0.4;q=0.4,application/"
                      "vnd.google.protobuf;proto=io.prometheus.client.MetricFamily;encoding=delimited;q=0.3,"
                      "application/openmetrics-text;version=0.0.1;q=0.2,*/*;q=0.1");

    // follow redirects
    APSARA_TEST_EQUAL(scrapeConfig.mFollowRedirects, false);

    // tls
    APSARA_TEST_EQUAL(scrapeConfig.mEnableTLS, true);
    APSARA_TEST_EQUAL(scrapeConfig.mTLS.mCaFile, "ca_file");
    APSARA_TEST_EQUAL(scrapeConfig.mTLS.mCertFile, "cert_file");
    APSARA_TEST_EQUAL(scrapeConfig.mTLS.mKeyFile, "key_file");
    APSARA_TEST_EQUAL(scrapeConfig.mTLS.mInsecureSkipVerify, true);

    // disable compression
    // APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Accept-Encoding"], "identity");

    // basic auth
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQ=");

    APSARA_TEST_EQUAL(scrapeConfig.mMaxScrapeSizeBytes, 1024 * 1024 * 1024ULL);
    APSARA_TEST_EQUAL(scrapeConfig.mSampleLimit, 10000ULL);
    APSARA_TEST_EQUAL(scrapeConfig.mSeriesLimit, 10000ULL);
    APSARA_TEST_EQUAL(scrapeConfig.mRelabelConfigs.mRelabelConfigs.size(), 1UL);
    APSARA_TEST_EQUAL(scrapeConfig.mParams["__param_query"][0], "test_query");
    APSARA_TEST_EQUAL(scrapeConfig.mParams["__param_query_1"][0], "test_query_1");
}

void ScrapeConfigUnittest::TestAuth() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    // error config
    configStr = R"JSON({
        "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "basic_auth": {
                "username": "test_user",
                "password": "test_password"
            },
            "authorization": {
                "type": "Bearer",
                "credentials": "test_token"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_FALSE(scrapeConfig.Init(config));
}

void ScrapeConfigUnittest::TestBasicAuth() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "basic_auth": {
                "username": "test_user",
                "password": "test_password"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQ=");

    scrapeConfig.mRequestHeaders.clear();
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "basic_auth": {
                "username": "test_user",
                "password_file": "prom_password.file"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQuZmlsZQ==");

    // error
    scrapeConfig.mRequestHeaders.clear();
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "basic_auth": {
                "username": "test_user",
                "password": "test_password",
                "password_file": "prom_password.file"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_FALSE(scrapeConfig.Init(config));
}

void ScrapeConfigUnittest::TestAuthorization() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "authorization": {
                "type": "Bearer",
                "credentials": "test_token"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    // bearer auth
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Authorization"], "Bearer test_token");

    scrapeConfig.mRequestHeaders.clear();

    // default Bearer auth
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "authorization": {
                "credentials_file": "prom_password.file"
            }
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mRequestHeaders["Authorization"], "Bearer " + mKey);
}

void ScrapeConfigUnittest::TestScrapeProtocols() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    // default
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http"
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(
        "text/plain;version=0.0.4;q=0.5,application/"
        "vnd.google.protobuf;proto=io.prometheus.client.MetricFamily;encoding=delimited;q=0.4,application/"
        "openmetrics-text;version=0.0.1;q=0.3,application/openmetrics-text;version=1.0.0;q=0.2,*/*;q=0.1",
        scrapeConfig.mRequestHeaders["Accept"]);

    // custom quality protocols
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["PrometheusProto", "OpenMetricsText1.0.0", "PrometheusText0.0.4", "OpenMetricsText0.0.1"]
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(
        "application/vnd.google.protobuf;proto=io.prometheus.client.MetricFamily;encoding=delimited;q=0.5,"
        "application/openmetrics-text;version=1.0.0;q=0.4,"
        "text/plain;version=0.0.4;q=0.3,application/openmetrics-text;version=0.0.1;q=0.2,*/*;q=0.1",
        scrapeConfig.mRequestHeaders["Accept"]);

    // only prometheus0.0.4 protocols
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["PrometheusText0.0.4"]
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL("text/plain;version=0.0.4;q=0.2,*/*;q=0.1", scrapeConfig.mRequestHeaders["Accept"]);

    // Capital error
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["prometheusproto"]
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_FALSE(scrapeConfig.Init(config));

    // OpenMetricsText1.0.0 duplication error
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["OpenMetricsText1.0.0", "PrometheusProto", "OpenMetricsText1.0.0"]
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_FALSE(scrapeConfig.Init(config));

    // protocols invalid
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["OpenMetricsText1.0.0", "PrometheusProto", 999]
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_FALSE(scrapeConfig.Init(config));

    // unknown protocol
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "scrape_protocols": ["OpenMetricsText"]
        })JSON";

    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_FALSE(scrapeConfig.Init(config));
}

void ScrapeConfigUnittest::TestEnableCompression() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    // default
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http"
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    // APSARA_TEST_EQUAL("gzip", scrapeConfig.mRequestHeaders["Accept-Encoding"]);

    // disable
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "enable_compression": false
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    // APSARA_TEST_EQUAL("identity", scrapeConfig.mRequestHeaders["Accept-Encoding"]);

    // enable
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "enable_compression": true
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    scrapeConfig.mRequestHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    // APSARA_TEST_EQUAL("gzip", scrapeConfig.mRequestHeaders["Accept-Encoding"]);
}

void ScrapeConfigUnittest::TestTLS() {
    Json::Value config;
    ScrapeConfig scrapeConfig;
    string errorMsg;
    string configStr;

    // default
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http"
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(false, scrapeConfig.mEnableTLS);

    // enable
    configStr = R"JSON({
            "job_name": "test_job",
            "scrape_interval": "30s",
            "scrape_timeout": "30s",
            "metrics_path": "/metrics",
            "scheme": "http",
            "tls_config": {
                "ca_file": "ca_file",
                "insecure_skip_verify": false
            }
        })JSON";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(true, scrapeConfig.mEnableTLS);
    APSARA_TEST_EQUAL("ca_file", scrapeConfig.mTLS.mCaFile);
    APSARA_TEST_EQUAL("", scrapeConfig.mTLS.mCertFile);
    APSARA_TEST_EQUAL("", scrapeConfig.mTLS.mKeyFile);
    APSARA_TEST_EQUAL(false, scrapeConfig.mTLS.mInsecureSkipVerify);
}

UNIT_TEST_CASE(ScrapeConfigUnittest, TestInit);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestAuth);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestBasicAuth);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestAuthorization);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestScrapeProtocols);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestEnableCompression);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestTLS);

} // namespace logtail

UNIT_TEST_MAIN