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

#include "plugin/flusher/kafka/FlusherKafka.h"

#include <cstring>

#include <sstream>

#include "collection_pipeline/batch/BatchedEvents.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "monitor/metric_constants/MetricConstants.h"

using namespace std;

namespace logtail {

const string FlusherKafka::sName = "flusher_kafka";

FlusherKafka::FlusherKafka()
    : mClientID(KAFKA_DEFAULT_CLIENT_ID),
      mTimeoutMs(KAFKA_DEFAULT_TIMEOUT_MS),
      mRetries(KAFKA_DEFAULT_RETRIES),
      mBatchNumMessages(KAFKA_DEFAULT_BATCH_NUM_MESSAGES),
      mLingerMs(KAFKA_DEFAULT_LINGER_MS),
      mProducer(nullptr),
      mKafkaConf(nullptr),
      mIsRunning(false) {
}

FlusherKafka::~FlusherKafka() {
    Stop(true);
    DestroyKafkaResources();
}

bool FlusherKafka::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    if (!GetMandatoryListParam<string>(config, "Brokers", mBrokers, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (!GetMandatoryStringParam(config, "Topic", mTopic, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    GetOptionalStringParam(config, "ClientID", mClientID, errorMsg);
    GetOptionalUIntParam(config, "TimeoutMs", mTimeoutMs, errorMsg);
    GetOptionalUIntParam(config, "Retries", mRetries, errorMsg);
    GetOptionalUIntParam(config, "BatchNumMessages", mBatchNumMessages, errorMsg);
    GetOptionalUIntParam(config, "LingerMs", mLingerMs, errorMsg);

    if (config.isMember("Kafka") && config["Kafka"].isObject()) {
        const Json::Value& kafkaConfig = config["Kafka"];
        for (const auto& key : kafkaConfig.getMemberNames()) {
            mKafkaOptions[key] = kafkaConfig[key].asString();
        }
    }

    if (!InitKafkaProducer()) {
        return false;
    }

    mSerializer = make_unique<JsonEventGroupSerializer>(this);

    GenerateQueueKey(mTopic);
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, *mContext);

    mSendCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OUT_EVENT_GROUPS_TOTAL);
    mSuccessCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SUCCESS_TOTAL);
    mSendDoneCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SEND_DONE_TOTAL);
    mDiscardCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_DISCARD_TOTAL);
    mNetworkErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_NETWORK_ERROR_TOTAL);
    mServerErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_SERVER_ERROR_TOTAL);
    mUnauthErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_UNAUTH_ERROR_TOTAL);
    mParamsErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_PARAMS_ERROR_TOTAL);
    mOtherErrorCnt = GetMetricsRecordRef().CreateCounter(METRIC_PLUGIN_FLUSHER_OTHER_ERROR_TOTAL);

    LOG_INFO(mContext->GetLogger(),
             ("FlusherKafka initialized successfully", "")("topic", mTopic)("brokers", mBrokers.size()));

    return true;
}

bool FlusherKafka::Start() {
    if (!Flusher::Start()) {
        return false;
    }

    try {
        mIsRunning = true;
        mPollThread = std::thread([this]() {
            LOG_INFO(mContext->GetLogger(), ("Kafka poll thread started", ""));
            while (mIsRunning.load(std::memory_order_relaxed)) {
                {
                    std::lock_guard<std::mutex> lock(mProducerMutex);
                    if (mProducer) {
                        rd_kafka_poll(mProducer, KAFKA_POLL_INTERVAL_MS);
                    }
                }
            }
            LOG_INFO(mContext->GetLogger(), ("Kafka poll thread stopped", ""));
        });
        LOG_INFO(mContext->GetLogger(), ("Kafka poll thread created successfully", ""));
        return true;
    } catch (const std::exception& e) {
        mIsRunning = false;
        LOG_ERROR(mContext->GetLogger(),
                  ("Failed to create Kafka poll thread", e.what())("action", "Kafka flusher cannot start properly"));
        return false;
    }
}

bool FlusherKafka::Stop(bool isPipelineRemoving) {
    if (mIsRunning.exchange(false)) {
        LOG_INFO(mContext->GetLogger(), ("stopping kafka poll thread", ""));
        if (mPollThread.joinable()) {
            mPollThread.join();
        }
    }


    std::lock_guard<std::mutex> lock(mProducerMutex);
    if (mProducer) {
        LOG_INFO(mContext->GetLogger(), ("kafka flushing messages before shutdown", "")("timeout_ms", mTimeoutMs));
        rd_kafka_flush(mProducer, mTimeoutMs);
    }


    return Flusher::Stop(isPipelineRemoving);
}

