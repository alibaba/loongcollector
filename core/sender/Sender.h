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
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>
#include <atomic>
#include "common/LogstoreSenderQueue.h"
#include "common/WaitObject.h"
#include "common/Lock.h"
#include "common/Thread.h"
#include "sdk/Closure.h"
#include "log_pb/sls_logs.pb.h"
#include "log_pb/logtail_buffer_meta.pb.h"
#include "aggregator/Aggregator.h"
#include "SenderQueueParam.h"

namespace logtail {

namespace sdk {
    class Client;
}

enum OperationOnFail { RETRY_ASYNC_WHEN_FAIL, RECORD_ERROR_WHEN_FAIL, DISCARD_WHEN_FAIL };

enum SEND_THREAD_TYPE { REALTIME_SEND_THREAD = 0, REPLAY_SEND_THREAD = 1, SEND_THREAD_TYPE_COUNT = 2 };

enum EndpointStatus { STATUS_OK_WITH_IP = 0, STATUS_OK_WITH_ENDPOINT, STATUS_ERROR };

struct EndpointDetail {
    bool mStatus;
    bool mProxyFlag;
    int32_t mLatency; //ms

    EndpointDetail(bool status, int32_t latency, bool proxy) {
        mStatus = status;
        mLatency = latency >= 0 ? latency : 0;
        mProxyFlag = proxy;
    }

    void SetDetail(bool status, int32_t latency) {
        mStatus = status;
        if (latency >= 0)
            mLatency = latency;
    }
};

struct RegionEndpointEntry {
    std::unordered_map<std::string, EndpointDetail> mEndpointDetailMap;
    std::string mDefaultEndpoint;

    // Control send concurrency of region, -1 means no limit.
    // It will be updated by SendClosure OnSuccess and OnFail.
    int32_t mConcurrency = -1;
    // To avoid occasional error.
    int32_t mContinuousErrorCount = 0;

    RegionEndpointEntry() {
        mDefaultEndpoint.clear();
        mEndpointDetailMap.clear();
    }

    void AddDefaultEndpoint(const std::string& endpoint) {
        mDefaultEndpoint = endpoint;
        AddEndpoint(endpoint, true, 0, false);
    }

    void AddEndpoint(const std::string& endpoint, bool status, int32_t latency, bool proxy = false) {
        if (mEndpointDetailMap.find(endpoint) == mEndpointDetailMap.end())
            mEndpointDetailMap.insert(std::make_pair(endpoint, EndpointDetail(status, latency, proxy)));
    }

    void RemoveEndpoint(const std::string& endpoint) {
        mEndpointDetailMap.erase(endpoint);
        if (mDefaultEndpoint == endpoint)
            mDefaultEndpoint.clear();
    }

    std::string GetCurrentEndpoint() {
        if (mDefaultEndpoint.size() > 0) {
            std::unordered_map<std::string, EndpointDetail>::iterator iter = mEndpointDetailMap.find(mDefaultEndpoint);
            if (iter != mEndpointDetailMap.end() && (iter->second).mStatus)
                return mDefaultEndpoint;
        }
        std::string proxyEndpoint;
        for (std::unordered_map<std::string, EndpointDetail>::iterator iter = mEndpointDetailMap.begin();
             iter != mEndpointDetailMap.end();
             ++iter) {
            if (!(iter->second).mStatus)
                continue;
            else if ((iter->second).mStatus && !(iter->second).mProxyFlag)
                return iter->first;
            else
                proxyEndpoint = iter->first;
        }
        if (proxyEndpoint.size() > 0)
            return proxyEndpoint;
        if (mDefaultEndpoint.size() > 0)
            return mDefaultEndpoint;
        else if (mEndpointDetailMap.size() > 0)
            return mEndpointDetailMap.begin()->first;
        else
            return mDefaultEndpoint;
    }

    void UpdateEndpointDetail(const std::string& endpoint, bool status, int32_t latency, bool createFlag = true) {
        std::unordered_map<std::string, EndpointDetail>::iterator iter = mEndpointDetailMap.find(endpoint);
        if (iter == mEndpointDetailMap.end()) {
            if (createFlag)
                AddEndpoint(endpoint, status, latency);
        } else
            (iter->second).SetDetail(status, latency);
    }
};

class SendClosure : public sdk::PostLogStoreLogsClosure {
public:
    virtual void OnSuccess(sdk::Response* response);
    virtual void OnFail(sdk::Response* response, const std::string& errorCode, const std::string& errorMessage);
    LoggroupTimeValue* mDataPtr;
};

struct SlsClientInfo {
    sdk::Client* sendClient;
    int32_t lastUsedTime;

