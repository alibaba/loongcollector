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

#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/batch/BatchedEvents.h"
#include "collection_pipeline/queue/SenderQueueManager.h"
#include "common/ParamExtractor.h"
#include "common/StringView.h"
#include "file_server/checkpoint/FileSendCheckpoint.h"
#include "logger/Logger.h"
#include "models/LogEvent.h"
#include "models/PipelineEvent.h"
#include "monitor/AlarmManager.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "plugin/flusher/kafka/KafkaConstant.h"

using namespace std;

namespace logtail {

const std::string FlusherKafka::sName = "flusher_kafka_native";

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

    if (!mSerializer) {
        mSerializer = make_unique<JsonEventGroupSerializer>(this);
    }

    if (!mTopicFormatter.Init(mKafkaConfig.Topic)) {
        LOG_ERROR(mContext->GetLogger(), ("invalid topic format string", mKafkaConfig.Topic));
        return false;
    }

    if (mKafkaConfig.PartitionerType.empty() || mKafkaConfig.PartitionerType == PARTITIONER_RANDOM) {
        mKafkaConfig.Partitioner = LIBRDKAFKA_PARTITIONER_RANDOM;
    } else if (mKafkaConfig.PartitionerType == PARTITIONER_HASH) {
        if (mKafkaConfig.HashKeys.empty()) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               "HashKeys must be specified when PartitionerType is hash",
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        for (const auto& key : mKafkaConfig.HashKeys) {
            if (key.rfind(PARTITIONER_PREFIX, 0) != 0) {
                PARAM_ERROR_RETURN(mContext->GetLogger(),
                                   mContext->GetAlarm(),
                                   std::string("HashKeys must start with ") + PARTITIONER_PREFIX,
                                   sName,
                                   mContext->GetConfigName(),
                                   mContext->GetProjectName(),
                                   mContext->GetLogstoreName(),
                                   mContext->GetRegion());
            }
        }
        mKafkaConfig.Partitioner = LIBRDKAFKA_PARTITIONER_MURMUR2_RANDOM;
    } else {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           std::string("Unknown PartitionerType: ") + mKafkaConfig.PartitionerType,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    if (!mProducer->Init(mKafkaConfig)) {
        LOG_ERROR(mContext->GetLogger(), ("failed to init kafka producer", ""));
        return false;
    }

    mExpandedTopic = mTopicFormatter.GetTemplate();
    GenerateQueueKey(mExpandedTopic);
    SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, mExpandedTopic, *mContext);

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
             ("FlusherKafka initialized successfully", "")("configured_topic", mKafkaConfig.Topic)(
                 "expanded_topic", mExpandedTopic)("brokers", mKafkaConfig.Brokers.size())(
                 "Version", mKafkaConfig.Version.empty() ? std::string("<unset>") : mKafkaConfig.Version));

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

    auto events = std::move(group.MutableEvents());

    const bool isDynamicTopic = mTopicFormatter.IsDynamic();
    const auto& sizedTags = group.GetSizedTags();
    auto& sourceBuffer = group.GetSourceBuffer();
    auto& checkpoint = group.GetExactlyOnceCheckpoint();

    // file input with deferred commit: track delivery of every message produced for this group;
    // the sentinel count (1) is released after the produce loop so callbacks that fire early do
    // not finalize the group prematurely
    auto fileCpt = group.GetFileSendCheckpoint();
    std::shared_ptr<GroupAck> ack;
    if (fileCpt) {
        ack = std::make_shared<GroupAck>();
        ack->mFileSendCheckpoint = fileCpt;
        ack->mRemaining.store(1);
    }

    bool allSuccess = true;
    std::string serializedData;
    std::string errorMsg;

    for (auto& event : events) {
        errorMsg.clear();
        serializedData.clear();

        std::string topic = mExpandedTopic;
        if (isDynamicTopic) {
            if (!mTopicFormatter.Format(event, group.GetTags(), topic)) {
                topic = mExpandedTopic;
                LOG_ERROR(mContext->GetLogger(), ("Failed to format dynamic topic from template", mExpandedTopic));
            }
        }

        std::string partitionKey;
        if (mKafkaConfig.PartitionerType == PARTITIONER_HASH) {
            partitionKey = GeneratePartitionKey(event);
        }

        BatchedEvents batchedEvents;
        batchedEvents.mEvents.reserve(1);
        batchedEvents.mEvents.emplace_back(std::move(event));
        batchedEvents.mTags = sizedTags;
        batchedEvents.mSourceBuffers.emplace_back(sourceBuffer);
        batchedEvents.mExactlyOnceCheckpoint = checkpoint;

        if (!mSerializer->DoSerialize(std::move(batchedEvents), serializedData, errorMsg)) {
            LOG_ERROR(mContext->GetLogger(),
                      ("failed to serialize events", errorMsg)("topic", topic)("action", "discard data"));
            mContext->GetAlarm().SendAlarmCritical(SERIALIZE_FAIL_ALARM,
                                                   "failed to serialize events: " + errorMsg + "\taction: discard data",
                                                   mContext->GetRegion(),
                                                   mContext->GetProjectName(),
                                                   mContext->GetConfigName(),
                                                   mContext->GetLogstoreName());
            mDiscardCnt->Add(1);
            allSuccess = false;
            continue;
        }

        mSendCnt->Add(1);

        size_t bytes = serializedData.size();
        if (ack) {
            ack->mRemaining.fetch_add(1);
        }
        mProducer->ProduceAsync(
            topic,
            std::move(serializedData),
            [this, bytes, ack](bool success, const KafkaProducer::ErrorInfo& errorInfo) {
                if (success) {
                    LOG_DEBUG(mContext->GetLogger(), ("kafka message queued", bytes));
                }
                HandleDeliveryResult(success, errorInfo);
                if (ack) {
                    if (!success) {
                        ack->mAllOk.store(false);
                    }
                    if (ack->mRemaining.fetch_sub(1) == 1) {
                        FinalizeGroupAck(ack);
                    }
                }
            },
            partitionKey);
    }
    // release the sentinel; if all callbacks already fired, this finalizes the group
    if (ack && ack->mRemaining.fetch_sub(1) == 1) {
        FinalizeGroupAck(ack);
    }
    return allSuccess;
}