bool FlusherKafka::Send(PipelineEventGroup&& g) {
    return SerializeAndSend(std::move(g));
}

bool FlusherKafka::Flush(size_t key) {
    std::lock_guard<std::mutex> lock(mProducerMutex);
    if (mProducer) {
        rd_kafka_resp_err_t result = rd_kafka_flush(mProducer, KAFKA_FLUSH_TIMEOUT_MS);
        if (result != RD_KAFKA_RESP_ERR_NO_ERROR) {
            LOG_WARNING(mContext->GetLogger(), ("failed to flush kafka producer", KafkaUtil::GetErrorString(result)));
            return false;
        }
    }
    return true;
}

bool FlusherKafka::FlushAll() {
    return Flush(0);
}

bool FlusherKafka::SetKafkaConfig(rd_kafka_conf_t* conf, const char* key, const std::string& value) {
    char errstr[512];
    if (rd_kafka_conf_set(conf, key, value.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOG_ERROR(mContext->GetLogger(), ("failed to set kafka config", errstr)("key", key)("value", value));
        return false;
    }
    return true;
}

bool FlusherKafka::InitKafkaProducer() {
    mKafkaConf = rd_kafka_conf_new();
    if (!mKafkaConf) {
        LOG_ERROR(mContext->GetLogger(), ("failed to create kafka configuration", ""));
        return false;
    }


    for (const auto& kv : mKafkaOptions) {
        if (!SetKafkaConfig(mKafkaConf, kv.first.c_str(), kv.second)) {
            rd_kafka_conf_destroy(mKafkaConf);
            mKafkaConf = nullptr;
            return false;
        }
    }

    std::string brokersStr = KafkaUtil::BrokersToString(mBrokers);
    if (brokersStr.empty()) {
        LOG_ERROR(mContext->GetLogger(), ("empty brokers list", ""));
        return false;
    }


    if (!SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_BOOTSTRAP_SERVERS.c_str(), brokersStr)
        || !SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_CLIENT_ID.c_str(), mClientID)
        || !SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_RETRIES.c_str(), std::to_string(mRetries))
        || !SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_REQUEST_TIMEOUT_MS.c_str(), std::to_string(mTimeoutMs))
        || !SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_BATCH_NUM_MESSAGES.c_str(), std::to_string(mBatchNumMessages))
        || !SetKafkaConfig(mKafkaConf, KAFKA_CONFIG_LINGER_MS.c_str(), std::to_string(mLingerMs))) {
        return false;
    }


    rd_kafka_conf_set_dr_msg_cb(mKafkaConf, DeliveryReportCallback);
    rd_kafka_conf_set_opaque(mKafkaConf, this);


    char errstr[512];
    mProducer = rd_kafka_new(RD_KAFKA_PRODUCER, mKafkaConf, errstr, sizeof(errstr));
    if (!mProducer) {
        LOG_ERROR(mContext->GetLogger(), ("failed to create kafka producer", errstr));
        mKafkaConf = nullptr;
        return false;
    }
    mKafkaConf = nullptr;

    LOG_INFO(
        mContext->GetLogger(),
        ("kafka producer initialized successfully", "")("topic", mTopic)("brokers", brokersStr)("retries", mRetries));
    return true;
}

bool FlusherKafka::SerializeAndSend(PipelineEventGroup&& group) {
    std::lock_guard<std::mutex> lock(mProducerMutex);

    if (!mProducer) {
        LOG_ERROR(mContext->GetLogger(), ("kafka producer not initialized", ""));
        return false;
    }

    BatchedEvents batchedEvents(std::move(group.MutableEvents()),
                                std::move(group.GetSizedTags()),
                                std::move(group.GetSourceBuffer()),
                                group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                                std::move(group.GetExactlyOnceCheckpoint()));

    string serializedData;
    string errorMsg;
    if (!mSerializer->DoSerialize(std::move(batchedEvents), serializedData, errorMsg)) {
        LOG_ERROR(mContext->GetLogger(), ("failed to serialize events", errorMsg)("action", "discard data"));
        mContext->GetAlarm().SendAlarm(SERIALIZE_FAIL_ALARM,
                                       "failed to serialize events: " + errorMsg + "\taction: discard data",
                                       mContext->GetRegion(),
                                       mContext->GetProjectName(),
                                       mContext->GetConfigName(),
                                       mContext->GetLogstoreName());
        mDiscardCnt->Add(1);
        return false;
    }

    mSendCnt->Add(1);

    rd_kafka_resp_err_t err = rd_kafka_producev(mProducer,
                                                RD_KAFKA_V_TOPIC(mTopic.c_str()),
                                                RD_KAFKA_V_PARTITION(RD_KAFKA_PARTITION_UA),
                                                RD_KAFKA_V_VALUE(serializedData.data(), serializedData.size()),
                                                RD_KAFKA_V_OPAQUE(nullptr),
                                                RD_KAFKA_V_END);

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        LOG_ERROR(mContext->GetLogger(),
                  ("kafka producev failed", rd_kafka_err2str(err))("error_code", static_cast<int>(err)));


        HandleProduceError(err);
        return false;
    }

    return true;
}

