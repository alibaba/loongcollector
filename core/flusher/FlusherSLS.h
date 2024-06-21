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

#include "batch/Batcher.h"
#include "batch/BatchStatus.h"
#include "common/LogstoreFeedbackKey.h"
#include "compression/Compressor.h"
#include "models/PipelineEventGroup.h"
#include "plugin/interface/Flusher.h"
#include "serializer/SLSSerializer.h"
#include "queue/SenderQueueItem.h"

namespace logtail {

class FlusherSLS : public Flusher {
public:
    enum class TelemetryType { LOG, METRIC };

    static const std::string sName;

    FlusherSLS();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Register() override;
    bool Unregister(bool isPipelineRemoving) override;
    void Send(PipelineEventGroup&& g) override;
    void Flush(size_t key) override;
    void FlushAll() override;

    LogstoreFeedBackKey GetLogstoreKey() const { return mLogstoreKey; }
    CompressType GetCompressType() const { return mCompressor ? mCompressor->GetCompressType() : CompressType::NONE; }

    // for use of Go pipeline, stream, observer and shennong
    bool Send(std::string&& data, const std::string& shardHashKey, const std::string& logstore = "");

    std::string mProject;
    std::string mLogstore;
    std::string mRegion;
    std::string mEndpoint;
    std::string mAliuid;
    TelemetryType mTelemetryType = TelemetryType::LOG;
    uint32_t mFlowControlExpireTime = 0;
    int32_t mMaxSendRate = -1;
    std::vector<std::string> mShardHashKeys;

private:
    static const std::unordered_set<std::string> sNativeParam;

    void GenerateGoPlugin(const Json::Value& config, Json::Value& res) const;
    void SerializeAndPush(std::vector<BatchedEventsList>&& groupLists);
    void SerializeAndPush(BatchedEventsList&& groupList);
    void SerializeAndPush(PipelineEventGroup&& g); // for exactly once only
    std::string GetShardHashKey(const BatchedEvents& g) const;
    void AddPackId(BatchedEvents& g) const;

    // TODO: remove after sender queue refactorization
    void PushToQueue(std::string&& data,
                     size_t rawSize,
                     RawDataType type,
                     const std::string& logstore = "",
                     const std::string& shardHashKey = "",
                     RangeCheckpointPtr&& eoo = RangeCheckpointPtr());

    LogstoreFeedBackKey mLogstoreKey = 0;

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
