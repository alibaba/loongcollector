
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
      mHonorLabels(false),
      mHonorTimestamps(true),
      mScheme("http"),
      mFollowRedirects(true),
      mEnableTLS(false),
      mMaxScrapeSizeBytes(0),
      mSampleLimit(0),
      mSeriesLimit(0) {
}

bool ScrapeConfig::Init(const Json::Value& scrapeConfig) {
    if (!InitStaticConfig(scrapeConfig)) {
        return false;
    }

    if (scrapeConfig.isMember(prom::SCRAPE_PROTOCOLS) && scrapeConfig[prom::SCRAPE_PROTOCOLS].isArray()) {
        if (!InitScrapeProtocols(scrapeConfig[prom::SCRAPE_PROTOCOLS])) {
            LOG_ERROR(sLogger, ("scrape protocol config error", scrapeConfig[prom::SCRAPE_PROTOCOLS]));
            return false;
        }
    } else {
        Json::Value nullJson;
        InitScrapeProtocols(nullJson);
    }

    if (scrapeConfig.isMember(prom::FOLLOW_REDIRECTS) && scrapeConfig[prom::FOLLOW_REDIRECTS].isBool()) {
        mFollowRedirects = scrapeConfig[prom::FOLLOW_REDIRECTS].asBool();
    }

    if (scrapeConfig.isMember(prom::TLS_CONFIG) && scrapeConfig[prom::TLS_CONFIG].isObject()) {
        if (!InitTLSConfig(scrapeConfig[prom::TLS_CONFIG])) {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prom::ENABLE_COMPRESSION)
        && scrapeConfig[prom::ENABLE_COMPRESSION].isBool()) {
        // InitEnableCompression(scrapeConfig[prom::ENABLE_COMPRESSION].asBool());
    } else {
        // InitEnableCompression(true);
    }

    // basic auth, authorization, oauth2
    // basic auth, authorization, oauth2 cannot be used at the same time
    if ((int)scrapeConfig.isMember(prom::BASIC_AUTH) + scrapeConfig.isMember(prom::AUTHORIZATION) > 1) {
        LOG_ERROR(sLogger, ("basic auth and authorization cannot be used at the same time", ""));
        return false;
    }
    if (scrapeConfig.isMember(prom::BASIC_AUTH) && scrapeConfig[prom::BASIC_AUTH].isObject()) {
        if (!InitBasicAuth(scrapeConfig[prom::BASIC_AUTH])) {
            LOG_ERROR(sLogger, ("basic auth config error", ""));
            return false;
        }
    }
    if (scrapeConfig.isMember(prom::AUTHORIZATION) && scrapeConfig[prom::AUTHORIZATION].isObject()) {
        if (!InitAuthorization(scrapeConfig[prom::AUTHORIZATION])) {
            LOG_ERROR(sLogger, ("authorization config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prom::PARAMS) && scrapeConfig[prom::PARAMS].isObject()) {
        const Json::Value& params = scrapeConfig[prom::PARAMS];
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

    // build query string
    for (auto& [key, values] : mParams) {
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

bool ScrapeConfig::InitStaticConfig(const Json::Value& scrapeConfig) {
    if (scrapeConfig.isMember(prom::JOB_NAME) && scrapeConfig[prom::JOB_NAME].isString()) {
        mJobName = scrapeConfig[prom::JOB_NAME].asString();
        if (mJobName.empty()) {
            LOG_ERROR(sLogger, ("job name is empty", ""));
            return false;
        }
    } else {
        return false;
    }

    if (scrapeConfig.isMember(prom::SCRAPE_INTERVAL) && scrapeConfig[prom::SCRAPE_INTERVAL].isString()) {
        string tmpScrapeIntervalString = scrapeConfig[prom::SCRAPE_INTERVAL].asString();
        mScrapeIntervalSeconds = DurationToSecond(tmpScrapeIntervalString);
        if (mScrapeIntervalSeconds == 0) {
            LOG_ERROR(sLogger, ("scrape interval is invalid", tmpScrapeIntervalString));
            return false;
        }
    }
    if (scrapeConfig.isMember(prom::SCRAPE_TIMEOUT) && scrapeConfig[prom::SCRAPE_TIMEOUT].isString()) {
        string tmpScrapeTimeoutString = scrapeConfig[prom::SCRAPE_TIMEOUT].asString();
        mScrapeTimeoutSeconds = DurationToSecond(tmpScrapeTimeoutString);
        if (mScrapeTimeoutSeconds == 0) {
            LOG_ERROR(sLogger, ("scrape timeout is invalid", tmpScrapeTimeoutString));
            return false;
        }
    }
    if (scrapeConfig.isMember(prom::METRICS_PATH) && scrapeConfig[prom::METRICS_PATH].isString()) {
        mMetricsPath = scrapeConfig[prom::METRICS_PATH].asString();
    }

    if (scrapeConfig.isMember(prom::HONOR_LABELS) && scrapeConfig[prom::HONOR_LABELS].isBool()) {
        mHonorLabels = scrapeConfig[prom::HONOR_LABELS].asBool();
    }

    if (scrapeConfig.isMember(prom::HONOR_TIMESTAMPS) && scrapeConfig[prom::HONOR_TIMESTAMPS].isBool()) {
        mHonorTimestamps = scrapeConfig[prom::HONOR_TIMESTAMPS].asBool();
    }

    if (scrapeConfig.isMember(prom::SCHEME) && scrapeConfig[prom::SCHEME].isString()) {
        mScheme = scrapeConfig[prom::SCHEME].asString();
    }

    // <size>: a size in bytes, e.g. 512MB. A unit is required. Supported units: B, KB, MB, GB, TB, PB, EB.
    if (scrapeConfig.isMember(prom::MAX_SCRAPE_SIZE) && scrapeConfig[prom::MAX_SCRAPE_SIZE].isString()) {
        string tmpMaxScrapeSize = scrapeConfig[prom::MAX_SCRAPE_SIZE].asString();
        mMaxScrapeSizeBytes = SizeToByte(tmpMaxScrapeSize);
        if (mMaxScrapeSizeBytes == 0) {
            LOG_ERROR(sLogger, ("max scrape size is invalid", tmpMaxScrapeSize));
            return false;
        }
    }

    if (scrapeConfig.isMember(prom::SAMPLE_LIMIT) && scrapeConfig[prom::SAMPLE_LIMIT].isInt64()) {
        mSampleLimit = scrapeConfig[prom::SAMPLE_LIMIT].asUInt64();
    }
    if (scrapeConfig.isMember(prom::SERIES_LIMIT) && scrapeConfig[prom::SERIES_LIMIT].isInt64()) {
        mSeriesLimit = scrapeConfig[prom::SERIES_LIMIT].asUInt64();
    }

    if (scrapeConfig.isMember(prom::RELABEL_CONFIGS)) {
        if (!mRelabelConfigs.Init(scrapeConfig[prom::RELABEL_CONFIGS])) {
            LOG_ERROR(sLogger, ("relabel config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prom::METRIC_RELABEL_CONFIGS)) {
        if (!mMetricRelabelConfigs.Init(scrapeConfig[prom::METRIC_RELABEL_CONFIGS])) {
            LOG_ERROR(sLogger, ("metric relabel config error", ""));
            return false;
        }
    }
    return true;
}

bool ScrapeConfig::InitBasicAuth(const Json::Value& basicAuth) {
    string username;
    string usernameFile;
    string password;
    string passwordFile;
    if (basicAuth.isMember(prom::USERNAME) && basicAuth[prom::USERNAME].isString()) {
        username = basicAuth[prom::USERNAME].asString();
    }
    if (basicAuth.isMember(prom::USERNAME_FILE) && basicAuth[prom::USERNAME_FILE].isString()) {
        usernameFile = basicAuth[prom::USERNAME_FILE].asString();
    }
    if (basicAuth.isMember(prom::PASSWORD) && basicAuth[prom::PASSWORD].isString()) {
        password = basicAuth[prom::PASSWORD].asString();
    }
    if (basicAuth.isMember(prom::PASSWORD_FILE) && basicAuth[prom::PASSWORD_FILE].isString()) {
        passwordFile = basicAuth[prom::PASSWORD_FILE].asString();
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
    mRequestHeaders[prom::A_UTHORIZATION] = prom::BASIC_PREFIX + token64;
    return true;
}

bool ScrapeConfig::InitAuthorization(const Json::Value& authorization) {
    string type;
    string credentials;
    string credentialsFile;

    if (authorization.isMember(prom::TYPE) && authorization[prom::TYPE].isString()) {
        type = authorization[prom::TYPE].asString();
    }
    // if not set, use default type Bearer
    if (type.empty()) {
        type = prom::AUTHORIZATION_DEFAULT_TYEP;
    }

    if (authorization.isMember(prom::CREDENTIALS) && authorization[prom::CREDENTIALS].isString()) {
        credentials = authorization[prom::CREDENTIALS].asString();
    }
    if (authorization.isMember(prom::CREDENTIALS_FILE)
        && authorization[prom::CREDENTIALS_FILE].isString()) {
        credentialsFile = authorization[prom::CREDENTIALS_FILE].asString();
    }
    if (!credentials.empty() && !credentialsFile.empty()) {
        LOG_ERROR(sLogger, ("authorization config error", ""));
        return false;
    }

    if (!credentialsFile.empty() && !ReadFile(credentialsFile, credentials)) {
        LOG_ERROR(sLogger, ("authorization read file error", ""));
        return false;
    }

    mRequestHeaders[prom::A_UTHORIZATION] = type + " " + credentials;
    return true;
}

bool ScrapeConfig::InitScrapeProtocols(const Json::Value& scrapeProtocols) {
    static auto sScrapeProtocolsHeaders = std::map<string, string>{
        {prom::PrometheusProto,
         "application/vnd.google.protobuf;proto=io.prometheus.client.MetricFamily;encoding=delimited"},
        {prom::PrometheusText0_0_4, "text/plain;version=0.0.4"},
        {prom::OpenMetricsText0_0_1, "application/openmetrics-text;version=0.0.1"},
        {prom::OpenMetricsText1_0_0, "application/openmetrics-text;version=1.0.0"},
    };
    static auto sDefaultScrapeProtocols = vector<string>{
        prom::PrometheusText0_0_4,
        prom::PrometheusProto,
        prom::OpenMetricsText0_0_1,
        prom::OpenMetricsText1_0_0,
    };

    auto join = [](const vector<string>& strs, const string& sep) {
        string result;
        for (const auto& str : strs) {
            if (!result.empty()) {
                result += sep;
            }
            result += str;
        }
        return result;
    };

    auto getScrapeProtocols = [](const Json::Value& scrapeProtocols, vector<string>& res) {
        for (const auto& scrapeProtocol : scrapeProtocols) {
            if (scrapeProtocol.isString()) {
                res.push_back(scrapeProtocol.asString());
            } else {
                LOG_ERROR(sLogger, ("scrape_protocols config error", ""));
                return false;
            }
        }
        return true;
    };

    auto validateScrapeProtocols = [](const vector<string>& scrapeProtocols) {
        set<string> dups;
        for (const auto& scrapeProtocol : scrapeProtocols) {
            if (!sScrapeProtocolsHeaders.count(scrapeProtocol)) {
                LOG_ERROR(sLogger,
                          ("unknown scrape protocol prometheusproto", scrapeProtocol)(
                              "supported",
                              "[OpenMetricsText0.0.1 OpenMetricsText1.0.0 PrometheusProto PrometheusText0.0.4]"));
                return false;
            }
            if (dups.count(scrapeProtocol)) {
                LOG_ERROR(sLogger, ("duplicated protocol in scrape_protocols", scrapeProtocol));
                return false;
            }
            dups.insert(scrapeProtocol);
        }
        return true;
    };

    vector<string> tmpScrapeProtocols;

    if (!getScrapeProtocols(scrapeProtocols, tmpScrapeProtocols)) {
        return false;
    }

    // if scrape_protocols is empty, use default protocols
    if (tmpScrapeProtocols.empty()) {
        tmpScrapeProtocols = sDefaultScrapeProtocols;
    }
    if (!validateScrapeProtocols(tmpScrapeProtocols)) {
        return false;
    }

    auto weight = tmpScrapeProtocols.size() + 1;
    for (auto& tmpScrapeProtocol : tmpScrapeProtocols) {
        auto val = sScrapeProtocolsHeaders[tmpScrapeProtocol];
        val += ";q=0." + std::to_string(weight--);
        tmpScrapeProtocol = val;
    }
    tmpScrapeProtocols.push_back("*/*;q=0." + ToString(weight));
    mRequestHeaders[prom::ACCEPT] = join(tmpScrapeProtocols, ",");
    return true;
}

void ScrapeConfig::InitEnableCompression(bool enableCompression) {
    if (enableCompression) {
        mRequestHeaders[prom::ACCEPT_ENCODING] = prom::GZIP;
    } else {
        mRequestHeaders[prom::ACCEPT_ENCODING] = prom::IDENTITY;
    }
}

bool ScrapeConfig::InitTLSConfig(const Json::Value& tlsConfig) {
    if (tlsConfig.isMember(prom::CA_FILE)) {
        if (tlsConfig[prom::CA_FILE].isString()) {
            mTLS.mCaFile = tlsConfig[prom::CA_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prom::CERT_FILE)) {
        if (tlsConfig[prom::CERT_FILE].isString()) {
            mTLS.mCertFile = tlsConfig[prom::CERT_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prom::KEY_FILE)) {
        if (tlsConfig[prom::KEY_FILE].isString()) {
            mTLS.mKeyFile = tlsConfig[prom::KEY_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prom::SERVER_NAME)) {
        if (tlsConfig[prom::SERVER_NAME].isString()) {
            mRequestHeaders[prom::HOST] = tlsConfig[prom::SERVER_NAME].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prom::INSECURE_SKIP_VERIFY)) {
        if (tlsConfig[prom::INSECURE_SKIP_VERIFY].isBool()) {
            mTLS.mInsecureSkipVerify = tlsConfig[prom::INSECURE_SKIP_VERIFY].asBool();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    mEnableTLS = true;
    return true;
}

} // namespace logtail