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

// #include <curl/curl.h>

// #include <atomic>
#include <chrono>
// #include <condition_variable>
#include <cstdint>
// #include <ctime>
// #include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
// #include <unordered_map>
// #include <unordered_set>
#include <vector>

// #include "common/http/HttpRequest.h"
// #include "common/http/HttpResponse.h"
#include "pipeline/queue/SenderQueueItem.h"
#include "plugin/flusher/sls/SLSResponse.h"

namespace logtail {

enum class EndpointMode { DEFAULT, ACCELERATE, CUSTOM };
const std::string& EndpointModeToString(EndpointMode mode);

struct CandidateEndpoints {
    // currently only remote endpoints can be updated
    EndpointMode mMode = EndpointMode::DEFAULT;
    // when mMode = ACCELERATE, mLocalEndpoints should be empty
    std::vector<std::string> mLocalEndpoints;
    std::vector<std::string> mRemoteEndpoints;
};

class HostInfo {
public:
    HostInfo(const std::string& hostname) : mHostname(hostname) {}
    HostInfo(const HostInfo& rhs) : mHostname(rhs.mHostname) { mLatency = rhs.GetLatency(); }

    const std::string& GetHostname() const { return mHostname; }
    std::chrono::milliseconds GetLatency() const;
    void SetLatency(const std::chrono::milliseconds& latency);
    void SetForbidden();
    bool IsForbidden() const;

private:
    // normally in the form of <project>.<endpoint>, except for the real ip scene, which equals to endpoint
    const std::string mHostname;

    mutable std::mutex mLatencyMux;
    std::chrono::milliseconds mLatency = std::chrono::milliseconds::max();
};

class CandidateHostsInfo {
public:
    CandidateHostsInfo(const std::string& project, const std::string& region, EndpointMode mode)
        : mProject(project), mRegion(region), mMode(mode) {}

    void UpdateHosts(const CandidateEndpoints& regionEndpoints);
    bool UpdateHostInfo(const std::string& hostname, const std::chrono::milliseconds& latency);
    void GetProbeHosts(std::vector<std::string>& hosts) const;
    void GetAllHosts(std::vector<std::string>& hosts) const;
    void SelectBestHost();
    std::string GetCurrentHost() const;
    std::string GetFirstHost() const;
    const std::string& GetProject() const { return mProject; }
    const std::string& GetRegion() const { return mRegion; }
    EndpointMode GetMode() const { return mMode; }
    bool IsInitialized() const { return mInitialized.load(); }
    void SetInitialized() { mInitialized = true; }

private:
    bool HasValidHost() const;
    void SetCurrentHost(const std::string& host);

    // for real ip scene, mProject is empty
    const std::string mProject;
    const std::string mRegion;
    const EndpointMode mMode;
    std::atomic_bool mInitialized = false;

    mutable std::mutex mCurrentHostMux;
    std::string mCurrentHost;

    // mMode = DEFAULT: each mCandidateHosts element has exactly one element
    // mMode = ACCELERATE: mCandidateHosts.size() == 1 && mCandidateHosts[0].size() == 2
    // mMode = CUSTOM: mCandidateHosts.size() == 1 && mCandidateHosts[0].size() == 1
    mutable std::mutex mCandidateHostsMux;
    std::vector<std::vector<HostInfo>> mCandidateHosts;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CandidateHostsInfoUnittest;
    friend class SLSClientManagerUnittest;
    friend class EnterpriseSLSClientManagerUnittest;
#endif
};

class SLSClientManager {
public:
    enum class AuthType { ANONYMOUS, AK };
    // enum class RemoteEndpointUpdateAction { CREATE, OVERWRITE, APPEND };

    virtual ~SLSClientManager() = default;
    SLSClientManager(const SLSClientManager&) = delete;
    SLSClientManager& operator=(const SLSClientManager&) = delete;

    static SLSClientManager* GetInstance();

    virtual void Init();
    virtual void Stop() {};

