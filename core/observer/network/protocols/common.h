/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "interface/protocol.h"
#include <deque>
#include "protobuf/sls/sls_logs.pb.h"
#include "interface/helper.h"
#include "LogtailAlarm.h"
#include "metas/ServiceMetaCache.h"
#include "Logger.h"
#include <unordered_map>
#include <ostream>

namespace logtail {


/**
 * Single protocol event metrics info.
 */
struct CommonProtocolEventInfo {
    CommonProtocolEventInfo() = default;
    CommonProtocolEventInfo(CommonProtocolEventInfo&& other) noexcept = default;
    CommonProtocolEventInfo(const CommonProtocolEventInfo& other) = default;
    CommonProtocolEventInfo& operator=(CommonProtocolEventInfo&& other) noexcept = default;
    CommonProtocolEventInfo& operator=(const CommonProtocolEventInfo& other) = default;


    friend std::ostream& operator<<(std::ostream& os, const CommonProtocolEventInfo& info) {
        os << "LatencyNs: " << info.LatencyNs << " ReqBytes: " << info.ReqBytes << " RespBytes: " << info.RespBytes;
        return os;
    }

    std::string ToString() const {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    int64_t LatencyNs{0};
    int32_t ReqBytes{0};
    int32_t RespBytes{0};
};

/**
 * A pair of protocol keys and metrics info.
 * @tparam ProtocolEventKey stores unique protocol keys
 */
template <typename ProtocolEventKey>
struct CommonProtocolEvent {
    CommonProtocolEvent() = default;
    CommonProtocolEvent(CommonProtocolEvent&& other) noexcept = default;
    CommonProtocolEvent(const CommonProtocolEvent& other) = default;
    CommonProtocolEvent& operator=(CommonProtocolEvent&& other) noexcept = default;
    CommonProtocolEvent& operator=(const CommonProtocolEvent& other) = default;

    ProtocolEventKey Key;
    CommonProtocolEventInfo Info;
};

/**
 * Aggregate single protocol event info.
 */
struct CommonProtocolAggResult {
    CommonProtocolAggResult() = default;
    void Clear() {
        TotalCount = 0;
        TotalLatencyNs = 0;
        TotalReqBytes = 0;
        TotalRespBytes = 0;
    }

    bool IsEmpty() const { return TotalCount == 0; }

    void AddEventInfo(CommonProtocolEventInfo& info) {
        ++TotalCount;
        TotalLatencyNs += info.LatencyNs;
        TotalReqBytes += info.ReqBytes;
        TotalRespBytes += info.RespBytes;
    }

    void Merge(CommonProtocolAggResult& aggResult) {
        TotalCount += aggResult.TotalCount;
        TotalLatencyNs += aggResult.TotalLatencyNs;
        TotalReqBytes += aggResult.TotalReqBytes;
        TotalRespBytes += aggResult.TotalRespBytes;
    }

    void ToPB(sls_logs::Log* log) const {
        AddAnyLogContent(log, observer::kCount, TotalCount);
        AddAnyLogContent(log, observer::kLatencyNs, TotalLatencyNs);
        AddAnyLogContent(log, observer::kReqBytes, TotalReqBytes);
        AddAnyLogContent(log, observer::kRespBytes, TotalRespBytes);
        AddAnyLogContent(log, observer::kTdigestLatency, std::string("xxxxx"));
    }

    int64_t TotalCount{0};
    int64_t TotalLatencyNs{0};
    int64_t TotalReqBytes{0};
    int64_t TotalRespBytes{0};
};


/**
 * A aggregation result for unique protocol event key.
 * @tparam ProtocolEventKey stores unique protocol keys
 * @tparam ProtocolEventAggResult stores the whole aggregation result for whole input events
 */
template <typename ProtocolEventKey, typename ProtocolEventAggResult>
struct CommonProtocolEventAggItem {
    CommonProtocolEventAggItem() = default;
    void ToPB(sls_logs::Log* log) {
        Key.ToPB(log);
        AggResult.ToPB(log);
    }
    void Merge(CommonProtocolEventAggItem<ProtocolEventKey, ProtocolEventAggResult>& aggItem) {
        AggResult.Merge(aggItem.AggResult);
    }
    void AddEventInfo(CommonProtocolEventInfo& info) { AggResult.AddEventInfo(info); }
    void Clear() { AggResult.Clear(); }

    ProtocolEventKey Key;
    ProtocolEventAggResult AggResult;
};

/**
 * Cache pool
 * @tparam ProtocolEventAggItem reused object.
 */
template <typename ProtocolEventAggItem>
class CommonProtocolEventAggItemManager {
public:
    explicit CommonProtocolEventAggItemManager(size_t maxCount = 128) : mMaxCount(maxCount) {}

    ~CommonProtocolEventAggItemManager() {
        for (auto iter = mUnUsed.begin(); iter != mUnUsed.end(); ++iter) {
            delete *iter;
        }
    }

