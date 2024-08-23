#pragma once

#include <json/value.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "prometheus/labels/Relabel.h"


namespace logtail {

class ScrapeConfig {
public:
    std::string mJobName;
    int64_t mScrapeIntervalSeconds;
    int64_t mScrapeTimeoutSeconds;
    std::string mMetricsPath;
    std::string mScheme;

    std::map<std::string, std::string> mAuthHeaders;

    int64_t mMaxScrapeSizeBytes;
    int64_t mSampleLimit;
    int64_t mSeriesLimit;
    std::vector<RelabelConfig> mRelabelConfigs;

    std::map<std::string, std::vector<std::string>> mParams;

    std::string mQueryString;

    ScrapeConfig();
    bool Init(const Json::Value& config);

private:
    bool InitBasicAuth(const Json::Value& basicAuth);
    bool InitAuthorization(const Json::Value& authorization);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ScrapeConfigUnittest;
#endif
};

} // namespace logtail