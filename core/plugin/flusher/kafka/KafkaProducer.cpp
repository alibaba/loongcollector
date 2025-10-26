/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "plugin/flusher/kafka/KafkaProducer.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "common/StringTools.h"
#include "logger/Logger.h"
#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaConstant.h"
#include "plugin/flusher/kafka/KafkaUtil.h"

namespace logtail {

namespace {

struct ProducerContext {
    KafkaProducer::Callback callback;
    KafkaProducer::ErrorInfo errorInfo;
};

} // namespace

KafkaProducer::ErrorType KafkaProducer::MapKafkaError(rd_kafka_resp_err_t err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR_NO_ERROR:
            return KafkaProducer::ErrorType::SUCCESS;

        case RD_KAFKA_RESP_ERR__QUEUE_FULL:
            return KafkaProducer::ErrorType::QUEUE_FULL;

        case RD_KAFKA_RESP_ERR__AUTHENTICATION:
        case RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED:
        case RD_KAFKA_RESP_ERR_GROUP_AUTHORIZATION_FAILED:
            return KafkaProducer::ErrorType::AUTH_ERROR;

        case RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE:
        case RD_KAFKA_RESP_ERR__INVALID_ARG:
            return KafkaProducer::ErrorType::PARAMS_ERROR;

        case RD_KAFKA_RESP_ERR__TRANSPORT:
        case RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN:
        case RD_KAFKA_RESP_ERR__DESTROY:
        case RD_KAFKA_RESP_ERR__TIMED_OUT:
            return KafkaProducer::ErrorType::NETWORK_ERROR;

        case RD_KAFKA_RESP_ERR_BROKER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION:
            return KafkaProducer::ErrorType::SERVER_ERROR;

        default:
            return KafkaProducer::ErrorType::OTHER_ERROR;
    }
}

struct KafkaProducer::HeadersTemplate {
    rd_kafka_headers_t* hdrs = nullptr;
};

class KafkaProducer::Impl {
public:
    Impl() : mProducer(nullptr), mConf(nullptr), mIsRunning(false), mIsClosed(false) {}
    ~Impl() {
        if (!mIsClosed) {
            Close();
        }
        for (auto& ctx : mContextPool) {
            delete ctx;
        }
    }

    ProducerContext* GetContext() {
        std::lock_guard<std::mutex> lock(mContextPoolMutex);
        if (mContextPool.empty()) {
            return new ProducerContext();
        }
        auto* ctx = mContextPool.back();
        mContextPool.pop_back();
        return ctx;
    }

    void ReleaseContext(ProducerContext* ctx) {
        ctx->callback = nullptr;
        ctx->errorInfo = {KafkaProducer::ErrorType::SUCCESS, "", 0};
        std::lock_guard<std::mutex> lock(mContextPoolMutex);
        if (mContextPool.size() < kMaxContextCache) {
            mContextPool.push_back(ctx);
        } else {
            delete ctx;
        }
    }

    bool Init(const KafkaConfig& config) {
        mConfig = config;

        mConf = rd_kafka_conf_new();
        if (!mConf) {
            return false;
        }

        if (!InitBasicConfig()) {
            return false;
        }

        if (!InitDeliveryConfig()) {
            return false;
        }

        if (!InitVersionConfig()) {
            return false;
        }

        if (!InitPartitionerConfig()) {
            return false;
        }

        if (!InitSecurityProtocol()) {
            return false;
        }

        if (!InitTlsConfig()) {
            return false;
        }

        if (!InitKerberosConfig()) {
            return false;
        }

        if (!InitSaslConfig()) {
            return false;
        }

        if (!InitCustomConfig()) {
            return false;
        }

        return InitProducer();
    }