    const std::string& GetUserAgent() const { return mUserAgent; }

    virtual bool
    GetAccessKey(const std::string& aliuid, AuthType& type, std::string& accessKeyId, std::string& accessKeySecret);
    virtual void UpdateAccessKeyStatus(const std::string& aliuid, bool success) {}

    // // currently not support hot relead
    // void UpdateLocalRegionEndpointsAndHttpsInfo(const std::string& region, const std::vector<std::string>&
    // endpoints); void UpdateRemoteRegionEndpoints(const std::string& region,
    //                                  const std::vector<std::string>& endpoints,
    //                                  RemoteEndpointUpdateAction action = RemoteEndpointUpdateAction::OVERWRITE);

    // std::shared_ptr<CandidateHostsInfo>
    // GetCandidateHostsInfo(const std::string& region, const std::string& project, EndpointMode mode);
    // bool UpdateHostInfo(const std::string& project,
    //                     EndpointMode mode,
    //                     const std::string& host,
    //                     const std::chrono::milliseconds& latency);
    // // only for real ip
    // bool UpdateHostInfo(const std::string& region, const std::string& host, const std::chrono::milliseconds&
    // latency);

    // only for open source
    std::shared_ptr<CandidateHostsInfo> GetCandidateHostsInfo(const std::string& project, const std::string& endpoint);

    virtual bool UsingHttps(const std::string& region) const;

    // void UpdateOutdatedRealIpRegions(const std::string& region);
    // std::string GetRealIp(const std::string& region) const;

#ifdef APSARA_UNIT_TEST_MAIN
    virtual void Clear();
#endif

protected:
    SLSClientManager() = default;

    virtual std::string GetRunningEnvironment();
    bool PingEndpoint(const std::string& host, const std::string& path);

    std::string mUserAgent;

private:
    // struct ProbeNetworkHttpRequest : public AsynHttpRequest {
    //     std::string mRegion;
    //     std::string mProject;
    //     EndpointMode mMode;
    //     std::string mHost;

    //     ProbeNetworkHttpRequest(const std::string& region,
    //                             const std::string& project,
    //                             EndpointMode mode,
    //                             const std::string& host,
    //                             bool httpsFlag);

    //     bool IsContextValid() const override { return true; };
    //     void OnSendDone(HttpResponse& response) override;
    // };

    virtual void GenerateUserAgent();

    // void UnInitializedHostProbeThread();
    // void DoProbeUnInitializedHost();
    // void HostProbeThread();
    // void DoProbeHost();
    // bool HasPartiallyInitializedCandidateHostsInfos() const;
    // void ClearExpiredCandidateHostsInfos();

    // void UpdateRealIpThread();
    // void DoUpdateRealIp();
    // void SetRealIp(const std::string& region, const std::string& ip);
    // bool HasOutdatedRealIpRegions() const;

    // mutable std::mutex mRegionCandidateEndpointsMapMux;
    // std::map<std::string, CandidateEndpoints> mRegionCandidateEndpointsMap;

    // mutable std::mutex mHttpsRegionsMux;
    // std::unordered_set<std::string> mHttpsRegions;

    mutable std::mutex mCandidateHostsInfosMapMux;
    // only custom mode is supported, and one project supports multiple custom endpoints
    std::map<std::string, std::list<std::weak_ptr<CandidateHostsInfo>>> mProjectCandidateHostsInfosMap;

    //     CURLM* mUnInitializedHostProbeClient = nullptr;
    //     mutable std::mutex mUnInitializedCandidateHostsInfosMux;
    //     std::vector<std::weak_ptr<CandidateHostsInfo>> mUnInitializedCandidateHostsInfos;

    //     std::future<void> mUnInitializedHostProbeThreadRes;
    //     mutable std::mutex mUnInitializedHostProbeThreadRunningMux;
    //     bool mIsUnInitializedHostProbeThreadRunning = true;

