
#include "prometheus/schedulers/ScrapeConfig.h"

#include <json/value.h>

#include <string>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

using namespace std;

namespace logtail {
ScrapeConfig::ScrapeConfig()
    : mScheme("http"),
      mMetricsPath("/metrics"),
      mScrapeIntervalSeconds(60),
      mScrapeTimeoutSeconds(10),
      mMaxScrapeSizeBytes(-1),
      mSampleLimit(-1),
      mSeriesLimit(-1) {
}
bool ScrapeConfig::Init(const Json::Value& scrapeConfig) {
    if (scrapeConfig.isMember(prometheus::JOB_NAME) && scrapeConfig[prometheus::JOB_NAME].isString()) {
        mJobName = scrapeConfig[prometheus::JOB_NAME].asString();
        if (mJobName.empty()) {
            LOG_ERROR(sLogger, ("job name is empty", ""));
            return false;
        }
    } else {
        return false;
    }
    if (scrapeConfig.isMember(prometheus::SCHEME) && scrapeConfig[prometheus::SCHEME].isString()) {
        mScheme = scrapeConfig[prometheus::SCHEME].asString();
    }
    if (scrapeConfig.isMember(prometheus::METRICS_PATH) && scrapeConfig[prometheus::METRICS_PATH].isString()) {
        mMetricsPath = scrapeConfig[prometheus::METRICS_PATH].asString();
    }
    if (scrapeConfig.isMember(prometheus::SCRAPE_INTERVAL) && scrapeConfig[prometheus::SCRAPE_INTERVAL].isString()) {
        string tmpScrapeIntervalString = scrapeConfig[prometheus::SCRAPE_INTERVAL].asString();
        mScrapeIntervalSeconds = DurationToSecond(tmpScrapeIntervalString);
    }
    if (scrapeConfig.isMember(prometheus::SCRAPE_TIMEOUT) && scrapeConfig[prometheus::SCRAPE_TIMEOUT].isString()) {
        string tmpScrapeTimeoutString = scrapeConfig[prometheus::SCRAPE_TIMEOUT].asString();
        mScrapeTimeoutSeconds = DurationToSecond(tmpScrapeTimeoutString);
    }
    // <size>: a size in bytes, e.g. 512MB. A unit is required. Supported units: B, KB, MB, GB, TB, PB, EB.
    if (scrapeConfig.isMember(prometheus::MAX_SCRAPE_SIZE) && scrapeConfig[prometheus::MAX_SCRAPE_SIZE].isString()) {
        string tmpMaxScrapeSize = scrapeConfig[prometheus::MAX_SCRAPE_SIZE].asString();
        if (tmpMaxScrapeSize.empty()) {
            mMaxScrapeSizeBytes = -1;
        } else if (EndWith(tmpMaxScrapeSize, "KiB") || EndWith(tmpMaxScrapeSize, "K")
                   || EndWith(tmpMaxScrapeSize, "KB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('K'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "MiB") || EndWith(tmpMaxScrapeSize, "M")
                   || EndWith(tmpMaxScrapeSize, "MB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('M'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024 * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "GiB") || EndWith(tmpMaxScrapeSize, "G")
                   || EndWith(tmpMaxScrapeSize, "GB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('G'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024 * 1024 * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "TiB") || EndWith(tmpMaxScrapeSize, "T")
                   || EndWith(tmpMaxScrapeSize, "TB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('T'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024 * 1024 * 1024 * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "PiB") || EndWith(tmpMaxScrapeSize, "P")
                   || EndWith(tmpMaxScrapeSize, "PB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('P'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024 * 1024 * 1024 * 1024 * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "EiB") || EndWith(tmpMaxScrapeSize, "E")
                   || EndWith(tmpMaxScrapeSize, "EB")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('E'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize) * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
        } else if (EndWith(tmpMaxScrapeSize, "B")) {
            tmpMaxScrapeSize = tmpMaxScrapeSize.substr(0, tmpMaxScrapeSize.find('B'));
            mMaxScrapeSizeBytes = stoll(tmpMaxScrapeSize);
        }
    }

    if (scrapeConfig.isMember(prometheus::SAMPLE_LIMIT) && scrapeConfig[prometheus::SAMPLE_LIMIT].isInt64()) {
        mSampleLimit = scrapeConfig[prometheus::SAMPLE_LIMIT].asInt64();
    }
    if (scrapeConfig.isMember(prometheus::SERIES_LIMIT) && scrapeConfig[prometheus::SERIES_LIMIT].isInt64()) {
        mSeriesLimit = scrapeConfig[prometheus::SERIES_LIMIT].asInt64();
    }
    if (scrapeConfig.isMember(prometheus::PARAMS) && scrapeConfig[prometheus::PARAMS].isObject()) {
        const Json::Value& params = scrapeConfig[prometheus::PARAMS];
        if (params.isObject()) {
            for (const auto& key : params.getMemberNames()) {
                const Json::Value& values = params[key];
                if (values.isArray()) {
                    vector<string> valueList;
                    for (const auto& value : values) {
                        valueList.push_back(value.asString());
                    }
                    mParams[key] = valueList;
                }
            }
        }
    }

    if (scrapeConfig.isMember(prometheus::AUTHORIZATION) && scrapeConfig[prometheus::AUTHORIZATION].isObject()) {
        string type = scrapeConfig[prometheus::AUTHORIZATION][prometheus::TYPE].asString();
        string bearerToken;
        bool b
            = ReadFile(scrapeConfig[prometheus::AUTHORIZATION][prometheus::CREDENTIALS_FILE].asString(), bearerToken);
        if (!b) {
            LOG_ERROR(sLogger,
                      ("read credentials_file failed, credentials_file",
                       scrapeConfig[prometheus::AUTHORIZATION][prometheus::CREDENTIALS_FILE].asString()));
            return false;
        }
        mHeaders[prometheus::A_UTHORIZATION] = type + " " + bearerToken;
    }

    for (const auto& relabelConfig : scrapeConfig[prometheus::RELABEL_CONFIGS]) {
        mRelabelConfigs.emplace_back(relabelConfig);
    }

    // build query string
    for (auto it = mParams.begin(); it != mParams.end(); ++it) {
        const auto& key = it->first;
        const auto& values = it->second;
        for (const auto& value : values) {
            if (!mQueryString.empty()) {
                mQueryString += "&";
            }
            mQueryString += key;
            mQueryString += "=";
            mQueryString += value;
        }
    }

    return true;
}
} // namespace logtail