    KafkaProducer::HeadersTemplate*
    CreateHeadersTemplate(const std::vector<std::pair<std::string, std::string>>& headers) {
        auto* tpl = new KafkaProducer::HeadersTemplate();
        if (!headers.empty()) {
            tpl->hdrs = rd_kafka_headers_new(static_cast<int>(headers.size()));
            if (!tpl->hdrs) {
                LOG_ERROR(sLogger, ("error", "Failed to allocate Kafka headers"));
                delete tpl;
                return nullptr;
            }
            for (const auto& kv : headers) {
                rd_kafka_resp_err_t err = rd_kafka_header_add(tpl->hdrs,
                                                              kv.first.c_str(),
                                                              static_cast<int>(kv.first.size()),
                                                              kv.second.data(),
                                                              static_cast<int>(kv.second.size()));
                if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
                    LOG_ERROR(sLogger,
                              ("Failed to add Kafka header",
                               rd_kafka_err2str(err))("header_key", kv.first)("header_value", kv.second));
                    rd_kafka_headers_destroy(tpl->hdrs);
                    delete tpl;
                    return nullptr;
                }
            }
        }
        return tpl;
    }

    void DestroyHeadersTemplate(KafkaProducer::HeadersTemplate* tpl) {
        if (!tpl)
            return;
        if (tpl->hdrs)
            rd_kafka_headers_destroy(tpl->hdrs);
        delete tpl;
    }

    void ProduceAsync(const std::string& topic,
                      std::string&& value,
                      KafkaProducer::Callback callback,
                      const std::string& key,
                      KafkaProducer::HeadersTemplate* headersTemplate) {
        rd_kafka_t* producer = nullptr;
        {
            std::lock_guard<std::mutex> lock(mProducerMutex);
            producer = mProducer;
        }
        if (!producer) {
            KafkaProducer::ErrorInfo errorInfo;
            errorInfo.type = KafkaProducer::ErrorType::OTHER_ERROR;
            errorInfo.message = "producer not initialized";
            errorInfo.code = 0;
            callback(false, errorInfo);
            return;
        }

        auto* context = GetContext();
        context->callback = std::move(callback);

        rd_kafka_headers_t* rdk_headers = nullptr;
        if (headersTemplate != nullptr && headersTemplate->hdrs != nullptr) {
            rdk_headers = rd_kafka_headers_copy(headersTemplate->hdrs);
        }

        rd_kafka_resp_err_t err;
        if (rdk_headers && !key.empty()) {
            err = rd_kafka_producev(producer,
                                    RD_KAFKA_V_TOPIC(topic.c_str()),
                                    RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                    RD_KAFKA_V_KEY(key.data(), key.size()),
                                    RD_KAFKA_V_VALUE(value.data(), value.size()),
                                    RD_KAFKA_V_HEADERS(rdk_headers),
                                    RD_KAFKA_V_OPAQUE(context),
                                    RD_KAFKA_V_END);
        } else if (rdk_headers) {
            err = rd_kafka_producev(producer,
                                    RD_KAFKA_V_TOPIC(topic.c_str()),
                                    RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                    RD_KAFKA_V_VALUE(value.data(), value.size()),
                                    RD_KAFKA_V_HEADERS(rdk_headers),
                                    RD_KAFKA_V_OPAQUE(context),
                                    RD_KAFKA_V_END);
        } else if (!key.empty()) {
            err = rd_kafka_producev(producer,
                                    RD_KAFKA_V_TOPIC(topic.c_str()),
                                    RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                    RD_KAFKA_V_KEY(key.data(), key.size()),
                                    RD_KAFKA_V_VALUE(value.data(), value.size()),
                                    RD_KAFKA_V_OPAQUE(context),
                                    RD_KAFKA_V_END);
        } else {
            err = rd_kafka_producev(producer,
                                    RD_KAFKA_V_TOPIC(topic.c_str()),
                                    RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                    RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                                    RD_KAFKA_V_VALUE(value.data(), value.size()),
                                    RD_KAFKA_V_OPAQUE(context),
                                    RD_KAFKA_V_END);
        }

        if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
            LOG_ERROR(sLogger,
                      ("rd_kafka_producev error", rd_kafka_err2str(err))("code", static_cast<int>(err))("topic", topic)(
                          "value_size", value.size()));
            if (rdk_headers) {
                rd_kafka_headers_destroy(rdk_headers);
            }
            KafkaProducer::ErrorInfo errorInfo;
            errorInfo.type = KafkaProducer::MapKafkaError(err);
            errorInfo.message = rd_kafka_err2str(err);
            errorInfo.code = static_cast<int>(err);
            if (context->callback) {
                context->callback(false, errorInfo);
            }
            ReleaseContext(context);
        }
    }

    bool Flush(int timeoutMs) {
        if (!mProducer) {
            return false;
        }

        rd_kafka_resp_err_t result = rd_kafka_flush(mProducer, timeoutMs);
        return result == RD_KAFKA_RESP_ERR_NO_ERROR;
    }

    void Close() {
        if (mIsClosed) {
            return;
        }

        mIsRunning = false;
        if (mPollThread.joinable()) {
            mPollThread.join();
        }

        std::lock_guard<std::mutex> lock(mProducerMutex);
        if (mProducer) {
            rd_kafka_flush(mProducer, 3000);
            rd_kafka_destroy(mProducer);
            mProducer = nullptr;
        }

        if (mConf) {
            rd_kafka_conf_destroy(mConf);
            mConf = nullptr;
        }

        mIsClosed = true;
    }

