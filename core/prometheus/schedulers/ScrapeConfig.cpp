
#include "prometheus/schedulers/ScrapeConfig.h"

#include <json/value.h>

#include <string>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"
#include "sdk/Common.h"

using namespace std;

namespace logtail {
ScrapeConfig::ScrapeConfig()
    : mScrapeIntervalSeconds(60),
      mScrapeTimeoutSeconds(10),
      mMetricsPath("/metrics"),
      mScheme("http"),
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

    if (scrapeConfig.isMember(prometheus::SCRAPE_INTERVAL) && scrapeConfig[prometheus::SCRAPE_INTERVAL].isString()) {
        string tmpScrapeIntervalString = scrapeConfig[prometheus::SCRAPE_INTERVAL].asString();
        mScrapeIntervalSeconds = DurationToSecond(tmpScrapeIntervalString);
    }
    if (scrapeConfig.isMember(prometheus::SCRAPE_TIMEOUT) && scrapeConfig[prometheus::SCRAPE_TIMEOUT].isString()) {
        string tmpScrapeTimeoutString = scrapeConfig[prometheus::SCRAPE_TIMEOUT].asString();
        mScrapeTimeoutSeconds = DurationToSecond(tmpScrapeTimeoutString);
    }
    if (scrapeConfig.isMember(prometheus::METRICS_PATH) && scrapeConfig[prometheus::METRICS_PATH].isString()) {
        mMetricsPath = scrapeConfig[prometheus::METRICS_PATH].asString();
    }
    if (scrapeConfig.isMember(prometheus::SCHEME) && scrapeConfig[prometheus::SCHEME].isString()) {
        mScheme = scrapeConfig[prometheus::SCHEME].asString();
    }

    // basic auth, authorization, oauth2
    // basic auth, authorization, oauth2 cannot be used at the same time
    if ((int)scrapeConfig.isMember(prometheus::BASIC_AUTH) + scrapeConfig.isMember(prometheus::AUTHORIZATION) > 1) {
        LOG_ERROR(sLogger, ("basic auth and authorization cannot be used at the same time", ""));
        return false;
    }
    if (scrapeConfig.isMember(prometheus::BASIC_AUTH) && scrapeConfig[prometheus::BASIC_AUTH].isObject()) {
        if (!InitBasicAuth(scrapeConfig[prometheus::BASIC_AUTH])) {
            LOG_ERROR(sLogger, ("basic auth config error", ""));
            return false;
        }
    }
    if (scrapeConfig.isMember(prometheus::AUTHORIZATION) && scrapeConfig[prometheus::AUTHORIZATION].isObject()) {
        if (!InitAuthorization(scrapeConfig[prometheus::AUTHORIZATION])) {
            LOG_ERROR(sLogger, ("authorization config error", ""));
            return false;
        }
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

bool ScrapeConfig::InitBasicAuth(const Json::Value& basicAuth) {
    string username;
    string usernameFile;
    string password;
    string passwordFile;
    if (basicAuth.isMember(prometheus::USERNAME) && basicAuth[prometheus::USERNAME].isString()) {
        username = basicAuth[prometheus::USERNAME].asString();
    }
    if (basicAuth.isMember(prometheus::USERNAME_FILE) && basicAuth[prometheus::USERNAME_FILE].isString()) {
        usernameFile = basicAuth[prometheus::USERNAME_FILE].asString();
    }
    if (basicAuth.isMember(prometheus::PASSWORD) && basicAuth[prometheus::PASSWORD].isString()) {
        password = basicAuth[prometheus::PASSWORD].asString();
    }
    if (basicAuth.isMember(prometheus::PASSWORD_FILE) && basicAuth[prometheus::PASSWORD_FILE].isString()) {
        passwordFile = basicAuth[prometheus::PASSWORD_FILE].asString();
    }

    if ((username.empty() && usernameFile.empty()) || (password.empty() && passwordFile.empty())) {
        LOG_ERROR(sLogger, ("basic auth username or password is empty", ""));
        return false;
    }
    if ((!username.empty() && !usernameFile.empty()) || (!password.empty() && !passwordFile.empty())) {
        LOG_ERROR(sLogger, ("basic auth config error", ""));
        return false;
    }
    if (!usernameFile.empty() && !ReadFile(usernameFile, username)) {
        LOG_ERROR(sLogger, ("read username_file failed, username_file", usernameFile));
        return false;
    }

    if (!passwordFile.empty() && !ReadFile(passwordFile, password)) {
        LOG_ERROR(sLogger, ("read password_file failed, password_file", passwordFile));
        return false;
    }

    auto token = username + ":" + password;
    auto token64 = sdk::Base64Enconde(token);
    mAuthHeaders[prometheus::A_UTHORIZATION] = prometheus::BASIC_PREFIX + token64;
    return true;
}

bool ScrapeConfig::InitAuthorization(const Json::Value& authorization) {
    string type;
    string credentials;
    string credentialsFile;

    if (authorization.isMember(prometheus::TYPE) && authorization[prometheus::TYPE].isString()) {
        type = authorization[prometheus::TYPE].asString();
    }
    // if not set, use default type Bearer
    if (type.empty()) {
        type = prometheus::AUTHORIZATION_DEFAULT_TYEP;
    }

    if (authorization.isMember(prometheus::CREDENTIALS) && authorization[prometheus::CREDENTIALS].isString()) {
        credentials = authorization[prometheus::CREDENTIALS].asString();
    }
    if (authorization.isMember(prometheus::CREDENTIALS_FILE)
        && authorization[prometheus::CREDENTIALS_FILE].isString()) {
        credentialsFile = authorization[prometheus::CREDENTIALS_FILE].asString();
    }
    if (!credentials.empty() && !credentialsFile.empty()) {
        LOG_ERROR(sLogger, ("authorization config error", ""));
        return false;
    }

    if (!credentialsFile.empty() && !ReadFile(credentialsFile, credentials)) {
        LOG_ERROR(sLogger, ("authorization read file error", ""));
        return false;
    }

    mAuthHeaders[prometheus::A_UTHORIZATION] = type + " " + credentials;
    return true;
}

} // namespace logtail