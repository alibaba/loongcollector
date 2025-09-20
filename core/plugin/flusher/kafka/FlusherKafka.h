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

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "collection_pipeline/plugin/interface/Flusher.h"
#include "collection_pipeline/serializer/JsonSerializer.h"
#include "common/FormattedString.h"
#include "models/PipelineEventGroup.h"
#include "models/PipelineEventPtr.h"
#include "monitor/MetricManager.h"
#include "plugin/flusher/kafka/KafkaConfig.h"
#include "plugin/flusher/kafka/KafkaProducer.h"

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

#ifdef APSARA_UNIT_TEST_MAIN
    void SetProducerForTest(std::unique_ptr<KafkaProducer> producer) { mProducer = std::move(producer); }
    void SetSerializerForTest(std::unique_ptr<EventGroupSerializer> serializer) { mSerializer = std::move(serializer); }
#endif

private:
    enum class TopicValueSource { GroupTag, TagWithPrefix, Content };

    struct TopicPlaceholderDescriptor {
        std::string placeholder;
        std::string key;
        TopicValueSource source;
    };

    bool SerializeAndSend(PipelineEventGroup&& group);
    void HandleDeliveryResult(bool success, const KafkaProducer::ErrorInfo& errorInfo);
    bool PopulateTopicValues(const PipelineEventPtr& event,
                             const GroupTags& groupTags,
                             std::unordered_map<std::string, std::string>& values) const;

    KafkaConfig mKafkaConfig;
    std::unique_ptr<KafkaProducer> mProducer;
    std::unique_ptr<EventGroupSerializer> mSerializer;

    FormattedString mTopicFormatter;
    std::vector<TopicPlaceholderDescriptor> mTopicDescriptors;
    std::set<std::string> mTopicSet;

    CounterPtr mSendCnt;
    CounterPtr mSuccessCnt;
    CounterPtr mSendDoneCnt;
    CounterPtr mDiscardCnt;
    CounterPtr mNetworkErrorCnt;
    CounterPtr mServerErrorCnt;
    CounterPtr mUnauthErrorCnt;
    CounterPtr mParamsErrorCnt;
    CounterPtr mOtherErrorCnt;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherKafkaUnittest;
#endif
};

} // namespace logtail
