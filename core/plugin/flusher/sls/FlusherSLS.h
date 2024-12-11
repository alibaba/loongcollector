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

#include "common/compression/Compressor.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/batch/BatchStatus.h"
#include "pipeline/batch/Batcher.h"
#include "pipeline/limiter/ConcurrencyLimiter.h"
#include "pipeline/plugin/interface/HttpFlusher.h"
#include "pipeline/queue/SLSSenderQueueItem.h"
#include "pipeline/serializer/SLSSerializer.h"
#include "plugin/flusher/sls/SLSClientManager.h"
#include "protobuf/sls/sls_logs.pb.h"

namespace logtail {

class FlusherSLS : public HttpFlusher {
public:
    static std::shared_ptr<ConcurrencyLimiter> GetLogstoreConcurrencyLimiter(const std::string& project,
                                                                             const std::string& logstore);
    static std::shared_ptr<ConcurrencyLimiter> GetProjectConcurrencyLimiter(const std::string& project);
    static std::shared_ptr<ConcurrencyLimiter> GetRegionConcurrencyLimiter(const std::string& region);
    static void ClearInvalidConcurrencyLimiters();

    static void RecycleResourceIfNotUsed();

    static std::string GetDefaultRegion();
    static void SetDefaultRegion(const std::string& region);
    static std::string GetAllProjects();
    static std::string GetProjectRegion(const std::string& project);

    static const std::string sName;

    FlusherSLS();

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    bool Send(PipelineEventGroup&& g) override;
    bool Flush(size_t key) override;
    bool FlushAll() override;
    bool BuildRequest(SenderQueueItem* item, std::unique_ptr<HttpSinkRequest>& req, bool* keepItem) override;
    void OnSendDone(const HttpResponse& response, SenderQueueItem* item) override;

    CompressType GetCompressType() const { return mCompressor ? mCompressor->GetCompressType() : CompressType::NONE; }

    // for use of Go pipeline and shennong
    bool Send(std::string&& data, const std::string& shardHashKey, const std::string& logstore = "");

    std::string mProject;
    std::string mLogstore;
    std::string mRegion;
    std::string mAliuid;
#ifdef __ENTERPRISE__
    EndpointMode mEndpointMode = EndpointMode::DEFAULT;
#endif
    std::string mEndpoint;
    sls_logs::SlsTelemetryType mTelemetryType = sls_logs::SlsTelemetryType::SLS_TELEMETRY_TYPE_LOGS;
    std::vector<std::string> mShardHashKeys;
    uint32_t mMaxSendRate = 0; // preserved only for exactly once

    // TODO: temporarily public for profile
    std::unique_ptr<Compressor> mCompressor;

private:
    static void InitResource();

    static void IncreaseProjectRegionReferenceCnt(const std::string& project, const std::string& region);
    static void DecreaseProjectRegionReferenceCnt(const std::string& project, const std::string& region);

    static std::mutex sMux;
    static std::unordered_map<std::string, std::weak_ptr<ConcurrencyLimiter>> sProjectConcurrencyLimiterMap;
    static std::unordered_map<std::string, std::weak_ptr<ConcurrencyLimiter>> sRegionConcurrencyLimiterMap;
    static std::unordered_map<std::string, std::weak_ptr<ConcurrencyLimiter>> sLogstoreConcurrencyLimiterMap;

    static const std::unordered_set<std::string> sNativeParam;

    static std::mutex sDefaultRegionLock;
    static std::string sDefaultRegion;

    static std::mutex sProjectRegionMapLock;
    static std::unordered_map<std::string, int32_t> sProjectRefCntMap;
    static std::unordered_map<std::string, std::string> sProjectRegionMap;

    static bool sIsResourceInited;

    void GenerateGoPlugin(const Json::Value& config, Json::Value& res) const;
    bool SerializeAndPush(std::vector<BatchedEventsList>&& groupLists);
    bool SerializeAndPush(BatchedEventsList&& groupList);
    bool SerializeAndPush(PipelineEventGroup&& g); // for exactly once only
    bool PushToQueue(QueueKey key, std::unique_ptr<SenderQueueItem>&& item, uint32_t retryTimes = 500);
    std::string GetShardHashKey(const BatchedEvents& g) const;
    void AddPackId(BatchedEvents& g) const;

    std::unique_ptr<HttpSinkRequest> CreatePostLogStoreLogsRequest(const std::string& accessKeyId,
                                                                   const std::string& accessKeySecret,
                                                                   SLSClientManager::AuthType type,
                                                                   SLSSenderQueueItem* item) const;
    std::unique_ptr<HttpSinkRequest> CreatePostMetricStoreLogsRequest(const std::string& accessKeyId,
                                                                      const std::string& accessKeySecret,
                                                                      SLSClientManager::AuthType type,
                                                                      SLSSenderQueueItem* item) const;

    Batcher<SLSEventBatchStatus> mBatcher;
    std::unique_ptr<EventGroupSerializer> mGroupSerializer;
    std::unique_ptr<Serializer<std::vector<CompressedLogGroup>>> mGroupListSerializer;
    // This may not be cached. However, this provides a simple way to control the lifetime of a CandidateHostsInfo.
    // Otherwise, timeout machanisim must be emplyed to clean up unused CandidateHostsInfo.
    std::shared_ptr<CandidateHostsInfo> mCandidateHostsInfo;

    CounterPtr mSendCnt;
    CounterPtr mSendDoneCnt;
    CounterPtr mSuccessCnt;
    CounterPtr mNetworkErrorCnt;
    CounterPtr mServerErrorCnt;
    CounterPtr mShardWriteQuotaErrorCnt;
    CounterPtr mProjectQuotaErrorCnt;
    CounterPtr mUnauthErrorCnt;
    CounterPtr mParamsErrorCnt;
    CounterPtr mSequenceIDErrorCnt;
    CounterPtr mRequestExpiredErrorCnt;
    CounterPtr mOtherErrorCnt;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherSLSUnittest;
#endif
};

sls_logs::SlsCompressType ConvertCompressType(CompressType type);

} // namespace logtail