    SlsClientInfo(sdk::Client* client, int32_t updateTime);
};

enum SendResult {
    SEND_OK,
    SEND_NETWORK_ERROR,
    SEND_QUOTA_EXCEED,
    SEND_UNAUTHORIZED,
    SEND_SERVER_ERROR,
    SEND_DISCARD_ERROR,
    SEND_INVALID_SEQUENCE_ID
};
SendResult ConvertErrorCode(const std::string& errorCode);

class Sender {
private:
    Sender();
    Sender(const Sender&);
    Sender& operator=(const Sender&);
    bool WriteToFile(const std::string& projectName, const sls_logs::LogGroup& logGroup, bool sendPerformance);
    bool WriteToFile(LoggroupTimeValue* value, bool sendPerformance);
    bool DumpDebugFile(LoggroupTimeValue* value, bool sendPerformance = false);
    void DaemonSender();
    void SendToNetAsync(LoggroupTimeValue* dataPtr);
    SendResult SendToNetSync(sdk::Client* sendClient,
                             const sls_logs::LogtailBufferMeta& bufferMeta,
                             const std::string& logData,
                             std::string& errorCode);
    SendResult SendBufferFileData(const sls_logs::LogtailBufferMeta& bufferMeta,
                                  const std::string& logData,
                                  std::string& errorCode);
    bool SendToBufferFile(LoggroupTimeValue* dataPtr);
    void FlowControl(int32_t dataSize, SEND_THREAD_TYPE type);

    bool IsValidToSend(const LogstoreFeedBackKey& logstoreKey);

    PTMutex mRegionEndpointEntryMapLock;
    std::unordered_map<std::string, RegionEndpointEntry*> mRegionEndpointEntryMap;

    WaitObject mWriteSecondaryWait; // semaphore between SendThreads & DumpSecondaryThread
    PTMutex mSecondaryMutexLock; // lock for mSecondaryBuffer
    std::vector<LoggroupTimeValue*> mSecondaryBuffer;

    // for flow control: value[0] for realtime thread, value[1] for replay thread
    int64_t mSendLastTime[SEND_THREAD_TYPE_COUNT];
    int32_t mSendLastByte[SEND_THREAD_TYPE_COUNT];

    LogstoreSenderQueue<SenderQueueParam> mSenderQueue;

    // for encryption buffer file
    struct EncryptionStateMeta {
        int32_t mLogDataSize;
        int32_t mEncryptionSize;
        int32_t mEncodedInfoSize;
        int32_t mTimeStamp;
        int32_t mHandled;
        int32_t mRetryTime;
    };

    volatile bool mFlushLog;
    std::string mBufferFilePath;
    std::atomic_int mSendingLogGroupCount{0};
    std::atomic_int mSendingBufferCount{0};
    //buffer file named before this unixtime can be read by daemon buffer sender thread
    //buffer file named after this unixtime maybe occupied by daemon sender thread at the moment
    volatile time_t mBufferDivideTime;
    volatile bool mIsSendingBuffer;
    std::string mBufferFileName;

    struct SendStatistic {
        int32_t mBeginTime;
        int32_t mServerErrorCount;
        int32_t mSendCount;

        SendStatistic(int32_t beginTime) {
            mBeginTime = beginTime;
            mServerErrorCount = 0;
            mSendCount = 0;
        }
    };

    PTMutex mSendStatisticLock;
    std::unordered_map<std::string, std::vector<SendStatistic*> > mSendStatisticMap;

    PTMutex mSendClientLock;
    std::unordered_map<std::string, SlsClientInfo*> mSendClientMap;
    int32_t mLastCheckSendClientTime;

    sdk::Client* mTestNetworkClient;

    // SendBufferThread: SecondaryFile -> SLS
    ThreadPtr mSendBufferThreadId;
    volatile bool mBufferSenderThreadIsRunning;
    WaitObject mBufferWait;

    std::atomic_int mLastDaemonRunTime{0};
    std::atomic_int mLastSendDataTime{0};
    std::atomic<int64_t> mLogGroupContextSeq{0};