private:
    bool SetConfig(const std::string& key, const std::string& value) {
        char errstr[512];
        if (rd_kafka_conf_set(mConf, key.c_str(), value.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            LOG_ERROR(sLogger, ("Failed to set Kafka config", key)("value", value)("error", errstr));
            return false;
        }
        return true;
    }

    bool InitBasicConfig() {
        std::string brokersStr = KafkaUtil::BrokersToString(mConfig.Brokers);
        if (brokersStr.empty()) {
            return false;
        }
        if (!SetConfig(KAFKA_CONFIG_BOOTSTRAP_SERVERS, brokersStr)) {
            return false;
        }
        LOG_INFO(sLogger, ("Kafka bootstrap.servers", brokersStr));

        if (!SetConfig(KAFKA_CONFIG_BATCH_NUM_MESSAGES, std::to_string(mConfig.BulkMaxSize))
            || !SetConfig(KAFKA_CONFIG_LINGER_MS, std::to_string(mConfig.BulkFlushFrequency))
            || !SetConfig(KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES, std::to_string(mConfig.QueueBufferingMaxKbytes))
            || !SetConfig(KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES, std::to_string(mConfig.QueueBufferingMaxMessages))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_MAX_BYTES, std::to_string(mConfig.MaxMessageBytes))) {
            return false;
        }

        return true;
    }

    bool InitDeliveryConfig() {
        std::string acksStr = (mConfig.RequiredAcks < 0) ? "all" : std::to_string(mConfig.RequiredAcks);
        if (!SetConfig(KAFKA_CONFIG_ACKS, acksStr)
            || !SetConfig(KAFKA_CONFIG_REQUEST_TIMEOUT_MS, std::to_string(mConfig.Timeout))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_TIMEOUT_MS, std::to_string(mConfig.MessageTimeoutMs))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES, std::to_string(mConfig.MaxRetries))
            || !SetConfig(KAFKA_CONFIG_RETRY_BACKOFF_MS, std::to_string(mConfig.RetryBackoffMs))) {
            return false;
        }
        LOG_INFO(sLogger,
                 ("Kafka delivery configs", "")("acks", acksStr)("request.timeout.ms", mConfig.Timeout)(
                     "message.timeout.ms", mConfig.MessageTimeoutMs)("message.send.max.retries", mConfig.MaxRetries)(
                     "retry.backoff.ms", mConfig.RetryBackoffMs));

        return true;
    }

    bool InitVersionConfig() {
        std::map<std::string, std::string> derivedConfigs;
        KafkaUtil::DeriveApiVersionConfigs(mConfig.Version, derivedConfigs);
        if (!derivedConfigs.empty()) {
            LOG_INFO(sLogger,
                     ("Apply Version derived configs",
                      "")(KAFKA_CONFIG_API_VERSION_REQUEST,
                          derivedConfigs.count(KAFKA_CONFIG_API_VERSION_REQUEST)
                              ? derivedConfigs[KAFKA_CONFIG_API_VERSION_REQUEST]
                              : "<unset>")(KAFKA_CONFIG_BROKER_VERSION_FALLBACK,
                                           derivedConfigs.count(KAFKA_CONFIG_BROKER_VERSION_FALLBACK)
                                               ? derivedConfigs[KAFKA_CONFIG_BROKER_VERSION_FALLBACK]
                                               : "<unset>")(KAFKA_CONFIG_API_VERSION_FALLBACK_MS,
                                                            derivedConfigs.count(KAFKA_CONFIG_API_VERSION_FALLBACK_MS)
                                                                ? derivedConfigs[KAFKA_CONFIG_API_VERSION_FALLBACK_MS]
                                                                : "<unset>"));
            for (const auto& kv : derivedConfigs) {
                if (!SetConfig(kv.first, kv.second)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool InitPartitionerConfig() {
        if (mConfig.Partitioner.empty()) {
            return true;
        }

        rd_kafka_topic_conf_t* tconf = rd_kafka_topic_conf_new();
        if (!tconf) {
            return false;
        }
        char errstr2[512];
        if (rd_kafka_topic_conf_set(
                tconf, KAFKA_CONFIG_PARTITIONER.c_str(), mConfig.Partitioner.c_str(), errstr2, sizeof(errstr2))
            != RD_KAFKA_CONF_OK) {
            LOG_ERROR(sLogger,
                      ("Failed to set Kafka topic config",
                       KAFKA_CONFIG_PARTITIONER)("value", mConfig.Partitioner)("error", errstr2));
            rd_kafka_topic_conf_destroy(tconf);
            return false;
        }
        rd_kafka_conf_set_default_topic_conf(mConf, tconf);

        return true;
    }

    bool InitSecurityProtocol() {
        const bool enableTLS = mConfig.Authentication.TlsEnabled;
        const bool enableKerberos = mConfig.Authentication.KerberosEnabled;
        const bool enableSASL = !mConfig.Authentication.SaslMechanism.empty();

        std::string securityProtocol;
        if (enableKerberos || enableSASL) {
            securityProtocol = enableTLS ? "sasl_ssl" : "sasl_plaintext";
        } else if (enableTLS) {
            securityProtocol = "ssl";
        }

        if (!securityProtocol.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SECURITY_PROTOCOL, securityProtocol)) {
                return false;
            }
        }

        return true;
    }

    bool InitTlsConfig() {
        if (!mConfig.Authentication.TlsEnabled) {
            return true;
        }

        if (!mConfig.Authentication.TlsCaFile.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SSL_CA_LOCATION, mConfig.Authentication.TlsCaFile)) {
                return false;
            }
        }

        const bool hasCert = !mConfig.Authentication.TlsCertFile.empty();
        const bool hasKey = !mConfig.Authentication.TlsKeyFile.empty();
        if (hasCert != hasKey) {
            LOG_ERROR(sLogger,
                      ("Kafka TLS client auth config error",
                       "both TLS.CertFile and TLS.KeyFile must be set together, or both unset"));
            return false;
        }

        if (hasCert) {
            if (!SetConfig(KAFKA_CONFIG_SSL_CERTIFICATE_LOCATION, mConfig.Authentication.TlsCertFile)) {
                return false;
            }
            if (!SetConfig(KAFKA_CONFIG_SSL_KEY_LOCATION, mConfig.Authentication.TlsKeyFile)) {
                return false;
            }
            if (!mConfig.Authentication.TlsKeyPassword.empty()) {
                if (!SetConfig(KAFKA_CONFIG_SSL_KEY_PASSWORD, mConfig.Authentication.TlsKeyPassword)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool InitKerberosConfig() {
        if (!mConfig.Authentication.KerberosEnabled) {
            return true;
        }

        const std::string mechanism = mConfig.Authentication.KerberosMechanisms.empty()
            ? std::string("GSSAPI")
            : mConfig.Authentication.KerberosMechanisms;
        if (!SetConfig(KAFKA_CONFIG_SASL_MECHANISMS, mechanism)) {
            return false;
        }

        if (!mConfig.Authentication.KerberosServiceName.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SASL_KERBEROS_SERVICE_NAME, mConfig.Authentication.KerberosServiceName)) {
                return false;
            }
        }

        if (!mConfig.Authentication.KerberosKinitCmd.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SASL_KERBEROS_KINIT_CMD, mConfig.Authentication.KerberosKinitCmd)) {
                return false;
            }
        }

        if (!mConfig.Authentication.KerberosPrincipal.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SASL_KERBEROS_PRINCIPAL, mConfig.Authentication.KerberosPrincipal)) {
                return false;
            }
        }

        if (!mConfig.Authentication.KerberosKeytab.empty()) {
            if (!SetConfig(KAFKA_CONFIG_SASL_KERBEROS_KEYTAB, mConfig.Authentication.KerberosKeytab)) {
                return false;
            }
        }

        return true;
    }

    bool InitSaslConfig() {
        if (mConfig.Authentication.KerberosEnabled) {
            return true;
        }

        if (mConfig.Authentication.SaslMechanism.empty()) {
            return true;
        }

        if (!SetConfig(KAFKA_CONFIG_SASL_MECHANISMS, mConfig.Authentication.SaslMechanism)) {
            return false;
        }

        if (!SetConfig(KAFKA_CONFIG_SASL_USERNAME, mConfig.Authentication.SaslUsername)) {
            return false;
        }

        if (!SetConfig(KAFKA_CONFIG_SASL_PASSWORD, mConfig.Authentication.SaslPassword)) {
            return false;
        }

        return true;
    }

    bool InitCustomConfig() {
        for (const auto& kv : mConfig.CustomConfig) {
            if (!SetConfig(kv.first, kv.second)) {
                return false;
            }
        }
        return true;
    }

    bool InitProducer() {
        rd_kafka_conf_set_dr_msg_cb(mConf, KafkaProducer::DeliveryReportCallback);
        rd_kafka_conf_set_opaque(mConf, this);

        char errstr[512];
        mProducer = rd_kafka_new(RD_KAFKA_PRODUCER, mConf, errstr, sizeof(errstr));
        if (!mProducer) {
            LOG_ERROR(sLogger, ("create rdkafka producer failed", errstr));
            return false;
        }
        mConf = nullptr;

        mIsRunning = true;
        mPollThread = std::thread([this]() {
            while (mIsRunning.load(std::memory_order_relaxed)) {
                rd_kafka_poll(mProducer, KAFKA_POLL_INTERVAL_MS);
            }
        });

        return true;
    }

    KafkaConfig mConfig;
    rd_kafka_t* mProducer;
    rd_kafka_conf_t* mConf;
    std::atomic<bool> mIsRunning;
    std::thread mPollThread;
    std::mutex mProducerMutex;
    bool mIsClosed;

    std::vector<ProducerContext*> mContextPool;
    std::mutex mContextPoolMutex;
    static constexpr size_t kMaxContextCache = 65536;
};

