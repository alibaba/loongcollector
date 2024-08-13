
#include <string>

#include "JsonUtil.h"
#include "json/value.h"
#include "prometheus/schedulers/ScrapeConfig.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class ScrapeConfigUnittest : public testing::Test {
public:
    void TestInit();
};

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
    configStr = R"JSON({
        "job_name": "test_job",
        "scheme": "http",
        "metrics_path": "/metrics",
        "scrape_interval": "30s",
        "scrape_timeout": "30s",
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
    APSARA_TEST_TRUE(ParseJsonTable(configStr, config, errorMsg));
    APSARA_TEST_TRUE(scrapeConfig.Init(config));
    APSARA_TEST_EQUAL(scrapeConfig.mJobName, "test_job");
    APSARA_TEST_EQUAL(scrapeConfig.mScheme, "http");
    APSARA_TEST_EQUAL(scrapeConfig.mMetricsPath, "/metrics");
    APSARA_TEST_EQUAL(scrapeConfig.mScrapeIntervalSeconds, 30);
    APSARA_TEST_EQUAL(scrapeConfig.mScrapeTimeoutSeconds, 30);
    APSARA_TEST_EQUAL(scrapeConfig.mMaxScrapeSizeBytes, 1024 * 1024 * 1024);
    APSARA_TEST_EQUAL(scrapeConfig.mSampleLimit, 10000);
    APSARA_TEST_EQUAL(scrapeConfig.mSeriesLimit, 10000);
    APSARA_TEST_EQUAL(scrapeConfig.mRelabelConfigs.size(), 1UL);
    APSARA_TEST_EQUAL(scrapeConfig.mParams["__param_query"][0], "test_query");
    APSARA_TEST_EQUAL(scrapeConfig.mParams["__param_query_1"][0], "test_query_1");
}

UNIT_TEST_CASE(ScrapeConfigUnittest, TestInit);

} // namespace logtail

UNIT_TEST_MAIN