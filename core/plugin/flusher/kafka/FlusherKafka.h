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

#pragma once

#include <librdkafka/rdkafka.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "collection_pipeline/plugin/interface/Flusher.h"
#include "collection_pipeline/serializer/JsonSerializer.h"
#include "monitor/MetricManager.h"
#include "plugin/flusher/kafka/KafkaConstant.h"
#include "plugin/flusher/kafka/KafkaUtil.h"

namespace logtail {

class FlusherKafka : public Flusher {
public:
    static const std::string sName;

    FlusherKafka();
    ~FlusherKafka() override;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;

private:
    bool InitKafkaProducer();
    bool SerializeAndSend(PipelineEventGroup&& group);
    void DestroyKafkaResources();

    static void DeliveryReportCallback(rd_kafka_t* rk, const rd_kafka_message_t* rkmessage, void* opaque);

    std::vector<std::string> mBrokers;
    std::string mTopic;
    std::string mClientID;
    uint32_t mTimeoutMs;
    uint32_t mRetries;
    uint32_t mBatchNumMessages;
    uint32_t mLingerMs;

    std::unique_ptr<EventGroupSerializer> mSerializer;

    CounterPtr mSendCnt;
    CounterPtr mSuccessCnt;
    CounterPtr mSendDoneCnt;
    CounterPtr mDiscardCnt;
    CounterPtr mNetworkErrorCnt;
    CounterPtr mServerErrorCnt;
    CounterPtr mUnauthErrorCnt;
    CounterPtr mParamsErrorCnt;
    CounterPtr mOtherErrorCnt;


    rd_kafka_t* mProducer;
    rd_kafka_topic_t* mKafkaTopic;
    rd_kafka_conf_t* mKafkaConf;
    rd_kafka_topic_conf_t* mTopicConf;

    std::atomic<bool> mIsRunning;
    std::thread mPollThread;
    std::mutex mProducerMutex;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherKafkaUnittest;
#endif
};

} // namespace logtail
