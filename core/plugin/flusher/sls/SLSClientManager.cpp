// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SLSClientManager.h"

#ifdef __linux__
#include <sys/utsname.h>
#endif

#include "app_config/AppConfig.h"
#include "common/EndpointUtil.h"
#ifdef __ENTERPRISE__
#include "common/EnterpriseEndpointUtil.h"
#endif
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/http/Constant.h"
#include "common/http/Curl.h"
#include "common/version.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#include "pipeline/queue/SenderQueueItem.h"
#ifdef __ENTERPRISE__
#include "plugin/flusher/sls/EnterpriseSLSClientManager.h"
#endif
#include "plugin/flusher/sls/PackIdManager.h"
#include "plugin/flusher/sls/SLSConstant.h"
#include "plugin/flusher/sls/SLSUtil.h"

DEFINE_FLAG_STRING(custom_user_agent, "custom user agent appended at the end of the exsiting ones", "");
// DEFINE_FLAG_INT32(sls_hosts_probe_max_try_cnt, "", 3);
// DEFINE_FLAG_INT32(sls_hosts_probe_timeout, "", 3);
// DEFINE_FLAG_INT32(sls_all_hosts_probe_interval_sec, "seconds", 5 * 60);
// DEFINE_FLAG_INT32(sls_hosts_probe_interval_sec, "", 60);
DEFINE_FLAG_STRING(default_access_key_id, "", "");
DEFINE_FLAG_STRING(default_access_key, "", "");
// DEFINE_FLAG_INT32(send_switch_real_ip_interval, "seconds", 60);
// DEFINE_FLAG_BOOL(send_prefer_real_ip, "use real ip to send data", false);

using namespace std;

