
#include "prometheus/schedulers/ScrapeConfig.h"

#include <chrono>
#include <mutex>
#include <string>

#include "json/value.h"

#include "common/EncodingUtil.h"
#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "logger/Logger.h"
#include "prometheus/Constants.h"
#include "prometheus/Utils.h"

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

    if (scrapeConfig.isMember(prometheus::SCRAPE_PROTOCOLS) && scrapeConfig[prometheus::SCRAPE_PROTOCOLS].isArray()) {
        if (!InitScrapeProtocols(scrapeConfig[prometheus::SCRAPE_PROTOCOLS])) {
            LOG_ERROR(sLogger, ("scrape protocol config error", scrapeConfig[prometheus::SCRAPE_PROTOCOLS]));
            return false;
        }
    } else {
        Json::Value nullJson;
        InitScrapeProtocols(nullJson);
    }

    if (scrapeConfig.isMember(prometheus::FOLLOW_REDIRECTS) && scrapeConfig[prometheus::FOLLOW_REDIRECTS].isBool()) {
        mFollowRedirects = scrapeConfig[prometheus::FOLLOW_REDIRECTS].asBool();
    }

    if (scrapeConfig.isMember(prometheus::TLS_CONFIG) && scrapeConfig[prometheus::TLS_CONFIG].isObject()) {
        if (!InitTLSConfig(scrapeConfig[prometheus::TLS_CONFIG])) {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prometheus::ENABLE_COMPRESSION)
        && scrapeConfig[prometheus::ENABLE_COMPRESSION].isBool()) {
        // InitEnableCompression(scrapeConfig[prometheus::ENABLE_COMPRESSION].asBool());
    } else {
        // InitEnableCompression(true);
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

    if (scrapeConfig.isMember(prometheus::PARAMS) && scrapeConfig[prometheus::PARAMS].isObject()) {
        const Json::Value& params = scrapeConfig[prometheus::PARAMS];
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
        if (mScrapeIntervalSeconds == 0) {
            LOG_ERROR(sLogger, ("scrape interval is invalid", tmpScrapeIntervalString));
            return false;
        }
    }
    if (scrapeConfig.isMember(prometheus::SCRAPE_TIMEOUT) && scrapeConfig[prometheus::SCRAPE_TIMEOUT].isString()) {
        string tmpScrapeTimeoutString = scrapeConfig[prometheus::SCRAPE_TIMEOUT].asString();
        mScrapeTimeoutSeconds = DurationToSecond(tmpScrapeTimeoutString);
        if (mScrapeTimeoutSeconds == 0) {
            LOG_ERROR(sLogger, ("scrape timeout is invalid", tmpScrapeTimeoutString));
            return false;
        }
    }
    if (scrapeConfig.isMember(prometheus::METRICS_PATH) && scrapeConfig[prometheus::METRICS_PATH].isString()) {
        mMetricsPath = scrapeConfig[prometheus::METRICS_PATH].asString();
    }

    if (scrapeConfig.isMember(prometheus::HONOR_LABELS) && scrapeConfig[prometheus::HONOR_LABELS].isBool()) {
        mHonorLabels = scrapeConfig[prometheus::HONOR_LABELS].asBool();
    }

    if (scrapeConfig.isMember(prometheus::HONOR_TIMESTAMPS) && scrapeConfig[prometheus::HONOR_TIMESTAMPS].isBool()) {
        mHonorTimestamps = scrapeConfig[prometheus::HONOR_TIMESTAMPS].asBool();
    }

    if (scrapeConfig.isMember(prometheus::SCHEME) && scrapeConfig[prometheus::SCHEME].isString()) {
        mScheme = scrapeConfig[prometheus::SCHEME].asString();
    }

    // <size>: a size in bytes, e.g. 512MB. A unit is required. Supported units: B, KB, MB, GB, TB, PB, EB.
    if (scrapeConfig.isMember(prometheus::MAX_SCRAPE_SIZE) && scrapeConfig[prometheus::MAX_SCRAPE_SIZE].isString()) {
        string tmpMaxScrapeSize = scrapeConfig[prometheus::MAX_SCRAPE_SIZE].asString();
        mMaxScrapeSizeBytes = SizeToByte(tmpMaxScrapeSize);
        if (mMaxScrapeSizeBytes == 0) {
            LOG_ERROR(sLogger, ("max scrape size is invalid", tmpMaxScrapeSize));
            return false;
        }
    }

    if (scrapeConfig.isMember(prometheus::SAMPLE_LIMIT) && scrapeConfig[prometheus::SAMPLE_LIMIT].isInt64()) {
        mSampleLimit = scrapeConfig[prometheus::SAMPLE_LIMIT].asUInt64();
    }
    if (scrapeConfig.isMember(prometheus::SERIES_LIMIT) && scrapeConfig[prometheus::SERIES_LIMIT].isInt64()) {
        mSeriesLimit = scrapeConfig[prometheus::SERIES_LIMIT].asUInt64();
    }

    if (scrapeConfig.isMember(prometheus::RELABEL_CONFIGS)) {
        if (!mRelabelConfigs.Init(scrapeConfig[prometheus::RELABEL_CONFIGS])) {
            LOG_ERROR(sLogger, ("relabel config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prometheus::METRIC_RELABEL_CONFIGS)) {
        if (!mMetricRelabelConfigs.Init(scrapeConfig[prometheus::METRIC_RELABEL_CONFIGS])) {
            LOG_ERROR(sLogger, ("metric relabel config error", ""));
            return false;
        }
    }

    if (scrapeConfig.isMember(prometheus::EXTERNAL_LABELS)) {
        if (!InitExternalLabels(scrapeConfig[prometheus::EXTERNAL_LABELS])) {
            LOG_ERROR(sLogger, ("external labels config error", ""));
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
    auto token64 = Base64Encode(token);
    mRequestHeaders[prometheus::A_UTHORIZATION] = prometheus::BASIC_PREFIX + token64;

    {
        lock_guard<mutex> lock(mAuthMutex);
        mAuthType = prometheus::BASIC_PREFIX;
        mBasicNamePath = usernameFile;
        mBasicPasswordPath = passwordFile;
    }

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
        LOG_ERROR(sLogger, ("authorization read file error", mBearerTokenPath));
        return false;
    }

    {
        lock_guard<mutex> lock(mAuthMutex);
        mAuthType = type;
        mBearerTokenPath = credentialsFile;
    }

    mRequestHeaders[prometheus::A_UTHORIZATION] = type + " " + credentials;
    return true;
}

// the return value is true if the authorization is updated
bool ScrapeConfig::UpdateAuthorization() {
    lock_guard<mutex> lock(mAuthMutex);
    auto currTime = chrono::system_clock::now();
    if (mAuthType.empty() || chrono::duration_cast<chrono::minutes>(currTime - mLastUpdateTime).count() < 5) {
        return false;
    }
    mLastUpdateTime = currTime;
    LOG_INFO(sLogger, (mJobName, "starte update authorization"));

    string credentials;
    if (mAuthType == prometheus::BASIC_PREFIX) {
        if (mBasicNamePath.empty() || mBasicPasswordPath.empty()) {
            return false;
        }
        string username;
        string password;
        if (!ReadFile(mBasicNamePath, username)) {
            LOG_ERROR(sLogger, ("read username_file failed, username_file", mBasicNamePath));
            return false;
        }

        if (!ReadFile(mBasicPasswordPath, password)) {
            LOG_ERROR(sLogger, ("read password_file failed, password_file", mBasicPasswordPath));
            return false;
        }
        credentials = prometheus::BASIC_PREFIX + Base64Encode(username + ":" + password);
    } else {
        if (mBearerTokenPath.empty()) {
            return false;
        }
        if (!ReadFile(mBearerTokenPath, credentials)) {
            LOG_ERROR(sLogger, ("authorization read file error", mBearerTokenPath));
            return false;
        }
        credentials = mAuthType + " " + credentials;
    }
    if (credentials.empty() || credentials == mRequestHeaders[prometheus::A_UTHORIZATION]) {
        return false;
    }
    mRequestHeaders[prometheus::A_UTHORIZATION] = credentials;
    LOG_INFO(sLogger, (mJobName, "authorization updated"));
    return true;
}

bool ScrapeConfig::InitScrapeProtocols(const Json::Value& scrapeProtocols) {
    static auto sScrapeProtocolsHeaders = std::map<string, string>{
        {prometheus::PrometheusProto,
         "application/vnd.google.protobuf;proto=io.prometheus.client.MetricFamily;encoding=delimited"},
        {prometheus::PrometheusText0_0_4, "text/plain;version=0.0.4"},
        {prometheus::OpenMetricsText0_0_1, "application/openmetrics-text;version=0.0.1"},
        {prometheus::OpenMetricsText1_0_0, "application/openmetrics-text;version=1.0.0"},
    };
    static auto sDefaultScrapeProtocols = vector<string>{
        prometheus::PrometheusText0_0_4,
        prometheus::PrometheusProto,
        prometheus::OpenMetricsText0_0_1,
        prometheus::OpenMetricsText1_0_0,
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
    mRequestHeaders[prometheus::ACCEPT] = join(tmpScrapeProtocols, ",");
    return true;
}

void ScrapeConfig::InitEnableCompression(bool enableCompression) {
    if (enableCompression) {
        mRequestHeaders[prometheus::ACCEPT_ENCODING] = prometheus::GZIP;
    } else {
        mRequestHeaders[prometheus::ACCEPT_ENCODING] = prometheus::IDENTITY;
    }
}

bool ScrapeConfig::InitTLSConfig(const Json::Value& tlsConfig) {
    if (tlsConfig.isMember(prometheus::CA_FILE)) {
        if (tlsConfig[prometheus::CA_FILE].isString()) {
            mTLS.mCaFile = tlsConfig[prometheus::CA_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prometheus::CERT_FILE)) {
        if (tlsConfig[prometheus::CERT_FILE].isString()) {
            mTLS.mCertFile = tlsConfig[prometheus::CERT_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prometheus::KEY_FILE)) {
        if (tlsConfig[prometheus::KEY_FILE].isString()) {
            mTLS.mKeyFile = tlsConfig[prometheus::KEY_FILE].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prometheus::SERVER_NAME)) {
        if (tlsConfig[prometheus::SERVER_NAME].isString()) {
            mRequestHeaders[prometheus::HOST] = tlsConfig[prometheus::SERVER_NAME].asString();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    if (tlsConfig.isMember(prometheus::INSECURE_SKIP_VERIFY)) {
        if (tlsConfig[prometheus::INSECURE_SKIP_VERIFY].isBool()) {
            mTLS.mInsecureSkipVerify = tlsConfig[prometheus::INSECURE_SKIP_VERIFY].asBool();
        } else {
            LOG_ERROR(sLogger, ("tls config error", ""));
            return false;
        }
    }
    mEnableTLS = true;
    return true;
}

bool ScrapeConfig::InitExternalLabels(const Json::Value& externalLabels) {
    if (!externalLabels.isObject()) {
        LOG_ERROR(sLogger, ("external_labels config error", ""));
        return false;
    }
    set<string> dups;
    for (auto& key : externalLabels.getMemberNames()) {
        if (externalLabels[key].isString()) {
            if (dups.find(key) != dups.end()) {
                LOG_ERROR(sLogger, ("duplicated key in external_labels", key));
                return false;
            }
            dups.insert(key);
            mExternalLabels.emplace_back(key, externalLabels[key].asString());
        } else {
            LOG_ERROR(sLogger, ("external_labels config error", ""));
            return false;
        }
    }
    std::sort(mExternalLabels.begin(), mExternalLabels.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.first < rhs.first;
    });
    return true;
}

} // namespace logtail
