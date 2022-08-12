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
#include "log_pb/sls_logs.pb.h"
#include "Logger.h"
#include "interface/helper.h"
#include "LogtailAlarm.h"
#include "metas/ServiceMetaCache.h"
#include <unordered_map>
#include <ostream>

namespace logtail {

template <typename T>
inline void AddAnyLogContent(sls_logs::Log* log, const std::string& key, const T& value) {
    auto content = log->add_contents();
    content->set_key(key);
    content->set_value(std::to_string(value));
}

template <>
inline void AddAnyLogContent(sls_logs::Log* log, const std::string& key, const std::string& value) {
    auto content = log->add_contents();
    content->set_key(key);
    content->set_value(value);
}

struct CommonAggKey {
    CommonAggKey() = default;
    CommonAggKey(PacketEventHeader* header)
        : HashVal(0),
          RemoteIp(SockAddressToString(header->DstAddr)),
          RemotePort(std::to_string(header->RoleType == PacketRoleType::Server ? 0 : header->DstPort)),
          Role(PacketRoleTypeToString(header->RoleType)),
          Pid(header->PID) {
        HashVal = XXH32(Role.c_str(), Role.size(), HashVal);
        HashVal = XXH32(RemoteIp.c_str(), RemoteIp.size(), HashVal);
        HashVal = XXH32(RemotePort.c_str(), RemotePort.size(), HashVal);
    }

    friend std::ostream& operator<<(std::ostream& os, const CommonAggKey& key) {
        os << "HashVal: " << key.HashVal << " RemoteIp: " << key.RemoteIp << " RemotePort: " << key.RemotePort
           << " Role: " << key.Role << " Pid: " << key.Pid;
        return os;
    }

    void ToPB(sls_logs::Log* log, ProtocolType protocolType) const {
        static auto sServiceMetaManager = ServiceMetaManager::GetInstance();
        auto content = log->add_contents();
        content->set_key("role");
        content->set_value(Role);
        Json::Value root;
        root["remote_ip"] = RemoteIp;
        root["remote_port"] = RemotePort;
        if (Role == "client") {
            auto& serviceMeta = sServiceMetaManager->GetOrPutServiceMeta(Pid, RemoteIp, protocolType);
            root["remote_type"] = ServiceCategoryToString(
                serviceMeta.Empty() ? DetectRemoteServiceCategory(protocolType) : serviceMeta.Category);
            if (!serviceMeta.Host.empty()) {
                root["remote_host"] = serviceMeta.Host;
            }
        }
        content = log->add_contents();
        content->set_key("remote_info");
        content->set_value(Json::FastWriter().write(root));
    }

    uint64_t HashVal;
    std::string RemoteIp;
    std::string RemotePort;
    std::string Role;
    uint32_t Pid;
};

struct CommonProtocolEventInfo {
    CommonProtocolEventInfo() : LatencyNs(0), ReqBytes(0), RespBytes(0) {}

    void ToPB(sls_logs::Log* log) const {
        AddAnyLogContent(log, "latency_ns", LatencyNs);
        AddAnyLogContent(log, "req_bytes", ReqBytes);
        AddAnyLogContent(log, "resp_bytes", RespBytes);
    }

    friend std::ostream& operator<<(std::ostream& os, const CommonProtocolEventInfo& info) {
        os << "LatencyNs: " << info.LatencyNs << " ReqBytes: " << info.ReqBytes << " RespBytes: " << info.RespBytes;
        return os;
    }

    std::string ToString() {
        std::stringstream ss;
        ss << *this;
        return ss.str();
    }

    int64_t LatencyNs;
    int32_t ReqBytes;
    int32_t RespBytes;
};

struct CommonProtocolAggResult {
    CommonProtocolAggResult() : TotalCount(0), TotalLatencyNs(0), TotalReqBytes(0), TotalRespBytes(0) {}
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
        AddAnyLogContent(log, "total_count", TotalCount);
        AddAnyLogContent(log, "total_latency_ns", TotalLatencyNs);
        AddAnyLogContent(log, "total_req_bytes", TotalReqBytes);
        AddAnyLogContent(log, "total_resp_bytes", TotalRespBytes);
    }

    int64_t TotalCount;
    int64_t TotalLatencyNs;
    int64_t TotalReqBytes;
    int64_t TotalRespBytes;
};

template <typename ProtocolEventKey>
struct CommonProtocolEvent {
    void GetKey(ProtocolEventKey& key) { key = Key; }

    void ToPB(sls_logs::Log* log) {
        Key.ToPB(log);
        Info.ToPB(log);
    }

    ProtocolEventKey Key;
    CommonProtocolEventInfo Info;
};

template <typename ProtocolEventKey, typename ProtocolEventAggResult>
struct CommonProtocolEventAggItem {
    CommonProtocolEventAggItem() = default;

    explicit CommonProtocolEventAggItem(const ProtocolEventKey& key) : Key(key) {}

    void ToPB(sls_logs::Log* log) {
        Key.ToPB(log);
        AggResult.ToPB(log);
    }

    const ProtocolEventKey& GetKey() const { return Key; }

    void SetKey(const ProtocolEventKey& key) { Key = key; }

    // AddEvent 更新聚合结果
    template <typename ProtocolEvent>
    void AddEvent(ProtocolEvent* event) {
        AggResult.AddEventInfo(event->Info);
    }

    void Merge(CommonProtocolEventAggItem<ProtocolEventKey, ProtocolEventAggResult>& aggItem) {
        AggResult.Merge(aggItem.AggResult);
    }

    // Clear 清理聚合结果
    void Clear() { AggResult.Clear(); }

    ProtocolEventKey Key;
    ProtocolEventAggResult AggResult;
};