    int64_t mCheckPeriod;
    SpinLock mBufferFileLock; //get set bufferfilepath and buffer filename

    struct RealIpInfo {
        RealIpInfo() : mLastUpdateTime(0), mForceFlushFlag(false) {}
        void SetRealIp(const std::string& realIp) {
            mRealIp = realIp;
            mLastUpdateTime = time(NULL);
            mForceFlushFlag = false;
        }
        std::string mRealIp;
        int32_t mLastUpdateTime;
        bool mForceFlushFlag;
    };

    typedef std::unordered_map<std::string, RealIpInfo*> RegionRealIpInfoMap;
    RegionRealIpInfoMap mRegionRealIpMap;
    sdk::Client* mUpdateRealIpClient;
    PTMutex mRegionRealIpLock;
    bool mStopRealIpThread = false;

    const static std::string BUFFER_FILE_NAME_PREFIX;
    const static int32_t BUFFER_META_BASE_SIZE;

    void ForceUpdateRealIp(const std::string& region);
    void UpdateSendClientRealIp(sdk::Client* client, const std::string& region);
    void RealIpUpdateThread();
    EndpointStatus UpdateRealIp(const std::string& region, const std::string& endpoint);
    void SetRealIp(const std::string& region, const std::string& ip);

    void DaemonBufferSender();
    void WriteSecondary();
    bool LoadFileToSend(time_t timeLine, std::vector<std::string>& filesToSend);
    bool CreateNewFile();
    bool WriteBackMeta(const int32_t pos, const void* buf, int32_t length, const std::string& filename);
    bool ReadNextEncryption(int32_t& pos,
                            const std::string& filename,
                            std::string& encryption,
                            EncryptionStateMeta& meta,
                            bool& readResult,
                            sls_logs::LogtailBufferMeta& bufferMeta);
    void SendEncryptionBuffer(const std::string& filename, int32_t keyVersion);

    void ResetSendingCount();
    void IncSendingCount(int32_t val = 1);
    void DescSendingCount();
    bool IsSecondaryBufferEmpty();
    int32_t GetSendingCount();
    void SetSendingBufferCount(int32_t count);
    void AddSendingBufferCount();
    void SubSendingBufferCount();

    bool IsBatchMapEmpty();
    void PutIntoSecondaryBuffer(LoggroupTimeValue* dataPtr, int32_t retryTimes);

    /*
     * add memory barrier for config varibles.
     */
    void SetBufferFilePath(const std::string& bufferfilepath);
    std::string GetBufferFilePath();
    std::string GetBufferFileName();
    void SetBufferFileName(const std::string& filename);
    std::string GetBufferFileHeader();
    void TestNetwork();
    bool TestEndpoint(const std::string& region, const std::string& endpoint);
    void PutIntoBatchMap(LoggroupTimeValue* data);

    double IncTotalSendStatistic(const std::string& projectName, const std::string& logstore, int32_t curTime);
    double IncSendServerErrorStatistic(const std::string& projectName, const std::string& logstore, int32_t curTime);
    double UpdateSendStatistic(const std::string& key, int32_t curTime, bool serverError);
    void CleanTimeoutSendStatistic();

    //bool CheckBatchMapFull(int64_t key);
    static bool IsProfileData(const std::string& region, const std::string& project, const std::string& logstore);

    std::string GetRegionCurrentEndpoint(const std::string& region);
    std::string GetRegionFromEndpoint(const std::string& endpoint);

    void AddTruncateInfo(int64_t key, const std::string& truncateinfo);
    std::string GetTruncateInfo(int64_t key);
    void DelTruncateInfo(int64_t key);

public:
    static Sender* Instance();
    //void ResetProfileSender();
    bool InitSender(); //Backward compatible
    // from collector to batchmap
    bool Send(const std::string& projectName,
              const std::string& sourceId,
              sls_logs::LogGroup& logGroup,
              const Config* config,
              DATA_MERGE_TYPE mergeType,
              const uint32_t logGroupSize,
              const std::string& defaultRegion = "",
              const std::string& filename = "",
              const LogGroupContext& context = LogGroupContext());
    //bool LoadConfig(const Json::Value& secondary);

