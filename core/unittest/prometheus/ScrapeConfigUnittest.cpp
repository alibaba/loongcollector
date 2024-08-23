
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
            "scheme": "http",
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

    // basic auth
    APSARA_TEST_EQUAL(scrapeConfig.mAuthHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQ=");

    APSARA_TEST_EQUAL(scrapeConfig.mMaxScrapeSizeBytes, 1024 * 1024 * 1024);
    APSARA_TEST_EQUAL(scrapeConfig.mSampleLimit, 10000);
    APSARA_TEST_EQUAL(scrapeConfig.mSeriesLimit, 10000);
    APSARA_TEST_EQUAL(scrapeConfig.mRelabelConfigs.size(), 1UL);
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
    APSARA_TEST_EQUAL(scrapeConfig.mAuthHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQ=");

    scrapeConfig.mAuthHeaders.clear();
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
    APSARA_TEST_EQUAL(scrapeConfig.mAuthHeaders["Authorization"], "Basic dGVzdF91c2VyOnRlc3RfcGFzc3dvcmQuZmlsZQ==");

    // error
    scrapeConfig.mAuthHeaders.clear();
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
    scrapeConfig.mAuthHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    // bearer auth
    APSARA_TEST_EQUAL(scrapeConfig.mAuthHeaders["Authorization"], "Bearer test_token");

    scrapeConfig.mAuthHeaders.clear();

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
    scrapeConfig.mAuthHeaders.clear();
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mAuthHeaders["Authorization"], "Bearer " + mKey);
}

UNIT_TEST_CASE(ScrapeConfigUnittest, TestInit);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestAuth);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestBasicAuth);
UNIT_TEST_CASE(ScrapeConfigUnittest, TestAuthorization);

} // namespace logtail

UNIT_TEST_MAIN