    /**
     * Create an new object or reuse the cached object.
     * @return an protocol metrics event aggregate node.
     */
    template <typename ProtocolEventKey>
    ProtocolEventAggItem* Create(ProtocolEventKey&& event) {
        ProtocolEventAggItem* item;
        if (mUnUsed.empty()) {
            item = new ProtocolEventAggItem();
            item->Key = std::move(event);
            return item;
        }
        item = mUnUsed.back();
        item->Clear();
        item->Key = std::move(event);
        mUnUsed.pop_back();
        return item;
    }

    /**
     * Delete object when cache is full, or else would cache it.
     * @param item deleting object.
     */
    void Delete(ProtocolEventAggItem* item) {
        if (mUnUsed.size() < mMaxCount) {
            mUnUsed.push_back(item);
            return;
        }
        delete item;
    }

private:
    size_t mMaxCount;
    std::deque<ProtocolEventAggItem*> mUnUsed;
};

// 通用的协议的聚类器实现
template <typename ProtocolEvent, typename ProtocolEventAggItem, typename ProtocolEventAggItemManager>
class CommonProtocolEventAggregator {
public:
    CommonProtocolEventAggregator(uint32_t maxClientAggSize, uint32_t maxServerAggSize)
        : mClientAggMaxSize(maxClientAggSize), mServerAggMaxSize(maxServerAggSize) {}

    ~CommonProtocolEventAggregator() {
        for (auto iter = mProtocolEventAggMap.begin(); iter != mProtocolEventAggMap.end(); ++iter) {
            mAggItemManager.Delete(iter->second);
        }
    }
    bool AddEvent(ProtocolEvent&& event) {
        auto key = event.Key;
        auto hashVal = key.Hash();
        auto findRst = mProtocolEventAggMap.find(hashVal);
        if (findRst == mProtocolEventAggMap.end()) {
            if (isFull(event.Key.ConnKey.Role)) {
                static uint32_t sLastDropTime{0};
                auto now = time(nullptr);
                LOG_DEBUG(sLogger, ("aggregator is full, some events would be dropped", event.Key.ToString()));
                if (now - sLastDropTime > 60) {
                    sLastDropTime = now;
                    LOG_ERROR(sLogger, ("aggregator is full, some events would be dropped", event.Key.ProtocolType()));
                }
                return false;
            }
            auto item = mAggItemManager.Create(std::move(event.Key));
            findRst = mProtocolEventAggMap.insert(std::make_pair(hashVal, item)).first;
        }
        findRst->second->AddEventInfo(event.Info);
        return true;
    }

    void FlushLogs(std::vector<sls_logs::Log>& allData,
                   const std::string& tags,
                   google::protobuf::RepeatedPtrField<sls_logs::Log_Content>& globalTags,
                   uint64_t interval) {
        for (auto iter = mProtocolEventAggMap.begin(); iter != mProtocolEventAggMap.end();) {
            if (iter->second->AggResult.IsEmpty()) {
                mAggItemManager.Delete(iter->second);
                iter = mProtocolEventAggMap.erase(iter);
            } else {
                sls_logs::Log newLog;
                newLog.mutable_contents()->CopyFrom(globalTags);
                AddAnyLogContent(&newLog, observer::kLocalInfo, tags);
                AddAnyLogContent(&newLog, observer::kInterval, interval);
                iter->second->ToPB(&newLog);
                iter->second->Clear(); // wait for next clear
                allData.push_back(std::move(newLog));
                ++iter;
            }
        }
    }


private:
    bool isFull(PacketRoleType role) {
        if (role == PacketRoleType::Client) {
            return this->mProtocolEventAggMap.size() >= mClientAggMaxSize;
        }
        if (role == PacketRoleType::Server) {
            return this->mProtocolEventAggMap.size() >= mServerAggMaxSize;
        }
        return true;
    }
    ProtocolEventAggItemManager mAggItemManager;
    std::unordered_map<uint64_t, ProtocolEventAggItem*> mProtocolEventAggMap;
    uint32_t mClientAggMaxSize;
    uint32_t mServerAggMaxSize;
};

/**
 * For many protocols, they don't have an ID to bind the request and the response, such as mysql.
 * So we would get many false matches for persistent connection.
 * The cache would remove "dirty" data according to the timestamp.
 */
template <typename reqType, typename respType, typename aggregatorType, typename eventType, std::size_t capacity>
class CommonCache {
public:
    explicit CommonCache(aggregatorType* aggregators) : mAggregators(aggregators) {
        for (long unsigned int i = 0; i < capacity; ++i) {
            mRequests[i] = new reqType;
            mResponses[i] = new respType;
        }
    }

    ~CommonCache() {
        for (long unsigned int i = 0; i < capacity; ++i) {
            delete mRequests[i];
            delete mResponses[i];
        }
    }

    // Only add event fail returns false;
    bool InsertReq(std::function<void(reqType* req)> configFunc) {
        configFunc(GetReqPos());
        return TryStitcherByReq();
    }

    // Only add event fail returns false;
    bool InsertResp(std::function<void(respType* resp)> configFunc) {
        configFunc(GetRespPos());
        return TryStitcherByResp();
    }