namespace logtail {

// TODO: move endpoint functions to EnterpriseEndpointUtil.h
// bool IsCustomEndpoint(const string& endpoint) {
//     if (StartWith(endpoint, "log.") || StartWith(endpoint, "log-intranet.") || StartWith(endpoint, "log-internal.")
//         || EndWith(endpoint, ".aliyuncs.com") || EndWith(endpoint, ".aliyun-inc.com")) {
//         return false;
//     }
//     return true;
// }

// enum class EndpointAddressType { INNER, INTRANET, PUBLIC };
// const string kLogEndpointSuffix = ".log.aliyuncs.com";

// EndpointAddressType GetEndpointAddressType(const string& address) {
//     // 一国一云 OXS区访问 & VPC访问
//     if (StartWith(address, "log-intranet.") || StartWith(address, "log-internal.")) {
//         return EndpointAddressType::INTRANET;
//     }
//     // 一国一云 公网访问
//     if (StartWith(address, "log.")) {
//         return EndpointAddressType::PUBLIC;
//     }
//     if (EndWith(address, "-intranet" + kLogEndpointSuffix) || EndWith(address, "-internal" + kLogEndpointSuffix)) {
//         return EndpointAddressType::INTRANET;
//     }
//     if (!EndWith(address, "-share" + kLogEndpointSuffix) && EndWith(address, kLogEndpointSuffix)) {
//         return EndpointAddressType::PUBLIC;
//     }
//     return EndpointAddressType::INNER;
// }

// static const string kAccelerationDataEndpoint = "log-global.aliyuncs.com";

const string& EndpointModeToString(EndpointMode mode) {
    switch (mode) {
        case EndpointMode::CUSTOM:
            static string customStr = "custom";
            return customStr;
        case EndpointMode::ACCELERATE:
            static string accelerateStr = "accelerate";
            return accelerateStr;
        case EndpointMode::DEFAULT:
            static string defaultStr = "default";
            return defaultStr;
        default:
            static string unknownStr = "unknown";
            return unknownStr;
    }
}

chrono::milliseconds HostInfo::GetLatency() const {
    lock_guard<mutex> lock(mLatencyMux);
    return mLatency;
}

void HostInfo::SetLatency(const chrono::milliseconds& latency) {
    lock_guard<mutex> lock(mLatencyMux);
    mLatency = latency;
}

void HostInfo::SetForbidden() {
    lock_guard<mutex> lock(mLatencyMux);
    mLatency = chrono::milliseconds::max();
}

bool HostInfo::IsForbidden() const {
    lock_guard<mutex> lock(mLatencyMux);
    return mLatency == chrono::milliseconds::max();
}

void CandidateHostsInfo::UpdateHosts(const CandidateEndpoints& regionEndpoints) {
    lock_guard<mutex> lock(mCandidateHostsMux);
    switch (mMode) {
#ifdef __ENTERPRISE__
        case EndpointMode::DEFAULT: {
            vector<string> endpoints(regionEndpoints.mLocalEndpoints);
            for (const auto& endpoint : regionEndpoints.mRemoteEndpoints) {
                bool found = false;
                for (const auto& existedEndpoint : regionEndpoints.mLocalEndpoints) {
                    if (existedEndpoint == endpoint) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    endpoints.emplace_back(endpoint);
                }
            }
            vector<vector<HostInfo>> infos;
            for (const auto& endpoint : endpoints) {
                string host = mProject.empty() ? endpoint : mProject + "." + endpoint;
                if (mCandidateHosts.empty()) {
                    infos.emplace_back().emplace_back(host);
                } else {
                    bool found = false;
                    for (const auto& item : mCandidateHosts) {
                        if (!item.empty() && item[0].GetHostname() == host) {
                            found = true;
                            infos.emplace_back(item);
                            break;
                        }
                    }
                    if (!found) {
                        infos.emplace_back().emplace_back(host);
                    }
                }
            }
            mCandidateHosts.swap(infos);
            break;
        }
        case EndpointMode::ACCELERATE: {
            vector<string> endpoints{kAccelerationDataEndpoint};
            for (const auto& item : regionEndpoints.mRemoteEndpoints) {
                if (GetEndpointAddressType(item) == EndpointAddressType::PUBLIC) {
                    endpoints.emplace_back(item);
                }
            }
            vector<HostInfo> infos;
            for (const auto& endpoint : endpoints) {
                string host = mProject.empty() ? endpoint : mProject + "." + endpoint;
                if (mCandidateHosts.empty()) {
                    infos.emplace_back(host);
                } else {
                    bool found = false;
                    for (const auto& item : mCandidateHosts[0]) {
                        if (item.GetHostname() == host) {
                            found = true;
                            infos.emplace_back(item);
                            break;
                        }
                    }
                    if (!found) {
                        infos.emplace_back(host);
                    }
                }
            }
            if (mCandidateHosts.empty()) {
                mCandidateHosts.emplace_back(infos);
            } else {
                mCandidateHosts[0].swap(infos);
            }
            break;
        }
#endif
        case EndpointMode::CUSTOM: {
            vector<HostInfo> infos;
            for (const auto& endpoint : regionEndpoints.mLocalEndpoints) {
                string host = mProject.empty() ? endpoint : mProject + "." + endpoint;
                if (mCandidateHosts.empty()) {
                    infos.emplace_back(host);
                } else {
                    bool found = false;
                    for (const auto& item : mCandidateHosts[0]) {
                        if (item.GetHostname() == host) {
                            found = true;
                            infos.emplace_back(item);
                            break;
                        }
                    }
                    if (!found) {
                        infos.emplace_back(host);
                    }
                }
            }
            if (mCandidateHosts.empty()) {
                mCandidateHosts.emplace_back(infos);
            } else {
                mCandidateHosts[0].swap(infos);
            }
            break;
        }
        default:
            break;
    }
}

bool CandidateHostsInfo::UpdateHostInfo(const string& hostname, const chrono::milliseconds& latency) {
    lock_guard<mutex> lock(mCandidateHostsMux);
    for (auto& item : mCandidateHosts) {
        for (auto& entry : item) {
            if (entry.GetHostname() == hostname) {
                entry.SetLatency(latency);
                return true;
            }
        }
    }
    return false;
}

void CandidateHostsInfo::GetProbeHosts(vector<string>& hosts) const {
    if (!HasValidHost()) {
        return GetAllHosts(hosts);
    }
    {
        lock_guard<mutex> lock(mCandidateHostsMux);
        for (const auto& item : mCandidateHosts) {
            for (const auto& entry : item) {
                if (!entry.IsForbidden()) {
                    hosts.emplace_back(entry.GetHostname());
                }
            }
        }
    }
}

void CandidateHostsInfo::GetAllHosts(vector<string>& hosts) const {
    lock_guard<mutex> lock(mCandidateHostsMux);
    for (const auto& item : mCandidateHosts) {
        for (const auto& entry : item) {
            hosts.emplace_back(entry.GetHostname());
        }
    }
}

void CandidateHostsInfo::SelectBestHost() {
    lock_guard<mutex> lock(mCandidateHostsMux);
    for (size_t i = 0; i < mCandidateHosts.size(); ++i) {
        const auto& hosts = mCandidateHosts[i];
        chrono::milliseconds minLatency = chrono::milliseconds::max();
        size_t minIdx = numeric_limits<size_t>::max();
        for (size_t j = 0; j < hosts.size(); ++j) {
            if (!hosts[j].IsForbidden() && hosts[j].GetLatency() < minLatency) {
                minLatency = hosts[j].GetLatency();
                minIdx = j;
            }
        }
        if (minIdx != numeric_limits<size_t>::max()) {
            const auto& hostname = hosts[minIdx].GetHostname();
            if (GetCurrentHost() != hostname) {
                SetCurrentHost(hostname);
                LOG_INFO(sLogger,
                         ("switch to the best host", hostname)("latency", minLatency.count())("project", mProject)(
                             "region", mRegion)("endpoint mode", EndpointModeToString(mMode)));
            }
            return;
        }
    }
    SetCurrentHost("");
    LOG_INFO(sLogger,
             ("no valid host", "stop sending data and retry later")("project", mProject)("region", mRegion)(
                 "endpoint mode", EndpointModeToString(mMode)));
}

string CandidateHostsInfo::GetCurrentHost() const {
    lock_guard<mutex> lock(mCurrentHostMux);
    return mCurrentHost;
}

string CandidateHostsInfo::GetFirstHost() const {
    lock_guard<mutex> lock(mCandidateHostsMux);
    if (mCandidateHosts.empty() || mCandidateHosts[0].empty()) {
        return "";
    }
    return mCandidateHosts[0][0].GetHostname();
}

bool CandidateHostsInfo::HasValidHost() const {
    lock_guard<mutex> lock(mCurrentHostMux);
    return !mCurrentHost.empty();
}

void CandidateHostsInfo::SetCurrentHost(const string& host) {
    lock_guard<mutex> lock(mCurrentHostMux);
    mCurrentHost = host;
}

// SLSClientManager::ProbeNetworkHttpRequest::ProbeNetworkHttpRequest(
//     const string& region, const string& project, EndpointMode mode, const string& host, bool httpsFlag)
//     : AsynHttpRequest(HTTP_GET,
//                       httpsFlag,
//                       host,
//                       httpsFlag ? 443 : 80,
//                       HEALTH,
//                       "",
//                       {},
//                       "",
//                       HttpResponse(),
//                       INT32_FLAG(sls_hosts_probe_timeout),
//                       INT32_FLAG(sls_hosts_probe_max_try_cnt)),
//       mRegion(region),
//       mProject(project),
//       mMode(mode),
//       mHost(host) {
// }

// void SLSClientManager::ProbeNetworkHttpRequest::OnSendDone(HttpResponse& response) {
//     if (mProject.empty()) {
//         SLSClientManager::GetInstance()->UpdateHostInfo(mRegion, mHost, response.GetResponseTime());
//     } else {
//         SLSClientManager::GetInstance()->UpdateHostInfo(mProject, mMode, mHost, response.GetResponseTime());
//     }
// }

// SLSClientManager::SLSClientManager() : mGetEndpointRealIp(GetEndpointRealIp) {
// }

SLSClientManager* SLSClientManager::GetInstance() {
#ifdef __ENTERPRISE__
    return EnterpriseSLSClientManager::GetInstance();
#else
    static auto ptr = unique_ptr<SLSClientManager>(new SLSClientManager());
    return ptr.get();
#endif
}

void SLSClientManager::Init() {
    GenerateUserAgent();

    // mUnInitializedHostProbeClient = curl_multi_init();
    // if (mUnInitializedHostProbeClient == nullptr) {
    //     LOG_ERROR(sLogger, ("failed to init uninitialized host probe", "failed to init curl client"));
    // } else {
    //     mUnInitializedHostProbeThreadRes = async(launch::async, &SLSClientManager::UnInitializedHostProbeThread,
    //     this);
    // }

    // mHostProbeClient = curl_multi_init();
    // if (mHostProbeClient == nullptr) {
    //     LOG_ERROR(sLogger, ("failed to init host probe", "failed to init curl client"));
    // } else {
    //     mHostProbeThreadRes = async(launch::async, &SLSClientManager::HostProbeThread, this);
    // }

    // if (BOOL_FLAG(send_prefer_real_ip)) {
    //     mUpdateRealIpThreadRes = async(launch::async, &SLSClientManager::UpdateRealIpThread, this);
    // }
}

// void SLSClientManager::Stop() {
//     if (mUnInitializedHostProbeClient) {
//         lock_guard<mutex> lock(mUnInitializedHostProbeThreadRunningMux);
//         mIsUnInitializedHostProbeThreadRunning = false;
//     }
//     if (mHostProbeClient) {
//         lock_guard<mutex> lock(mHostProbeThreadRunningMux);
//         mIsHostProbeThreadRunning = false;
//     }
//     if (BOOL_FLAG(send_prefer_real_ip)) {
//         lock_guard<mutex> lock(mUpdateRealIpThreadRunningMux);
//         mIsUpdateRealIpThreadRunning = false;
//     }
//     mStopCV.notify_all();

//     if (mUnInitializedHostProbeClient) {
//         future_status s = mUnInitializedHostProbeThreadRes.wait_for(chrono::seconds(1));
//         if (s == future_status::ready) {
//             LOG_INFO(sLogger, ("sls uninitialized host probe", "stopped successfully"));
//         } else {
//             LOG_WARNING(sLogger, ("sls uninitialized host probe", "forced to stopped"));
//         }
//     }
//     if (mHostProbeClient) {
//         future_status s = mHostProbeThreadRes.wait_for(chrono::seconds(1));
//         if (s == future_status::ready) {
//             LOG_INFO(sLogger, ("sls host probe", "stopped successfully"));
//         } else {
//             LOG_WARNING(sLogger, ("sls host probe", "forced to stopped"));
//         }
//     }
//     if (BOOL_FLAG(send_prefer_real_ip) && mUpdateRealIpThreadRes.valid()) {
//         future_status s = mUpdateRealIpThreadRes.wait_for(chrono::seconds(1));
//         if (s == future_status::ready) {
//             LOG_INFO(sLogger, ("sls real ip update", "stopped successfully"));
//         } else {
//             LOG_WARNING(sLogger, ("sls real ip update", "forced to stopped"));
//         }
//     }
// }

bool SLSClientManager::GetAccessKey(const string& aliuid,
                                    AuthType& type,
                                    string& accessKeyId,
                                    string& accessKeySecret) {
    accessKeyId = STRING_FLAG(default_access_key_id);
    accessKeySecret = STRING_FLAG(default_access_key);
    type = AuthType::AK;
    return true;
}

// void SLSClientManager::UpdateLocalRegionEndpointsAndHttpsInfo(const string& region,
//                                                               const vector<string>& rawEndpoints) {
//     vector<string> endpoints;
//     for (const auto& item : rawEndpoints) {
//         auto tmp = ExtracteEndpoint(item);
//         if (!tmp.empty()) {
//             endpoints.emplace_back(tmp);
//         }
//     }

//     lock_guard<mutex> lock(mRegionCandidateEndpointsMapMux);
//     auto& candidate = mRegionCandidateEndpointsMap[region];
//     candidate.mMode = EndpointMode::DEFAULT;
//     candidate.mLocalEndpoints.clear();
//     for (const auto& item : endpoints) {
//         // if both acclerate and custom endpoints are given, we ignore custom endpoints
//         if (item == kAccelerationDataEndpoint) {
//             candidate.mMode = EndpointMode::ACCELERATE;
//             break;
//         }
//         if (IsCustomEndpoint(item)) {
//             candidate.mMode = EndpointMode::CUSTOM;
//             candidate.mLocalEndpoints.emplace_back(item);
//         }
//     }
//     if (candidate.mMode == EndpointMode::ACCELERATE) {
//         candidate.mLocalEndpoints.clear();
//     } else if (candidate.mMode == EndpointMode::DEFAULT) {
//         candidate.mLocalEndpoints = endpoints;
//     }

//     // as long as one endpoint is https, we treat the region as https
//     bool isHttps = false;
//     for (const auto& item : rawEndpoints) {
//         if (IsHttpsEndpoint(item)) {
//             isHttps = true;
//             break;
//         }
//     }
//     {
//         lock_guard<mutex> lock(mHttpsRegionsMux);
//         if (isHttps) {
//             mHttpsRegions.insert(region);
//         } else {
//             mHttpsRegions.erase(region);
//         }
//     }
// }

// void SLSClientManager::UpdateRemoteRegionEndpoints(const string& region,
//                                                    const vector<string>& rawEndpoints,
//                                                    RemoteEndpointUpdateAction action) {
//     vector<string> endpoints;
//     for (const auto& item : rawEndpoints) {
//         auto tmp = ExtractEndpoint(item);
//         if (!tmp.empty()) {
//             endpoints.emplace_back(tmp);
//         }
//     }
//     lock_guard<mutex> lock(mRegionCandidateEndpointsMapMux);

//     if (action == RemoteEndpointUpdateAction::CREATE
//         && mRegionCandidateEndpointsMap.find(region) != mRegionCandidateEndpointsMap.end()) {
//         return;
//     }
//     auto& candidate = mRegionCandidateEndpointsMap[region];
//     if (action == RemoteEndpointUpdateAction::APPEND) {
//         for (const auto& item : endpoints) {
//             bool found = false;
//             for (const auto& existed : candidate.mRemoteEndpoints) {
//                 if (existed == item) {
//                     found = true;
//                     break;
//                 }
//             }
//             if (!found) {
//                 candidate.mRemoteEndpoints.emplace_back(item);
//             }
//         }
//     } else {
//         candidate.mRemoteEndpoints = endpoints;
//     }
//     {
//         lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
//         for (auto& item : mRegionCandidateHostsInfosMap[region]) {
//             if (item.expired()) {
//                 continue;
//             }
//             auto info = item.lock();
//             info->UpdateHosts(candidate);
//         }
//     }
//     if (BOOL_FLAG(send_prefer_real_ip)) {
//         lock_guard<mutex> lock(mRegionRealIpCandidateHostsInfosMapMux);
//         auto it = mRegionRealIpCandidateHostsInfosMap.find(region);
//         if (it == mRegionRealIpCandidateHostsInfosMap.end()) {
//             it = mRegionRealIpCandidateHostsInfosMap.try_emplace(region, "", region, EndpointMode::DEFAULT).first;
//         }
//         it->second.UpdateHosts(candidate);
//     }
// }

// shared_ptr<CandidateHostsInfo>
// SLSClientManager::GetCandidateHostsInfo(const string& region, const string& project, EndpointMode mode) {
//     if (mode == EndpointMode::DEFAULT) {
//         // when flusher does not specify the mode, we use the mode of the region, which is set before flusher init
//         and
//         // will not change.
//         lock_guard<mutex> lock(mRegionCandidateEndpointsMapMux);
//         auto it = mRegionCandidateEndpointsMap.find(region);
//         if (it != mRegionCandidateEndpointsMap.end()) {
//             mode = it->second.mMode;
//         }
//     }
//     {
//         lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
//         auto& hostsInfo = mProjectCandidateHostsInfosMap[project];
//         for (auto& item : hostsInfo) {
//             if (item.expired()) {
//                 continue;
//             }
//             auto info = item.lock();
//             if (info->GetMode() == mode) {
//                 return info;
//             }
//         }
//     }

//     auto info = make_shared<CandidateHostsInfo>(project, region, mode);
//     CandidateEndpoints endpoints;
//     {
//         lock_guard<mutex> lock(mRegionCandidateEndpointsMapMux);
//         auto it = mRegionCandidateEndpointsMap.find(region);
//         if (it != mRegionCandidateEndpointsMap.end()) {
//             endpoints = it->second;
//         }
//     }
//     info->UpdateHosts(endpoints);
//     {
//         lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
//         mProjectCandidateHostsInfosMap[project].emplace_back(info);
//         mRegionCandidateHostsInfosMap[region].emplace_back(info);
//     }
//     {
//         lock_guard<mutex> lock(mUnInitializedCandidateHostsInfosMux);
//         mUnInitializedCandidateHostsInfos.emplace_back(info);
//     }
//     return info;
// }

// bool SLSClientManager::UpdateHostInfo(const string& project,
//                                       EndpointMode mode,
//                                       const string& host,
//                                       const chrono::milliseconds& latency) {
//     lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
//     auto it = mProjectCandidateHostsInfosMap.find(project);
//     if (it == mProjectCandidateHostsInfosMap.end()) {
//         return false;
//     }
//     for (auto& entry : it->second) {
//         if (entry.expired()) {
//             continue;
//         }
//         auto info = entry.lock();
//         if (info->GetMode() == mode) {
//             info->UpdateHostInfo(host, latency);
//             return true;
//         }
//     }
//     return false;
// }

// bool SLSClientManager::UpdateHostInfo(const string& region, const string& host, const chrono::milliseconds& latency)
// {
//     lock_guard<mutex> lock(mRegionRealIpCandidateHostsInfosMapMux);
//     auto it = mRegionRealIpCandidateHostsInfosMap.find(region);
//     if (it == mRegionRealIpCandidateHostsInfosMap.end()) {
//         return false;
//     }
//     return it->second.UpdateHostInfo(host, latency);
// }

shared_ptr<CandidateHostsInfo> SLSClientManager::GetCandidateHostsInfo(const string& project, const string& endpoint) {
    if (endpoint.empty()) {
        // this should only occur on first update, where we try find any available info
        lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
        auto& hostsInfo = mProjectCandidateHostsInfosMap[project];
        for (auto& item : hostsInfo) {
            if (item.expired()) {
                continue;
            }
            return item.lock();
        }
        return nullptr;
    }

    string standardEndpoint = ExtractEndpoint(endpoint);
    {
        lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
        auto& hostsInfo = mProjectCandidateHostsInfosMap[project];
        for (auto& item : hostsInfo) {
            if (item.expired()) {
                continue;
            }
            auto info = item.lock();
            if (info->GetFirstHost() == project + "." + standardEndpoint) {
                return info;
            }
        }
    }
    auto info = make_shared<CandidateHostsInfo>(project, "", EndpointMode::CUSTOM);
    info->UpdateHosts({EndpointMode::CUSTOM, {standardEndpoint}});
    // manually set the endpoint to be available
    info->UpdateHostInfo(info->GetFirstHost(), chrono::milliseconds(10));
    info->SelectBestHost();
    info->SetInitialized();
    {
        lock_guard<mutex> lock(mCandidateHostsInfosMapMux);
        mProjectCandidateHostsInfosMap[project].emplace_back(info);
    }
    return info;
}

bool SLSClientManager::UsingHttps(const string& region) const {
    return true;
    // lock_guard<mutex> lock(mHttpsRegionsMux);
    // return mHttpsRegions.find(region) != mHttpsRegions.end();
}

// void SLSClientManager::UnInitializedHostProbeThread() {
//     LOG_INFO(sLogger, ("sls uninitialized host probe", "started"));
//     unique_lock<mutex> lock(mUnInitializedHostProbeThreadRunningMux);
//     while (mIsUnInitializedHostProbeThreadRunning) {
//         DoProbeUnInitializedHost();
//         if (mStopCV.wait_for(
//                 lock, chrono::milliseconds(10), [this]() { return !mIsUnInitializedHostProbeThreadRunning; })) {
//             return;
//         }
//     }

//     auto mc = curl_multi_cleanup(mUnInitializedHostProbeClient);
//     if (mc != CURLM_OK) {
//         LOG_ERROR(sLogger, ("failed to cleanup curl multi handle", "exit anyway")("errMsg",
//         curl_multi_strerror(mc)));
//     }
// }

// void SLSClientManager::DoProbeUnInitializedHost() {
//     vector<weak_ptr<CandidateHostsInfo>> infos;
//     {
//         lock_guard<mutex> lock(mUnInitializedCandidateHostsInfosMux);
//         infos.swap(mUnInitializedCandidateHostsInfos);
//     }
//     if (infos.empty()) {
//         return;
//     }
// #ifndef APSARA_UNIT_TEST_MAIN
//     for (auto& item : infos) {
//         if (item.expired()) {
//             continue;
//         }
//         auto info = item.lock();
//         bool httpsFlag = UsingHttps(info->GetRegion());
//         auto req = make_unique<ProbeNetworkHttpRequest>(
//             info->GetRegion(), info->GetProject(), info->GetMode(), info->GetFirstHost(), httpsFlag);
//         AddRequestToMultiCurlHandler(mUnInitializedHostProbeClient, std::move(req));
//     }
//     SendAsynRequests(mUnInitializedHostProbeClient);
// #else
//     for (auto& item : infos) {
//         if (item.expired()) {
//             continue;
//         }
//         auto info = item.lock();
//         bool httpsFlag = UsingHttps(info->GetRegion());
//         auto req = make_unique<ProbeNetworkHttpRequest>(
//             info->GetRegion(), info->GetProject(), info->GetMode(), info->GetFirstHost(), httpsFlag);
//         HttpResponse response = mDoProbeNetwork(req);
//         req->OnSendDone(response);
//     }
// #endif

//     for (auto& item : infos) {
//         if (item.expired()) {
//             continue;
//         }
//         item.lock()->SelectBestHost();
//     }
//     {
//         lock_guard<mutex> lock(mPartiallyInitializedCandidateHostsInfosMux);
//         mPartiallyInitializedCandidateHostsInfos.insert(
//             mPartiallyInitializedCandidateHostsInfos.end(), infos.begin(), infos.end());
//     }
// }

// void SLSClientManager::HostProbeThread() {
//     LOG_INFO(sLogger, ("sls host probe", "started"));
//     unique_lock<mutex> lock(mHostProbeThreadRunningMux);
//     while (mIsHostProbeThreadRunning) {
//         DoProbeHost();
//         if (mStopCV.wait_for(lock, chrono::seconds(1), [this]() { return !mIsHostProbeThreadRunning; })) {
//             return;
//         }
//     }

//     auto mc = curl_multi_cleanup(mHostProbeClient);
//     if (mc != CURLM_OK) {
//         LOG_ERROR(sLogger, ("failed to cleanup curl multi handle", "exit anyway")("errMsg",
//         curl_multi_strerror(mc)));
//     }
// }

// void SLSClientManager::DoProbeHost() {
//     static time_t lastProbeAllEndpointsTime = time(nullptr);
//     static time_t lastProbeAvailableEndpointsTime = time(nullptr);
//     bool shouldTestAllEndpoints = false;
//     bool shouldTestAvailableEndpoints = false;
//     time_t beginTime = time(nullptr);
//     if (beginTime - lastProbeAllEndpointsTime >= INT32_FLAG(sls_all_hosts_probe_interval_sec)) {
//         shouldTestAllEndpoints = true;
//         lastProbeAllEndpointsTime = beginTime;
//     } else if (beginTime - lastProbeAvailableEndpointsTime >= INT32_FLAG(sls_hosts_probe_interval_sec)) {
//         shouldTestAvailableEndpoints = true;
//         lastProbeAvailableEndpointsTime = beginTime;
//     } else if (!HasPartiallyInitializedCandidateHostsInfos()) {
//         ClearExpiredCandidateHostsInfos();
//         PackIdManager::GetInstance()->CleanTimeoutEntry();
//         return;
//     }

//     // collect hosts to be probed
//     map<pair<string, EndpointMode>, pair<string, vector<string>>> projectHostMap;
//     map<string, vector<string>> regionHostsMap; // only for realip
//     if (shouldTestAllEndpoints || shouldTestAvailableEndpoints) {
//         {
//             lock_guard<mutex> lk(mCandidateHostsInfosMapMux);
//             for (const auto& item : mProjectCandidateHostsInfosMap) {
//                 for (const auto& entry : item.second) {
//                     if (entry.expired()) {
//                         continue;
//                     }
//                     auto info = entry.lock();
//                     auto& hosts = projectHostMap[make_pair(item.first, info->GetMode())];
//                     hosts.first = info->GetRegion();
//                     if (shouldTestAllEndpoints) {
//                         info->GetAllHosts(hosts.second);
//                     } else {
//                         info->GetProbeHosts(hosts.second);
//                     }
//                 }
//             }
//         }
//         if (BOOL_FLAG(send_prefer_real_ip)) {
//             lock_guard<mutex> lk(mRegionRealIpCandidateHostsInfosMapMux);
//             for (const auto& item : mRegionRealIpCandidateHostsInfosMap) {
//                 auto& hosts = regionHostsMap[item.second.GetRegion()];
//                 if (shouldTestAllEndpoints) {
//                     item.second.GetAllHosts(hosts);
//                 } else {
//                     item.second.GetProbeHosts(hosts);
//                 }
//             }
//         }
//     } else {
//         lock_guard<mutex> lk(mPartiallyInitializedCandidateHostsInfosMux);
//         for (const auto& item : mPartiallyInitializedCandidateHostsInfos) {
//             if (item.expired()) {
//                 continue;
//             }
//             auto info = item.lock();
//             auto& hosts = projectHostMap[make_pair(info->GetProject(), info->GetMode())];
//             hosts.first = info->GetRegion();
//             info->GetAllHosts(hosts.second);
//         }
//         mPartiallyInitializedCandidateHostsInfos.clear();
//     }

//     // do probe and update latency
//     for (const auto& item : projectHostMap) {
//         bool httpsFlag = UsingHttps(item.second.first);
// #ifndef APSARA_UNIT_TEST_MAIN
//         for (const auto& host : item.second.second) {
//             auto req = make_unique<ProbeNetworkHttpRequest>(
//                 item.second.first, item.first.first, item.first.second, host, httpsFlag);
//             AddRequestToMultiCurlHandler(mHostProbeClient, std::move(req));
//         }
//         SendAsynRequests(mHostProbeClient);
// #else
//         for (const auto& host : item.second.second) {
//             auto req = make_unique<ProbeNetworkHttpRequest>(
//                 item.second.first, item.first.first, item.first.second, host, httpsFlag);
//             HttpResponse response = mDoProbeNetwork(req);
//             req->OnSendDone(response);
//         }
// #endif
//     }
//     if (BOOL_FLAG(send_prefer_real_ip)) {
//         for (const auto& item : regionHostsMap) {
//             bool httpsFlag = UsingHttps(item.first);
// #ifndef APSARA_UNIT_TEST_MAIN
//             for (const auto& host : item.second) {
//                 auto req = make_unique<ProbeNetworkHttpRequest>(item.first, "", EndpointMode::DEFAULT, host,
//                 httpsFlag); AddRequestToMultiCurlHandler(mHostProbeClient, std::move(req));
//             }
//             SendAsynRequests(mHostProbeClient);
// #else
//             for (const auto& host : item.second) {
//                 auto req = make_unique<ProbeNetworkHttpRequest>(item.first, "", EndpointMode::DEFAULT, host,
//                 httpsFlag); HttpResponse response = mDoProbeNetwork(req); req->OnSendDone(response);
//             }
// #endif
//         }
//     }

//     // update selected host
//     {
//         lock_guard<mutex> lk(mCandidateHostsInfosMapMux);
//         for (const auto& item : mProjectCandidateHostsInfosMap) {
//             for (auto& entry : item.second) {
//                 if (auto p = entry.lock()) {
//                     p->SelectBestHost();
//                 }
//             }
//         }
//     }
//     if (BOOL_FLAG(send_prefer_real_ip) && (shouldTestAllEndpoints || shouldTestAvailableEndpoints)) {
//         lock_guard<mutex> lk(mRegionRealIpCandidateHostsInfosMapMux);
//         for (auto& item : mRegionRealIpCandidateHostsInfosMap) {
//             item.second.SelectBestHost();
//         }
//     }
// }

// bool SLSClientManager::HasPartiallyInitializedCandidateHostsInfos() const {
//     lock_guard<mutex> lk(mPartiallyInitializedCandidateHostsInfosMux);
//     return !mPartiallyInitializedCandidateHostsInfos.empty();
// }

// void SLSClientManager::ClearExpiredCandidateHostsInfos() {
//     lock_guard<mutex> lk(mCandidateHostsInfosMapMux);
//     for (auto iter = mProjectCandidateHostsInfosMap.begin(); iter != mProjectCandidateHostsInfosMap.end();) {
//         for (auto it = iter->second.begin(); it != iter->second.end();) {
//             if (it->expired()) {
//                 it = iter->second.erase(it);
//             } else {
//                 ++it;
//             }
//         }
//         if (iter->second.empty()) {
//             iter = mProjectCandidateHostsInfosMap.erase(iter);
//         } else {
//             ++iter;
//         }
//     }
//     for (auto iter = mRegionCandidateHostsInfosMap.begin(); iter != mRegionCandidateHostsInfosMap.end();) {
//         for (auto it = iter->second.begin(); it != iter->second.end();) {
//             if (it->expired()) {
//                 it = iter->second.erase(it);
//             } else {
//                 ++it;
//             }
//         }
//         if (iter->second.empty()) {
//             iter = mRegionCandidateHostsInfosMap.erase(iter);
//         } else {
//             ++iter;
//         }
//     }
// }

// void SLSClientManager::UpdateRealIpThread() {
//     LOG_INFO(sLogger, ("sls real ip update", "started"));
//     unique_lock<mutex> lock(mUpdateRealIpThreadRunningMux);
//     while (mIsUpdateRealIpThreadRunning) {
//         DoUpdateRealIp();
//         if (mStopCV.wait_for(lock, chrono::seconds(1), [this]() { return !mIsUpdateRealIpThreadRunning; })) {
//             break;
//         }
//     }
// }

// void SLSClientManager::DoUpdateRealIp() {
//     vector<pair<string, string>> hosts;
//     static time_t lastUpdateRealIpTime = 0;
//     if (time(nullptr) - lastUpdateRealIpTime >= INT32_FLAG(send_switch_real_ip_interval)) {
//         {
//             lock_guard<mutex> lock(mOutdatedRealIpRegionsMux);
//             mOutdatedRealIpRegions.clear();
//         }
//         {
//             lock_guard<mutex> lock(mRegionRealIpCandidateHostsInfosMapMux);
//             for (const auto& item : mRegionRealIpCandidateHostsInfosMap) {
//                 hosts.emplace_back(item.first, item.second.GetCurrentHost());
//             }
//         }
//         lastUpdateRealIpTime = time(nullptr);
//     } else if (HasOutdatedRealIpRegions()) {
//         vector<string> regions;
//         {
//             lock_guard<mutex> lock(mOutdatedRealIpRegionsMux);
//             regions.swap(mOutdatedRealIpRegions);
//         }
//         {
//             lock_guard<mutex> lock(mRegionRealIpCandidateHostsInfosMapMux);
//             for (const auto& item : regions) {
//                 auto it = mRegionRealIpCandidateHostsInfosMap.find(item);
//                 if (it != mRegionRealIpCandidateHostsInfosMap.end()) {
//                     hosts.emplace_back(item, it->second.GetCurrentHost());
//                 }
//             }
//         }
//     }
//     for (const auto& item : hosts) {
//         if (item.second.empty()) {
//             continue;
//         }
//         string ip;
//         if (!mGetEndpointRealIp(item.second, ip)) {
//             LOG_WARNING(sLogger,
//                         ("failed to get real ip", "retry later")("region", item.first)("endpoint", item.second));
//             continue;
//         }
//         SetRealIp(item.first, ip);
//     }
// }

// bool SLSClientManager::HasOutdatedRealIpRegions() const {
//     lock_guard<mutex> lock(mOutdatedRealIpRegionsMux);
//     return !mOutdatedRealIpRegions.empty();
// }

// void SLSClientManager::UpdateOutdatedRealIpRegions(const string& region) {
//     lock_guard<mutex> lock(mOutdatedRealIpRegionsMux);
//     mOutdatedRealIpRegions.emplace_back(region);
// }

// string SLSClientManager::GetRealIp(const string& region) const {
//     lock_guard<mutex> lock(mRegionRealIpMux);
//     auto it = mRegionRealIpMap.find(region);
//     if (it == mRegionRealIpMap.end()) {
//         return "";
//     }
//     return it->second;
// }

// void SLSClientManager::SetRealIp(const string& region, const string& ip) {
//     lock_guard<mutex> lock(mRegionRealIpMux);
//     auto it = mRegionRealIpMap.find(region);
//     if (it == mRegionRealIpMap.end()) {
//         mRegionRealIpMap.emplace(region, ip);
//     } else if (it->second != ip) {
//         LOG_INFO(sLogger, ("set real ip for region", region)("from", it->second)("to", ip));
//         it->second = ip;
//     }
// }

void SLSClientManager::GenerateUserAgent() {
    string os;
#if defined(__linux__)
    utsname* buf = new utsname;
    if (-1 == uname(buf)) {
        LOG_WARNING(
            sLogger,
            ("get os info part of user agent failed", errno)("use default os info", LoongCollectorMonitor::mOsDetail));
        os = LoongCollectorMonitor::mOsDetail;
    } else {
        char* pch = strchr(buf->release, '-');
        if (pch) {
            *pch = '\0';
        }
        os.append(buf->sysname);
        os.append("; ");
        os.append(buf->release);
        os.append("; ");
        os.append(buf->machine);
    }
    delete buf;
#elif defined(_MSC_VER)
    os = LoongCollectorMonitor::mOsDetail;
#endif

    mUserAgent = string("ilogtail/") + ILOGTAIL_VERSION + " (" + os + ") ip/" + LoongCollectorMonitor::mIpAddr + " env/"
        + GetRunningEnvironment();
    if (!STRING_FLAG(custom_user_agent).empty()) {
        mUserAgent += " " + STRING_FLAG(custom_user_agent);
    }
    LOG_INFO(sLogger, ("user agent", mUserAgent));
}

string SLSClientManager::GetRunningEnvironment() {
    string env;
    if (getenv("ALIYUN_LOG_STATIC_CONTAINER_INFO")) {
        env = "ECI";
    } else if (getenv("ACK_NODE_LOCAL_DNS_ADMISSION_CONTROLLER_SERVICE_HOST")) {
        // logtail-ds installed by ACK will possess the above env
        env = "ACK-Daemonset";
    } else if (getenv("KUBERNETES_SERVICE_HOST")) {
        // containers in K8S will possess the above env
        if (AppConfig::GetInstance()->IsPurageContainerMode()) {
            env = "K8S-Daemonset";
        } else if (PingEndpoint("100.100.100.200", "/latest/meta-data")) {
            // containers in ACK can be connected to the above address, see
            // https://help.aliyun.com/document_detail/108460.html#section-akf-lwh-1gb.
            // Note: we can not distinguish ACK from K8S built on ECS
            env = "ACK-Sidecar";
        } else {
            env = "K8S-Sidecar";
        }
    } else if (AppConfig::GetInstance()->IsPurageContainerMode() || getenv("ALIYUN_LOGTAIL_CONFIG")) {
        env = "Docker";
    } else if (PingEndpoint("100.100.100.200", "/latest/meta-data")) {
        env = "ECS";
    } else {
        env = "Others";
    }
    return env;
}

bool SLSClientManager::PingEndpoint(const string& host, const string& path) {
    map<string, string> header;
    HttpResponse response;
    return SendHttpRequest(make_unique<HttpRequest>(HTTP_GET, false, host, 80, path, "", header, "", 3, 1, true),
                           response);
}

#ifdef APSARA_UNIT_TEST_MAIN
void SLSClientManager::Clear() {
    mProjectCandidateHostsInfosMap.clear();
}
#endif

void PreparePostLogStoreLogsRequest(const string& accessKeyId,
                                    const string& accessKeySecret,
                                    SLSClientManager::AuthType type,
                                    const string& host,
                                    bool isHostIp,
                                    const string& project,
                                    const string& logstore,
                                    const string& compressType,
                                    RawDataType dataType,
                                    const string& body,
                                    size_t rawSize,
                                    const string& shardHashKey,
                                    optional<uint64_t> seqId,
                                    string& path,
                                    string& query,
                                    map<string, string>& header) {
    path = LOGSTORES;
    path.append("/").append(logstore);
    if (shardHashKey.empty()) {
        path.append("/shards/lb");
    } else {
        path.append("/shards/route");
    }

    if (isHostIp) {
        header[HOST] = project + "." + host;
    } else {
        header[HOST] = host;
    }
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
    header[CONTENT_LENGTH] = to_string(body.size());
    header[CONTENT_MD5] = CalcMD5(body);
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    if (dataType == RawDataType::EVENT_GROUP) {
        header[X_LOG_BODYRAWSIZE] = to_string(rawSize);
    } else {
        header[X_LOG_BODYRAWSIZE] = to_string(body.size());
        header[X_LOG_MODE] = LOG_MODE_BATCH_GROUP;
    }
    if (type == SLSClientManager::AuthType::ANONYMOUS) {
        header[X_LOG_KEYPROVIDER] = MD5_SHA1_SALT_KEYPROVIDER;
    }

    map<string, string> parameterList;
    if (!shardHashKey.empty()) {
        parameterList["key"] = shardHashKey;
        if (!seqId.has_value()) {
            parameterList["seqid"] = to_string(seqId.value());
        }
    }
    query = GetQueryString(parameterList);

    string signature = GetUrlSignature(HTTP_POST, path, header, parameterList, body, accessKeySecret);
    header[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + accessKeyId + ':' + signature;
}

void PreparePostMetricStoreLogsRequest(const string& accessKeyId,
                                       const string& accessKeySecret,
                                       SLSClientManager::AuthType type,
                                       const string& host,
                                       bool isHostIp,
                                       const string& project,
                                       const string& logstore,
                                       const string& compressType,
                                       const string& body,
                                       size_t rawSize,
                                       string& path,
                                       map<string, string>& header) {
    path = METRICSTORES;
    path.append("/").append(project).append("/").append(logstore).append("/api/v1/write");

    if (isHostIp) {
        header[HOST] = project + "." + host;
    } else {
        header[HOST] = host;
    }
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
    header[CONTENT_LENGTH] = to_string(body.size());
    header[CONTENT_MD5] = CalcMD5(body);
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    header[X_LOG_BODYRAWSIZE] = to_string(rawSize);
    if (type == SLSClientManager::AuthType::ANONYMOUS) {
        header[X_LOG_KEYPROVIDER] = MD5_SHA1_SALT_KEYPROVIDER;
    }

    map<string, string> parameterList;
    string signature = GetUrlSignature(HTTP_POST, path, header, parameterList, body, accessKeySecret);
    header[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + accessKeyId + ':' + signature;
}

SLSResponse PostLogStoreLogs(const string& accessKeyId,
                             const string& accessKeySecret,
                             SLSClientManager::AuthType type,
                             const string& host,
                             bool httpsFlag,
                             const string& project,
                             const string& logstore,
                             const string& compressType,
                             RawDataType dataType,
                             const string& body,
                             size_t rawSize,
                             const string& shardHashKey) {
    string path, query;
    map<string, string> header;
    PreparePostLogStoreLogsRequest(accessKeyId,
                                   accessKeySecret,
                                   type,
                                   host,
                                   false, // sync request always uses vip
                                   project,
                                   logstore,
                                   compressType,
                                   dataType,
                                   body,
                                   rawSize,
                                   shardHashKey,
                                   nullopt, // sync request does not support exactly-once
                                   path,
                                   query,
                                   header);
    HttpResponse response;
    SendHttpRequest(
        make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, query, header, body),
        response);
    return ParseHttpResponse(response);
}

SLSResponse PostMetricStoreLogs(const string& accessKeyId,
                                const string& accessKeySecret,
                                SLSClientManager::AuthType type,
                                const string& host,
                                bool httpsFlag,
                                const string& project,
                                const string& logstore,
                                const string& compressType,
                                const string& body,
                                size_t rawSize) {
    string path;
    map<string, string> header;
    PreparePostMetricStoreLogsRequest(accessKeyId,
                                      accessKeySecret,
                                      type,
                                      host,
                                      false, // sync request always uses vip
                                      project,
                                      logstore,
                                      compressType,
                                      body,
                                      rawSize,
                                      path,
                                      header);
    HttpResponse response;
    SendHttpRequest(make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, "", header, body),
                    response);
    return ParseHttpResponse(response);
}

SLSResponse PutWebTracking(const string& host,
                           bool httpsFlag,
                           const string& logstore,
                           const string& compressType,
                           const string& body,
                           size_t rawSize) {
    string path = LOGSTORES;
    path.append("/").append(logstore).append("/track");

    map<string, string> header;
    header[HOST] = host;
    header[USER_AGENT] = SLSClientManager::GetInstance()->GetUserAgent();
    header[DATE] = GetDateString();
    header[CONTENT_LENGTH] = to_string(body.size());
    header[X_LOG_APIVERSION] = LOG_API_VERSION;
    // header[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
    if (!compressType.empty()) {
        header[X_LOG_COMPRESSTYPE] = compressType;
    }
    header[X_LOG_BODYRAWSIZE] = to_string(rawSize);

    HttpResponse response;
    SendHttpRequest(make_unique<HttpRequest>(HTTP_POST, httpsFlag, host, httpsFlag ? 443 : 80, path, "", header, body),
                    response);
    return ParseHttpResponse(response);
}

// bool GetEndpointRealIp(const string& endpoint, string& ip) {
//     static string body;
//     if (body.empty()) {
//         sls_logs::LogGroup logGroup;
//         logGroup.set_source(LoongCollectorMonitor::mIpAddr);
//         body = logGroup.SerializeAsString();
//     }

//     static string project = "logtail-real-ip-project";
//     static string logstore = "logtail-real-ip-logstore";

//     SLSClientManager::AuthType type;
//     string accessKeyId, accessKeySecret;
//     SLSClientManager::GetInstance()->GetAccessKey("", type, accessKeyId, accessKeySecret);

//     string path, query;
//     map<string, string> header;
//     string host = project + "." + endpoint;
//     PreparePostLogStoreLogsRequest(accessKeyId,
//                                    accessKeySecret,
//                                    type,
//                                    host,
//                                    false,
//                                    project,
//                                    logstore,
//                                    "",
//                                    RawDataType::EVENT_GROUP,
//                                    body,
//                                    body.size(),
//                                    "",
//                                    nullopt,
//                                    path,
//                                    query,
//                                    header);
//     HttpResponse response;
//     if (!SendHttpRequest(make_unique<HttpRequest>(HTTP_POST, false, host, 80, path, query, header, body), response))
//     {
//         return false;
//     }
//     if (response.GetStatusCode() != 200) {
//         auto it = response.GetHeader().find(X_LOG_HOSTIP);
//         if (it != response.GetHeader().end()) {
//             ip = it->second;
//         }
//         return true;
//     }
//     // should not happen
//     return true;
// }

} // namespace logtail