template <typename ProtocolEventAggItem>
struct CommonProtocolEventAggItemManager {
    explicit CommonProtocolEventAggItemManager(size_t maxCount = 128) : mMaxCount(maxCount) {}

    ~CommonProtocolEventAggItemManager() {
        for (auto iter = mUnUsed.begin(); iter != mUnUsed.end(); ++iter) {
            delete *iter;
        }
    }

    ProtocolEventAggItem* Create() {
        if (!mUnUsed.empty()) {
            ProtocolEventAggItem* item = mUnUsed.back();
            item->Clear();
            mUnUsed.pop_back();
            return item;
        }
        ProtocolEventAggItem* item = new ProtocolEventAggItem();
        return item;
    }

    void Delete(ProtocolEventAggItem* item) {
        if (mUnUsed.size() < mMaxCount) {
            mUnUsed.push_back(item);
            return;
        }
        delete item;
    }

    size_t mMaxCount;
    std::deque<ProtocolEventAggItem*> mUnUsed;
};

template <typename ProtocolEventKey>
struct CommonProtocolPatternGenerator {
    static CommonProtocolPatternGenerator<ProtocolEventKey>* GetInstance() {
        static CommonProtocolPatternGenerator<ProtocolEventKey>* sCommonProtocolPatternGenerator
            = new CommonProtocolPatternGenerator<ProtocolEventKey>();
        return sCommonProtocolPatternGenerator;
    }

    ProtocolEventKey& GetPattern(ProtocolEventKey& key) { return key; }
};

// 通用的协议的聚类器实现
template <typename ProtocolEventKey,
          typename ProtocolEvent,
          typename ProtocolEventAggItem,
          typename ProtocolPatternGenerator,
          typename ProtocolEventAggItemManager,
          ProtocolType PT>
class CommonProtocolEventAggregator {
public:
    CommonProtocolEventAggregator(uint32_t maxClientAggSize, uint32_t maxServerAggSize)
        : mPatternGenerator(*(ProtocolPatternGenerator::GetInstance())),
          mClientAggMaxSize(maxClientAggSize),
          mServerAggMaxSize(maxServerAggSize) {}

    ~CommonProtocolEventAggregator() {
        for (auto iter = mProtocolEventAggMap.begin(); iter != mProtocolEventAggMap.end(); ++iter) {
            mAggItemManager.Delete(iter->second);
        }
    }

    // AddEvent 增加一个事件
    bool AddEvent(ProtocolEvent* event) {
        ProtocolEventKey eventKey;
        event->GetKey(eventKey);
        ProtocolEventKey& patternKey = mPatternGenerator.GetPattern(eventKey);
        uint64_t hashVal = patternKey.Hash();
        auto findRst = mProtocolEventAggMap.find(hashVal);
        if (findRst == mProtocolEventAggMap.end()) {
            if ((eventKey.ConnKey.Role == PacketRoleTypeToString(PacketRoleType::Client)
                 && this->mProtocolEventAggMap.size() >= mClientAggMaxSize)
                || (eventKey.ConnKey.Role == PacketRoleTypeToString(PacketRoleType::Server)
                    && this->mProtocolEventAggMap.size() >= mServerAggMaxSize)) {
                if (sLogger->should_log(spdlog::level::debug)) {
                    LogMaker maker;
                    sLogger->log(spdlog::level::debug,
                                 "{}:{}\t{}",
                                 __FILE__,
                                 __LINE__,
                                 maker("aggregator is full, the event would be dropped", ProtocolTypeToString(PT))(
                                     "key", event->Key.ToString())("info", event->Info.ToString())
                                     .GetContent());
                }
                static uint32_t sLastDropTime;
                auto now = time(NULL);
                if (now - sLastDropTime > 60) {
                    sLastDropTime = now;
                    if (sLogger->should_log(spdlog::level::err)) {
                        LogMaker maker;
                        sLogger->log(spdlog::level::err,
                                     "{}:{}\t{}",
                                     __FILE__,
                                     __LINE__,
                                     maker("aggregator is full, some events would be dropped", ProtocolTypeToString(PT))
                                         .GetContent());
                    }
                }
                return false;
            }
            ProtocolEventAggItem* item = mAggItemManager.Create();
            item->SetKey(patternKey);
            findRst = mProtocolEventAggMap.insert(std::make_pair(hashVal, item)).first;
        }
        findRst->second->AddEvent(event);
        return true;
    }

    void FlushLogs(std::vector<sls_logs::Log>& allData,
                   ::google::protobuf::RepeatedPtrField<sls_logs::Log_Content>& tags) {
        for (auto iter = mProtocolEventAggMap.begin(); iter != mProtocolEventAggMap.end();) {
            if (iter->second->AggResult.IsEmpty()) {
                mAggItemManager.Delete(iter->second);
                iter = mProtocolEventAggMap.erase(iter);
            } else {
                sls_logs::Log newLog;
                newLog.mutable_contents()->CopyFrom(tags);
                auto content = newLog.add_contents();
                content->set_key("protocol");
                content->set_value(ProtocolTypeToString(PT));
                iter->second->ToPB(&newLog);
                // wait for next clear
                iter->second->Clear();
                allData.push_back(newLog);
                ++iter;
            }
        }
    }

protected:
    ProtocolPatternGenerator& mPatternGenerator;
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
        for (int i = 0; i < capacity; ++i) {
            mRequests[i] = new reqType;
            mResponses[i] = new respType;
        }
    }

    ~CommonCache() {
        for (int i = 0; i < capacity; ++i) {
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
            success = this->mAggregators->AddEvent(&event);
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
            success = this->mAggregators->AddEvent(&event);
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