    bool GarbageCollection(uint64_t expireTimeNs) {
        while (mHeadRequestsIdx <= mTailRequestsIdx) {
            if (this->GetReqFront()->TimeNano < expireTimeNs) {
                ++mHeadRequestsIdx;
                continue;
            }
            break;
        }
        while (mHeadResponsesIdx <= mTailResponsesIdx) {
            if (this->GetRespFront()->TimeNano < expireTimeNs) {
                ++mHeadResponsesIdx;
                continue;
            }
            break;
        }
        return this->GetRequestsSize() == 0 && this->GetResponsesSize() == 0;
    }

    size_t GetRequestsSize() { return this->mTailRequestsIdx - this->mHeadRequestsIdx + 1; }

    size_t GetResponsesSize() { return this->mTailResponsesIdx - this->mHeadResponsesIdx + 1; }

    reqType* GetReqByIndex(size_t index) { return this->mRequests[index & (capacity - 1)]; }

    respType* GetRespByIndex(size_t index) { return this->mResponses[index & (capacity - 1)]; }

    void BindConvertFunc(std::function<bool(reqType* req, respType* resp, eventType&)> func) {
        this->mConvertEventFunc = func;
    }

private:
    bool TryStitcherByReq() {
        auto req = this->GetReqFront();
        auto resp = this->GetRespFront();
        if (req == nullptr || resp == nullptr) {
            return true;
        }
        // pop illegal nodes with sequence when the timeNano of them is less than the front request node.
        while (true) {
            if (resp != nullptr && resp->TimeNano < req->TimeNano) {
                ++this->mHeadResponsesIdx;
                resp = this->GetRespFront();
                continue;
            }
            break;
        }
        if (resp == nullptr) {
            return true;
        }
        eventType event;
        bool success = true;
        if (this->mConvertEventFunc != nullptr && this->mConvertEventFunc(req, resp, event)) {
            success = this->mAggregators->AddEvent(std::move(event));
        }
        ++this->mHeadRequestsIdx;
        ++this->mHeadResponsesIdx;
        return success;
    }

    bool TryStitcherByResp() {
        auto req = this->GetReqFront();
        auto resp = this->GetRespFront();
        if (req == nullptr || resp == nullptr) {
            return true;
        }
        // pop the resp node when the timeNano before the first req node.
        if (resp->TimeNano < req->TimeNano) {
            ++this->mHeadResponsesIdx;
            return true;
        }
        // try to find the most matching req node, that means they are having the most closing timeNano.
        int idx = this->mHeadRequestsIdx;
        while (idx <= this->mTailRequestsIdx) {
            if (this->GetReqByIndex(idx)->TimeNano > resp->TimeNano) {
                break;
            }
            ++idx;
        }
        this->mHeadRequestsIdx = idx - 1;
        req = this->GetReqFront();
        if (req == nullptr) {
            return true;
        }
        eventType event;
        bool success = true;
        if (this->mConvertEventFunc != nullptr && this->mConvertEventFunc(req, resp, event)) {
            LOG_TRACE(sLogger,
                      ("head_req", this->mHeadRequestsIdx)("tail_req", this->mTailRequestsIdx)(
                          "head_resp", this->mHeadRequestsIdx)("tail_resp", this->mTailResponsesIdx));
            success = this->mAggregators->AddEvent(std::move(event));
        }
        ++this->mHeadRequestsIdx;
        ++this->mHeadResponsesIdx;
        return success;
    }

    reqType* GetReqPos() {
        ++this->mTailRequestsIdx;
        if (mTailRequestsIdx - mHeadRequestsIdx == capacity) {
            ++mHeadRequestsIdx;
        }
        return this->mRequests[mTailRequestsIdx & (capacity - 1)];
    }

    reqType* GetReqFront() {
        if (this->mHeadRequestsIdx > this->mTailRequestsIdx) {
            return nullptr;
        }
        return this->mRequests[mHeadRequestsIdx & (capacity - 1)];
    }

    respType* GetRespPos() {
        ++this->mTailResponsesIdx;
        if (mTailResponsesIdx - mHeadResponsesIdx == capacity) {
            ++mHeadResponsesIdx;
        }
        return this->mResponses[mTailResponsesIdx & (capacity - 1)];
    }

    respType* GetRespFront() {
        if (this->mHeadResponsesIdx > this->mTailResponsesIdx) {
            return nullptr;
        }
        return this->mResponses[mHeadResponsesIdx & (capacity - 1)];
    }

private:
    std::array<reqType*, capacity> mRequests;
    std::array<respType*, capacity> mResponses;
    // idx keep increasing
    int64_t mHeadRequestsIdx = 0;
    int64_t mTailRequestsIdx = -1;
    int64_t mHeadResponsesIdx = 0;
    int64_t mTailResponsesIdx = -1;
    aggregatorType* mAggregators;

    std::function<bool(reqType* req, respType* resp, eventType&)> mConvertEventFunc;

    friend class ProtocolUtilUnittest;
};
} // namespace logtail
