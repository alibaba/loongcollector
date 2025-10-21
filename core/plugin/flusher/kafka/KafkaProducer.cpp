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

        std::string acksStr;
        if (mConfig.RequiredAcks < 0) {
            acksStr = "all";
        } else {
            acksStr = std::to_string(mConfig.RequiredAcks);
        }
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

        if (!mConfig.Partitioner.empty()) {
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
        }

        for (const auto& kv : mConfig.CustomConfig) {
            if (!SetConfig(kv.first, kv.second)) {
                return false;
            }
        }

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

    void ProduceAsync(const std::string& topic,
                      std::string&& value,
                      KafkaProducer::Callback callback,
                      const std::string& key) {
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

        rd_kafka_resp_err_t err;
        if (!key.empty()) {
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
            ReleaseContext(context);
            KafkaProducer::ErrorInfo errorInfo;
            errorInfo.type = KafkaProducer::MapKafkaError(err);
            errorInfo.message = rd_kafka_err2str(err);
            errorInfo.code = static_cast<int>(err);
            callback(false, errorInfo);
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

void KafkaProducer::ProduceAsync(const std::string& topic,
                                 std::string&& value,
                                 Callback callback,
                                 const std::string& key) {
    mImpl->ProduceAsync(topic, std::move(value), std::move(callback), key);
}

bool KafkaProducer::Flush(int timeoutMs) {
    return mImpl->Flush(timeoutMs);
}

void KafkaProducer::Close() {
    mImpl->Close();
}

} // namespace logtail