    //     CURLM* mHostProbeClient = nullptr;
    //     mutable std::mutex mPartiallyInitializedCandidateHostsInfosMux;
    //     std::vector<std::weak_ptr<CandidateHostsInfo>> mPartiallyInitializedCandidateHostsInfos;

    //     std::future<void> mHostProbeThreadRes;
    //     mutable std::mutex mHostProbeThreadRunningMux;
    //     bool mIsHostProbeThreadRunning = true;
    // #ifdef APSARA_UNIT_TEST_MAIN
    //     HttpResponse (*mDoProbeNetwork)(const std::unique_ptr<ProbeNetworkHttpRequest>& req) = nullptr;
    // #endif

    //     mutable std::mutex mRegionRealIpMux;
    //     std::map<std::string, std::string> mRegionRealIpMap;
    //     mutable std::mutex mOutdatedRealIpRegionsMux;
    //     std::vector<std::string> mOutdatedRealIpRegions;
    //     mutable std::mutex mRegionRealIpCandidateHostsInfosMapMux;
    //     std::map<std::string, CandidateHostsInfo> mRegionRealIpCandidateHostsInfosMap;
    //     bool (*mGetEndpointRealIp)(const std::string& endpoint, std::string& ip) = nullptr;

    //     std::future<void> mUpdateRealIpThreadRes;
    //     mutable std::mutex mUpdateRealIpThreadRunningMux;
    //     bool mIsUpdateRealIpThreadRunning = true;

    //     mutable std::condition_variable mStopCV;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FlusherSLSUnittest;
    friend class SLSClientManagerUnittest;
    // friend class ProbeNetworkMock;
    // friend class GetRealIpMock;
#endif
};

void PreparePostLogStoreLogsRequest(const std::string& accessKeyId,
                                    const std::string& accessKeySecret,
                                    SLSClientManager::AuthType type,
                                    const std::string& host,
                                    bool isHostIp,
                                    const std::string& project,
                                    const std::string& logstore,
                                    const std::string& compressType,
                                    RawDataType dataType,
                                    const std::string& body,
                                    size_t rawSize,
                                    const std::string& shardHashKey,
                                    std::optional<uint64_t> seqId,
                                    std::string& path,
                                    std::string& query,
                                    std::map<std::string, std::string>& header);
void PreparePostMetricStoreLogsRequest(const std::string& accessKeyId,
                                       const std::string& accessKeySecret,
                                       SLSClientManager::AuthType type,
                                       const std::string& host,
                                       bool isHostIp,
                                       const std::string& project,
                                       const std::string& logstore,
                                       const std::string& compressType,
                                       const std::string& body,
                                       size_t rawSize,
                                       std::string& path,
                                       std::map<std::string, std::string>& header);
SLSResponse PostLogStoreLogs(const std::string& accessKeyId,
                             const std::string& accessKeySecret,
                             SLSClientManager::AuthType type,
                             const std::string& host,
                             bool httpsFlag,
                             const std::string& project,
                             const std::string& logstore,
                             const std::string& compressType,
                             RawDataType dataType,
                             const std::string& body,
                             size_t rawSize,
                             const std::string& shardHashKey);
SLSResponse PostMetricStoreLogs(const std::string& accessKeyId,
                                const std::string& accessKeySecret,
                                SLSClientManager::AuthType type,
                                const std::string& host,
                                bool httpsFlag,
                                const std::string& project,
                                const std::string& logstore,
                                const std::string& compressType,
                                const std::string& body,
                                size_t rawSize);
SLSResponse PutWebTracking(const std::string& host,
                           bool httpsFlag,
                           const std::string& logstore,
                           const std::string& compressType,
                           const std::string& body,
                           size_t rawSize);
// bool GetEndpointRealIp(const std::string& endpoint, std::string& ip);

// #ifdef APSARA_UNIT_TEST_MAIN
// extern HttpResponse (*DoGetRealIp)(const std::unique_ptr<HttpRequest>& req);
// #endif

} // namespace logtail
