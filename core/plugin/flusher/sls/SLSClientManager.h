/*
 * Copyright 2024 iLogtail Authors
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

#include <condition_variable>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "sdk/Client.h"

namespace logtail {

class SLSClientManager {
public:
    enum class EndpointSourceType { LOCAL, REMOTE };
    enum class EndpointSwitchPolicy { DESIGNATED_FIRST, DESIGNATED_LOCKED };

    SLSClientManager(const SLSClientManager&) = delete;
    SLSClientManager& operator=(const SLSClientManager&) = delete;

    static SLSClientManager* GetInstance() {
        static SLSClientManager instance;
        return &instance;
    }

    void Init();
    void Stop();

    EndpointSwitchPolicy GetServerSwitchPolicy() const { return mDataServerSwitchPolicy; }

    void IncreaseAliuidReferenceCntForRegion(const std::string& region, const std::string& aliuid);
    void DecreaseAliuidReferenceCntForRegion(const std::string& region, const std::string& aliuid);

    sdk::Client* GetClient(const std::string& region, const std::string& aliuid, bool createIfNotFound = true);
    bool ResetClientEndpoint(const std::string& aliuid, const std::string& region, time_t curTime);
    void CleanTimeoutClient();

    void AddEndpointEntry(const std::string& region,
                          const std::string& endpoint,
                          bool isProxy,
                          const EndpointSourceType& endpointType);
    void UpdateEndpointStatus(const std::string& region,
                              const std::string& endpoint,
                              bool status,
                              std::optional<uint32_t> latency = std::optional<uint32_t>());

    void ForceUpdateRealIp(const std::string& region);
    void UpdateSendClientRealIp(sdk::Client* client, const std::string& region);

    std::string GetRegionFromEndpoint(const std::string& endpoint); // for backward compatibility
    bool HasNetworkAvailable(); // TODO: remove this function

private:
    enum class EndpointStatus { STATUS_OK_WITH_IP, STATUS_OK_WITH_ENDPOINT, STATUS_ERROR };

    struct EndpointInfo {
        bool mValid = true;
        std::optional<uint32_t> mLatencyMs;
        bool mProxy = false;

        EndpointInfo(bool valid, bool proxy) : mValid(valid), mProxy(proxy) {}

        void UpdateInfo(bool valid, std::optional<uint32_t> latency) {
            mValid = valid;
            mLatencyMs = latency;
        }
    };

    struct RegionEndpointsInfo {
        std::unordered_map<std::string, EndpointInfo> mEndpointInfoMap;
        std::string mDefaultEndpoint;
        EndpointSourceType mDefaultEndpointType;

        bool AddDefaultEndpoint(const std::string& endpoint, const EndpointSourceType& endpointType, bool& isDefault);
        bool AddEndpoint(const std::string& endpoint, bool status, bool proxy = false);
        void UpdateEndpointInfo(const std::string& endpoint,
                                bool status,
                                std::optional<uint32_t> latency,
                                bool createFlag = true);
        void RemoveEndpoint(const std::string& endpoint);
        std::string GetAvailableEndpointWithTopPriority() const;
    };

    struct RealIpInfo {
        std::string mRealIp;
        time_t mLastUpdateTime = 0;
        bool mForceFlushFlag = false;

        void SetRealIp(const std::string& realIp) {
            mRealIp = realIp;
            mLastUpdateTime = time(NULL);
            mForceFlushFlag = false;
        }
    };

    SLSClientManager() = default;
    ~SLSClientManager() = default;

    void InitEndpointSwitchPolicy();
    std::vector<std::string> GetRegionAliuids(const std::string& region);

    void ResetClientPort(const std::string& region, sdk::Client* sendClient);
    std::string GetAvailableEndpointWithTopPriority(const std::string& region) const;

    void ProbeNetworkThread();
    bool TestEndpoint(const std::string& region, const std::string& endpoint);

    void UpdateRealIpThread();
    EndpointStatus UpdateRealIp(const std::string& region, const std::string& endpoint);
    void SetRealIp(const std::string& region, const std::string& ip);

    mutable std::mutex mRegionAliuidRefCntMapLock;
    std::unordered_map<std::string, std::unordered_map<std::string, uint32_t>> mRegionAliuidRefCntMap;

    mutable std::mutex mClientMapMux;
    std::unordered_map<std::string, std::pair<std::unique_ptr<sdk::Client>, time_t>> mClientMap;
    // int32_t mLastCheckSendClientTime;

    mutable std::mutex mRegionEndpointEntryMapLock;
    std::unordered_map<std::string, RegionEndpointsInfo> mRegionEndpointEntryMap;
    EndpointSwitchPolicy mDataServerSwitchPolicy = EndpointSwitchPolicy::DESIGNATED_FIRST;
    std::unique_ptr<sdk::Client> mProbeNetworkClient;

    std::future<void> mProbeNetworkThreadRes;
    mutable std::mutex mProbeNetworkThreadRunningMux;
    bool mIsProbeNetworkThreadRunning = true;

    mutable std::mutex mRegionRealIpLock;
    std::unordered_map<std::string, RealIpInfo*> mRegionRealIpMap;
    std::unique_ptr<sdk::Client> mUpdateRealIpClient;

    std::future<void> mUpdateRealIpThreadRes;
    mutable std::mutex mUpdateRealIpThreadRunningMux;
    bool mIsUpdateRealIpThreadRunning = true;

    mutable std::condition_variable mStopCV;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherSLSUnittest;
#endif
};

} // namespace logtail