void FlusherKafka::HandleProduceError(rd_kafka_resp_err_t err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR__QUEUE_FULL:
            LOG_WARNING(mContext->GetLogger(), ("kafka internal queue is full", "consider reducing send rate"));
            mContext->GetAlarm().SendAlarm(DISCARD_DATA_ALARM,
                                           "kafka internal queue is full, message discarded",
                                           mContext->GetRegion(),
                                           mContext->GetProjectName(),
                                           mContext->GetConfigName(),
                                           mContext->GetLogstoreName());
            mDiscardCnt->Add(1);
            break;

        case RD_KAFKA_RESP_ERR__AUTHENTICATION:
        case RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED:
        case RD_KAFKA_RESP_ERR_GROUP_AUTHORIZATION_FAILED:
            mUnauthErrorCnt->Add(1);
            break;

        case RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE:
        case RD_KAFKA_RESP_ERR__INVALID_ARG:
            mParamsErrorCnt->Add(1);
            break;

        default:
            mOtherErrorCnt->Add(1);
            break;
    }
}

void FlusherKafka::DestroyKafkaResources() {
    if (mProducer) {
        rd_kafka_destroy(mProducer);
        mProducer = nullptr;
    }

    if (mKafkaConf) {
        rd_kafka_conf_destroy(mKafkaConf);
        mKafkaConf = nullptr;
    }
}

void FlusherKafka::DeliveryReportCallback(rd_kafka_t* rk, const rd_kafka_message_t* message, void* opaque) {
    auto* flusher = static_cast<FlusherKafka*>(opaque);
    if (!flusher) {
        return;
    }

    flusher->mSendDoneCnt->Add(1);

    if (message->err) {
        const std::string errStr = rd_kafka_err2str(message->err);
        LOG_ERROR(flusher->mContext->GetLogger(),
                  ("kafka message delivery failed",
                   errStr)("topic", rd_kafka_topic_name(message->rkt))("partition", message->partition));


        flusher->HandleDeliveryError(message->err);

        flusher->mContext->GetAlarm().SendAlarm(SEND_DATA_FAIL_ALARM,
                                                "Kafka delivery error after retries: " + errStr,
                                                flusher->mContext->GetRegion(),
                                                flusher->mContext->GetProjectName(),
                                                flusher->mContext->GetConfigName(),
                                                flusher->mTopic);
    } else {
        flusher->mSuccessCnt->Add(1);
        LOG_DEBUG(flusher->mContext->GetLogger(),
                  ("kafka message delivered successfully", "")("topic", rd_kafka_topic_name(message->rkt))(
                      "partition", message->partition)("offset", message->offset));
    }
}


void FlusherKafka::HandleDeliveryError(rd_kafka_resp_err_t err) {
    switch (err) {
        case RD_KAFKA_RESP_ERR__AUTHENTICATION:
        case RD_KAFKA_RESP_ERR_TOPIC_AUTHORIZATION_FAILED:
        case RD_KAFKA_RESP_ERR_GROUP_AUTHORIZATION_FAILED:
            mUnauthErrorCnt->Add(1);
            break;

        case RD_KAFKA_RESP_ERR__TRANSPORT:
        case RD_KAFKA_RESP_ERR__ALL_BROKERS_DOWN:
        case RD_KAFKA_RESP_ERR__DESTROY:
        case RD_KAFKA_RESP_ERR__TIMED_OUT:
            mNetworkErrorCnt->Add(1);
            break;

        case RD_KAFKA_RESP_ERR_BROKER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_LEADER_NOT_AVAILABLE:
        case RD_KAFKA_RESP_ERR_NOT_LEADER_FOR_PARTITION:
            mServerErrorCnt->Add(1);
            break;

        case RD_KAFKA_RESP_ERR_MSG_SIZE_TOO_LARGE:
        case RD_KAFKA_RESP_ERR__INVALID_ARG:
            mParamsErrorCnt->Add(1);
            break;

        default:
            mOtherErrorCnt->Add(1);
            break;
    }
}

} // namespace logtail