void FlusherKafka::FinalizeGroupAck(const std::shared_ptr<GroupAck>& ack) {
    if (!ack || !ack->mFileSendCheckpoint) {
        return;
    }
    const auto& cp = ack->mFileSendCheckpoint;
    if (ack->mAllOk.load()) {
        // every message of this group was durably delivered, advance the committed file offset
        ConfirmFileSendCheckpoint(cp);
    } else {
        // a message could not be delivered; request a runtime rewind so the reader re-reads this
        // unit's offset range from the last committed position (recovered without a restart)
        ReportFileSendCheckpointFailure(cp);
    }
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

        mContext->GetAlarm().SendAlarmCritical(SEND_DATA_FAIL_ALARM,
                                               "Kafka delivery error: " + errorInfo.message,
                                               mContext->GetRegion(),
                                               mContext->GetProjectName(),
                                               mContext->GetConfigName(),
                                               mKafkaConfig.Topic);
    }
}

std::string FlusherKafka::GeneratePartitionKey(const PipelineEventPtr& event) const {
    if (mKafkaConfig.PartitionerType != PARTITIONER_HASH) {
        return std::string();
    }

    std::string result;
    result.reserve(64);

    for (const auto& key : mKafkaConfig.HashKeys) {
        if (key.size() <= PARTITIONER_PREFIX.size()) {
            continue;
        }
        StringView fieldName(key.data() + PARTITIONER_PREFIX.size(), key.size() - PARTITIONER_PREFIX.size());

        // TODO: future support more event types such as MetricEvent or SpanEvent
        if (event->GetType() == PipelineEvent::Type::LOG) {
            const LogEvent& logEvent = event.Cast<LogEvent>();
            StringView v = logEvent.GetContent(fieldName);
            if (!v.empty()) {
                if (!result.empty()) {
                    result.append("###");
                }
                result.append(v.data(), v.size());
            }
        } else {
            LOG_ERROR(mContext->GetLogger(), ("unsupported event type for partition key", (int)event->GetType()));
        }
    }

    return result;
}

} // namespace logtail