    // added by xianzhi(bowen.gbw@antfin.com)
    // no merge and wait, send instantly
    // WARNING: now this function can only be called when sending integrity info
    bool SendInstantly(sls_logs::LogGroup& logGroup,
                       const std::string& aliuid,
                       const std::string& region,
                       const std::string& projectName,
                       const std::string& logstore);

    bool RemoveSender();
    bool FlushOut(int32_t time_interval_in_mili_seconds);

    ~Sender();

    bool HasNetworkAvailable();
    void SetNetworkStat(const std::string& region, const std::string& endpoint, bool status, int32_t latency = -1);

    sdk::Client* GetSendClient(const std::string& region, const std::string& aliuid);

    bool ResetSendClientEndpoint(const std::string aliuid, const std::string region, int32_t curTime);
    void CleanTimeoutSendClient();

    //for debug & ut
    void (*MockAsyncSend)(const std::string& projectName,
                          const std::string& logstore,
                          const std::string& logData,
                          SEND_DATA_TYPE dataType,
                          int32_t rawSize,
                          SendClosure* sendClosure);
    void (*MockSyncSend)(const std::string& projectName,
                         const std::string& logstore,
                         const std::string& logData,
                         SEND_DATA_TYPE dataType,
                         int32_t rawSize);
    void (*MockTestEndpoint)(const std::string& projectName,
                             const std::string& logstore,
                             const std::string& logData,
                             SEND_DATA_TYPE dataType,
                             int32_t rawSize);
    void (*MockIntegritySend)(LoggroupTimeValue* data);
    sdk::GetRealIpResponse (*MockGetRealIp)(const std::string& projectName, const std::string& logstore);
    static bool ParseLogGroupFromLZ4(const std::string& logData, int32_t rawSize, sls_logs::LogGroup& logGroupPb);
    static void ParseLogGroupFromString(const std::string& logData,
                                        SEND_DATA_TYPE dataType,
                                        int32_t rawSize,
                                        std::vector<sls_logs::LogGroup>& logGroupVec);
    static bool LZ4CompressLogGroup(const sls_logs::LogGroup& logGroup, std::string& compressed, int32_t& rawSize);

    void AddEndpointEntry(const std::string& region,
                          const std::string& endpoint,
                          bool isDefault = false,
                          bool isProxy = false);
    LogstoreFeedBackInterface* GetSenderFeedBackInterface();
    void SetFeedBackInterface(LogstoreFeedBackInterface* pProcessInterface);
    void OnSendDone(LoggroupTimeValue* mDataPtr, LogstoreSenderInfo::SendResult sendRst);

    bool IsFlush();
    void SetFlush();
    void ResetFlush();

    void SetQueueUrgent();
    void ResetQueueUrgent();

    void IncreaseRegionConcurrency(const std::string& region);
    void ResetRegionConcurrency(const std::string& region);

    int32_t GetSendingBufferCount();

    int32_t GetLastDeamonRunTime() { return mLastDaemonRunTime; }

    int32_t GetLastSendTime() { return mLastSendDataTime; }

    void RestLastSenderTime() {
        mLastDaemonRunTime = 0;
        mLastSendDataTime = 0;
    }

    LogstoreSenderQueue<SenderQueueParam>& GetQueue() { return mSenderQueue; }

    LogstoreSenderStatistics GetSenderStatistics(const LogstoreFeedBackKey& key);
    void
    SetLogstoreFlowControl(const LogstoreFeedBackKey& logstoreKey, int32_t maxSendBytesPerSecond, int32_t expireTime);
    bool SendPb(Config* pConfig,
                char* pbBuffer,
                int32_t pbSize,
                int32_t lines,
                const std::string& logstore = "",
                const std::string& shardHash = "");

    void SendLZ4Compressed(const std::string& projectName,
                           sls_logs::LogGroup& logGroup,
                           const std::vector<int32_t>& neededLogIndex,
                           const std::string& configName,
                           const std::string& aliuid,
                           const std::string& region,
                           const std::string& filename,
                           const LogGroupContext& context);

    void SendLZ4Compressed(std::vector<MergeItem*>& sendDataVec);
    void SendLogPackageList(std::vector<MergeItem*>& sendDataVec);

    friend class SendClosure;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SenderUnittest;
    friend class ConfigUpdatorUnittest;
    friend class FuxiSceneUnittest;
#endif
};

} // namespace logtail