void KafkaProducer::DeliveryReportCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque) {
    auto* context = static_cast<ProducerContext*>(rkmessage->_private);
    if (!context) {
        return;
    }

    if (rkmessage->err == RD_KAFKA_RESP_ERR_NO_ERROR) {
        context->callback(true, {KafkaProducer::ErrorType::SUCCESS, "", 0});
    } else {
        KafkaProducer::ErrorInfo errorInfo;
        errorInfo.type = KafkaProducer::MapKafkaError(rkmessage->err);
        errorInfo.message = rd_kafka_err2str(rkmessage->err);
        errorInfo.code = static_cast<int>(rkmessage->err);
        context->callback(false, errorInfo);
    }

    auto* producerImpl = static_cast<KafkaProducer::Impl*>(rd_kafka_opaque(rk));
    if (producerImpl) {
        producerImpl->ReleaseContext(context);
    } else {
        delete context;
    }
}

KafkaProducer::KafkaProducer() : mImpl(std::make_unique<Impl>()) {
}

KafkaProducer::~KafkaProducer() = default;

bool KafkaProducer::Init(const KafkaConfig& config) {
    return mImpl->Init(config);
}

KafkaProducer::HeadersTemplate*
KafkaProducer::CreateHeadersTemplate(const std::vector<std::pair<std::string, std::string>>& headers) {
    return mImpl->CreateHeadersTemplate(headers);
}

void KafkaProducer::DestroyHeadersTemplate(KafkaProducer::HeadersTemplate* tpl) {
    mImpl->DestroyHeadersTemplate(tpl);
}

void KafkaProducer::ProduceAsync(const std::string& topic,
                                 std::string&& value,
                                 Callback callback,
                                 const std::string& key,
                                 KafkaProducer::HeadersTemplate* headersTemplate) {
    mImpl->ProduceAsync(topic, std::move(value), std::move(callback), key, headersTemplate);
}

bool KafkaProducer::Flush(int timeoutMs) {
    return mImpl->Flush(timeoutMs);
}

void KafkaProducer::Close() {
    mImpl->Close();
}

} // namespace logtail
