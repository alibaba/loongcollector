/*
 * Copyright 2023 iLogtail Authors
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

#include <json/json.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "batch/BatchStatus.h"
#include "batch/Batcher.h"
#include "compression/Compressor.h"
#include "models/PipelineEventGroup.h"
#include "plugin/interface/Flusher.h"
#include "queue/SenderQueueItem.h"
#include "sender/ConcurrencyLimiter.h"
#include "serializer/SLSSerializer.h"

namespace logtail {

class FlusherSLS : public Flusher {
    // TODO: temporarily used
    friend class ProfileSender;
public:
    enum class TelemetryType { LOG, METRIC };

    static std::shared_ptr<ConcurrencyLimiter> GetProjectConcurrencyLimiter(const std::string& project);
    static std::shared_ptr<ConcurrencyLimiter> GetRegionConcurrencyLimiter(const std::string& region);
    static void ClearInvalidConcurrencyLimiters();

    static const std::string sName;

    FlusherSLS();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;
    sdk::AsynRequest* BuildRequest(SenderQueueItem* item) const override;

    CompressType GetCompressType() const { return mCompressor ? mCompressor->GetCompressType() : CompressType::NONE; }

    // for use of Go pipeline, stream, observer and shennong
    bool Send(std::string&& data, const std::string& shardHashKey, const std::string& logstore = "");

    std::string mProject;
    std::string mLogstore;
    std::string mRegion;
    std::string mEndpoint;
    std::string mAliuid;
    TelemetryType mTelemetryType = TelemetryType::LOG;
    std::vector<std::string> mShardHashKeys;
    uint32_t mMaxSendRate = 0; // preserved only for exactly once
    uint32_t mFlowControlExpireTime = 0;

private:
    static const std::unordered_set<std::string> sNativeParam;

    static std::mutex sMux;
    static std::unordered_map<std::string, std::weak_ptr<ConcurrencyLimiter>> sProjectConcurrencyLimiterMap;
    static std::unordered_map<std::string, std::weak_ptr<ConcurrencyLimiter>> sRegionConcurrencyLimiterMap;

    void GenerateGoPlugin(const Json::Value& config, Json::Value& res) const;
    bool SerializeAndPush(std::vector<BatchedEventsList>&& groupLists);
    bool SerializeAndPush(BatchedEventsList&& groupList);
    bool SerializeAndPush(PipelineEventGroup&& g); // for exactly once only
    bool PushToQueue(QueueKey key, std::unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes = 500);
    std::string GetShardHashKey(const BatchedEvents& g) const;
    void AddPackId(BatchedEvents& g) const;

    Batcher<SLSEventBatchStatus> mBatcher;
    std::unique_ptr<EventGroupSerializer> mGroupSerializer;
    std::unique_ptr<Serializer<std::vector<CompressedLogGroup>>> mGroupListSerializer;
    std::unique_ptr<Compressor> mCompressor;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherSLSUnittest;
#endif
};

sls_logs::SlsCompressType ConvertCompressType(CompressType type);

} // namespace logtail
