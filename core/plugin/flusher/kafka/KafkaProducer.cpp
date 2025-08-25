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

    delete context;
}

class KafkaProducer::Impl {
public:
    Impl() : mProducer(nullptr), mConf(nullptr), mIsRunning(false), mIsClosed(false) {}
    ~Impl() {
        if (!mIsClosed) {
            Close();
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

        if (!SetConfig(KAFKA_CONFIG_BATCH_NUM_MESSAGES, std::to_string(mConfig.Producer.BatchNumMessages))
            || !SetConfig(KAFKA_CONFIG_LINGER_MS, std::to_string(mConfig.Producer.LingerMs))
            || !SetConfig(KAFKA_CONFIG_QUEUE_BUFFERING_MAX_KBYTES,
                          std::to_string(mConfig.Producer.QueueBufferingMaxKbytes))
            || !SetConfig(KAFKA_CONFIG_QUEUE_BUFFERING_MAX_MESSAGES,
                          std::to_string(mConfig.Producer.QueueBufferingMaxMessages))
            || !SetConfig(KAFKA_CONFIG_BATCH_SIZE, std::to_string(mConfig.Producer.BatchSize))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_MAX_BYTES, std::to_string(mConfig.Producer.MaxMessageBytes))) {
            return false;
        }

        if (!SetConfig(KAFKA_CONFIG_ACKS, mConfig.Delivery.Acks)
            || !SetConfig(KAFKA_CONFIG_REQUEST_TIMEOUT_MS, std::to_string(mConfig.Delivery.RequestTimeoutMs))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_TIMEOUT_MS, std::to_string(mConfig.Delivery.MessageTimeoutMs))
            || !SetConfig(KAFKA_CONFIG_MESSAGE_SEND_MAX_RETRIES, std::to_string(mConfig.Delivery.MaxRetries))
            || !SetConfig(KAFKA_CONFIG_RETRY_BACKOFF_MS, std::to_string(mConfig.Delivery.RetryBackoffMs))) {
            return false;
        }

        for (const auto& kv : mConfig.CustomConfig) {
            if (!SetConfig(kv.first, kv.second)) {
                return false;
            }
        }

        rd_kafka_conf_set_dr_msg_cb(mConf, KafkaProducer::DeliveryReportCallback);

        char errstr[512];
        mProducer = rd_kafka_new(RD_KAFKA_PRODUCER, mConf, errstr, sizeof(errstr));
        if (!mProducer) {
            return false;
        }

        mIsRunning = true;
        mPollThread = std::thread([this]() {
            while (mIsRunning.load(std::memory_order_relaxed)) {
                std::lock_guard<std::mutex> lock(mProducerMutex);
                if (mProducer) {
                    rd_kafka_poll(mProducer, 100);
                }
            }
        });

        return true;
    }

    void ProduceAsync(const std::string& topic, std::string&& value, KafkaProducer::Callback callback) {
        if (!mProducer) {
            KafkaProducer::ErrorInfo errorInfo;
            errorInfo.type = KafkaProducer::ErrorType::OTHER_ERROR;
            errorInfo.message = "producer not initialized";
            errorInfo.code = 0;
            callback(false, errorInfo);
            return;
        }

        auto* context = new ProducerContext{std::move(callback), {KafkaProducer::ErrorType::SUCCESS, "", 0}};

        rd_kafka_resp_err_t err = rd_kafka_producev(mProducer,
                                                    RD_KAFKA_V_TOPIC(topic.c_str()),
                                                    RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                                    RD_KAFKA_V_VALUE(value.data(), value.size()),
                                                    RD_KAFKA_V_OPAQUE(context),
                                                    RD_KAFKA_V_END);

        if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
            delete context;
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


        mIsRunning = false;
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
};

KafkaProducer::KafkaProducer() : mImpl(std::make_unique<Impl>()) {
}

KafkaProducer::~KafkaProducer() = default;

bool KafkaProducer::Init(const KafkaConfig& config) {
    return mImpl->Init(config);
}

void KafkaProducer::ProduceAsync(const std::string& topic, std::string&& value, Callback callback) {
    mImpl->ProduceAsync(topic, std::move(value), std::move(callback));
}

bool KafkaProducer::Flush(int timeoutMs) {
    return mImpl->Flush(timeoutMs);
}

void KafkaProducer::Close() {
    mImpl->Close();
}

} // namespace logtail
