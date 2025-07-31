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

const std::string FlusherKafka::sName = "flusher_kafka";

FlusherKafka::FlusherKafka() : mProducer(std::make_unique<KafkaProducer>()) {
}

FlusherKafka::~FlusherKafka() = default;

bool FlusherKafka::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    if (!mKafkaConfig.Load(config, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (!mProducer->Init(mKafkaConfig)) {
        return false;
    }

    mSerializer = make_unique<JsonEventGroupSerializer>(this);

    GenerateQueueKey(mKafkaConfig.Topic);
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
             ("FlusherKafka initialized successfully", "")("topic", mKafkaConfig.Topic)("brokers",
                                                                                        mKafkaConfig.Brokers.size()));

    return true;
}

bool FlusherKafka::Start() {
    return Flusher::Start();
}

bool FlusherKafka::Stop(bool isPipelineRemoving) {
    if (mProducer) {
        mProducer->Close();
    }
    return Flusher::Stop(isPipelineRemoving);
}

bool FlusherKafka::Send(PipelineEventGroup&& g) {
    return SerializeAndSend(std::move(g));
}

bool FlusherKafka::Flush(size_t key) {
    if (mProducer) {
        return mProducer->Flush(KAFKA_FLUSH_TIMEOUT_MS);
    }
    return true;
}

bool FlusherKafka::FlushAll() {
    return Flush(0);
}

bool FlusherKafka::SerializeAndSend(PipelineEventGroup&& group) {
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

    mProducer->ProduceAsync(
        mKafkaConfig.Topic, std::move(serializedData), [this](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
            HandleDeliveryResult(success, errorInfo);
        });

    return true;
}

void FlusherKafka::HandleDeliveryResult(bool success, const KafkaProducer::ErrorInfo& errorInfo) {
    mSendDoneCnt->Add(1);

    if (success) {
        mSuccessCnt->Add(1);
    } else {
        LOG_ERROR(mContext->GetLogger(),
                  ("kafka message delivery failed", errorInfo.message)("topic", mKafkaConfig.Topic)("error_code",
                                                                                                    errorInfo.code));

        switch (errorInfo.type) {
            case KafkaProducer::ErrorType::AUTH_ERROR:
                mUnauthErrorCnt->Add(1);
                break;
            case KafkaProducer::ErrorType::NETWORK_ERROR:
                mNetworkErrorCnt->Add(1);
                break;
            case KafkaProducer::ErrorType::SERVER_ERROR:
                mServerErrorCnt->Add(1);
                break;
            case KafkaProducer::ErrorType::PARAMS_ERROR:
                mParamsErrorCnt->Add(1);
                break;
            case KafkaProducer::ErrorType::QUEUE_FULL:
                mDiscardCnt->Add(1);
                break;
            case KafkaProducer::ErrorType::OTHER_ERROR:
            default:
                mOtherErrorCnt->Add(1);
                break;
        }

        mContext->GetAlarm().SendAlarm(SEND_DATA_FAIL_ALARM,
                                       "Kafka delivery error: " + errorInfo.message,
                                       mContext->GetRegion(),
                                       mContext->GetProjectName(),
                                       mContext->GetConfigName(),
                                       mKafkaConfig.Topic);
    }
}

} // namespace logtail
