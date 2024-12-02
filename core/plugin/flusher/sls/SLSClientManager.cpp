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

#include <curl/curl.h>

#include "app_config/AppConfig.h"
#include "common/EndpointUtil.h"
#include "common/Flags.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/version.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"
#ifdef __ENTERPRISE__
#include "plugin/flusher/sls/EnterpriseSLSClientManager.h"
#endif
#include "plugin/flusher/sls/FlusherSLS.h"
#include "plugin/flusher/sls/SendResult.h"
#include "sdk/Exception.h"

// for windows compatability, to avoid conflict with the same function defined in windows.h
#ifdef SetPort
#undef SetPort
#endif

DEFINE_FLAG_STRING(data_endpoint_policy,
                   "policy for switching between data server endpoints, possible options include "
                   "'designated_first'(default) and 'designated_locked'",
                   "designated_first");
DEFINE_FLAG_INT32(sls_host_update_interval, "seconds", 5);
DEFINE_FLAG_INT32(send_client_timeout_interval, "recycle clients avoid memory increment", 12 * 3600);
DEFINE_FLAG_INT32(test_network_normal_interval, "if last check is normal, test network again after seconds ", 30);
DEFINE_FLAG_INT32(test_unavailable_endpoint_interval, "test unavailable endpoint interval", 60);
DEFINE_FLAG_INT32(send_switch_real_ip_interval, "seconds", 60);
DEFINE_FLAG_BOOL(send_prefer_real_ip, "use real ip to send data", false);
DEFINE_FLAG_STRING(default_access_key_id, "", "");
DEFINE_FLAG_STRING(default_access_key, "", "");
DEFINE_FLAG_STRING(custom_user_agent, "custom user agent appended at the end of the exsiting ones", "");

using namespace std;

namespace logtail {

bool SLSClientManager::RegionEndpointsInfo::AddDefaultEndpoint(const std::string& endpoint,
                                                               const EndpointSourceType& endpointType,
                                                               bool& isDefault) {
    if (mDefaultEndpoint.empty()
        || (endpointType == EndpointSourceType::LOCAL && mDefaultEndpointType == EndpointSourceType::REMOTE)) {
        mDefaultEndpoint = endpoint;
        mDefaultEndpointType = endpointType;
        isDefault = true;
    }
    return AddEndpoint(endpoint, true, false);
}

bool SLSClientManager::RegionEndpointsInfo::AddEndpoint(const std::string& endpoint, bool status, bool proxy) {
    if (mEndpointInfoMap.find(endpoint) == mEndpointInfoMap.end()) {
        mEndpointInfoMap.emplace(std::make_pair(endpoint, EndpointInfo(status, proxy)));
        return true;
    }
    return false;
}

void SLSClientManager::RegionEndpointsInfo::UpdateEndpointInfo(const std::string& endpoint,
                                                               bool status,
                                                               std::optional<uint32_t> latency,
                                                               bool createFlag) {
    auto iter = mEndpointInfoMap.find(endpoint);
    if (iter == mEndpointInfoMap.end()) {
        if (createFlag) {
            AddEndpoint(endpoint, status);
        }
    } else {
        (iter->second).UpdateInfo(status, latency);
    }
}

void SLSClientManager::RegionEndpointsInfo::RemoveEndpoint(const std::string& endpoint) {
    mEndpointInfoMap.erase(endpoint);
    if (mDefaultEndpoint == endpoint) {
        mDefaultEndpoint.clear();
    }
}

std::string SLSClientManager::RegionEndpointsInfo::GetAvailableEndpointWithTopPriority() const {
    if (!mDefaultEndpoint.empty()) {
        auto iter = mEndpointInfoMap.find(mDefaultEndpoint);
        if (iter != mEndpointInfoMap.end() && (iter->second).mValid) {
            return mDefaultEndpoint;
        }
    }
    std::string proxyEndpoint;
    for (auto iter = mEndpointInfoMap.begin(); iter != mEndpointInfoMap.end(); ++iter) {
        if (!(iter->second).mValid) {
            continue;
        }
        if ((iter->second).mValid && !(iter->second).mProxy) {
            return iter->first;
        }
        proxyEndpoint = iter->first;
    }
    if (!proxyEndpoint.empty()) {
        return proxyEndpoint;
    }
    if (!mDefaultEndpoint.empty()) {
        return mDefaultEndpoint;
    }
    if (!mEndpointInfoMap.empty()) {
        return mEndpointInfoMap.begin()->first;
    }
    return mDefaultEndpoint;
}

SLSClientManager* SLSClientManager::GetInstance() {
#ifdef __ENTERPRISE__
    static auto ptr = unique_ptr<SLSClientManager>(new EnterpriseSLSClientManager());
#else
    static auto ptr = unique_ptr<SLSClientManager>(new SLSClientManager());
#endif
    return ptr.get();
}

void SLSClientManager::Init() {
    InitEndpointSwitchPolicy();
    GenerateUserAgent();
    if (mDataServerSwitchPolicy == EndpointSwitchPolicy::DESIGNATED_FIRST) {
        mProbeNetworkClient.reset(new sdk::Client("",
                                                  "",
                                                  INT32_FLAG(sls_client_send_timeout)));
        mProbeNetworkClient->SetPort(AppConfig::GetInstance()->GetDataServerPort());
        mProbeNetworkThreadRes = async(launch::async, &SLSClientManager::ProbeNetworkThread, this);
    }
    if (BOOL_FLAG(send_prefer_real_ip)) {
        mUpdateRealIpClient.reset(new sdk::Client("",
                                                  "",
                                                  INT32_FLAG(sls_client_send_timeout)));
        mUpdateRealIpClient->SetPort(AppConfig::GetInstance()->GetDataServerPort());
        mUpdateRealIpThreadRes = async(launch::async, &SLSClientManager::UpdateRealIpThread, this);
    }
}

void SLSClientManager::Stop() {
    if (mDataServerSwitchPolicy == EndpointSwitchPolicy::DESIGNATED_FIRST) {
        lock_guard<mutex> lock(mProbeNetworkThreadRunningMux);
        mIsProbeNetworkThreadRunning = false;
    }
    if (BOOL_FLAG(send_prefer_real_ip)) {
        lock_guard<mutex> lock(mUpdateRealIpThreadRunningMux);
        mIsUpdateRealIpThreadRunning = false;
    }
    mStopCV.notify_all();
    if (mDataServerSwitchPolicy == EndpointSwitchPolicy::DESIGNATED_FIRST && mProbeNetworkThreadRes.valid()) {
        future_status s = mProbeNetworkThreadRes.wait_for(chrono::seconds(1));
        if (s == future_status::ready) {
            LOG_INFO(sLogger, ("sls endpoint probe", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("sls endpoint probe", "forced to stopped"));
        }
    }
    if (BOOL_FLAG(send_prefer_real_ip) && mUpdateRealIpThreadRes.valid()) {
        future_status s = mUpdateRealIpThreadRes.wait_for(chrono::seconds(1));
        if (s == future_status::ready) {
            LOG_INFO(sLogger, ("sls real ip update", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("sls real ip update", "forced to stopped"));
        }
    }
}

void SLSClientManager::InitEndpointSwitchPolicy() {
    if (STRING_FLAG(data_endpoint_policy) == "designated_locked") {
        mDataServerSwitchPolicy = EndpointSwitchPolicy::DESIGNATED_LOCKED;
    } else if (STRING_FLAG(data_endpoint_policy) == "designated_first") {
        mDataServerSwitchPolicy = EndpointSwitchPolicy::DESIGNATED_FIRST;
    } else {
        LOG_WARNING(sLogger,
                    ("param data_endpoint_policy is invalid, action", "use default value instead")("default value",
                                                                                                   "designated_first"));
    }
}

vector<string> SLSClientManager::GetRegionAliuids(const std::string& region) {
    lock_guard<mutex> lock(mRegionAliuidRefCntMapLock);
    vector<string> aliuids;
    for (const auto& item : mRegionAliuidRefCntMap[region]) {
        aliuids.push_back(item.first);
    }
    return aliuids;
}

void SLSClientManager::IncreaseAliuidReferenceCntForRegion(const std::string& region, const std::string& aliuid) {
    lock_guard<mutex> lock(mRegionAliuidRefCntMapLock);
    ++mRegionAliuidRefCntMap[region][aliuid];
}

void SLSClientManager::DecreaseAliuidReferenceCntForRegion(const std::string& region, const std::string& aliuid) {
    lock_guard<mutex> lock(mRegionAliuidRefCntMapLock);
    auto outerIter = mRegionAliuidRefCntMap.find(region);
    if (outerIter == mRegionAliuidRefCntMap.end()) {
        // should not happen
        return;
    }
    auto innerIter = outerIter->second.find(aliuid);
    if (innerIter == outerIter->second.end()) {
        // should not happen
        return;
    }
    if (--innerIter->second == 0) {
        outerIter->second.erase(innerIter);
    }
    if (outerIter->second.empty()) {
        mRegionAliuidRefCntMap.erase(outerIter);
    }
}

sdk::Client* SLSClientManager::GetClient(const string& region, const string& aliuid, bool createIfNotFound) {
    string key = region + "_" + aliuid;
    {
        lock_guard<mutex> lock(mClientMapMux);
        auto iter = mClientMap.find(key);
        if (iter != mClientMap.end()) {
            (iter->second).second = time(NULL);
            return (iter->second).first.get();
        }
    }
    if (!createIfNotFound) {
        return nullptr;
    }

    string endpoint = GetAvailableEndpointWithTopPriority(region);
    auto client = make_unique<sdk::Client>(aliuid,
                                           endpoint,
                                           INT32_FLAG(sls_client_send_timeout));
    ResetClientPort(region, client.get());
    LOG_INFO(sLogger,
             ("init endpoint for sender, region", region)("uid", aliuid)("hostname", GetHostFromEndpoint(endpoint))(
                 "use https", ToString(client->IsUsingHTTPS())));
    auto ptr = client.get();
    {
        lock_guard<mutex> lock(mClientMapMux);
        mClientMap.insert(make_pair(key, make_pair(std::move(client), time(nullptr))));
    }
    return ptr;
}

bool SLSClientManager::ResetClientEndpoint(const string& aliuid, const string& region, time_t curTime) {
    sdk::Client* sendClient = GetClient(region, aliuid, false);
    if (sendClient == nullptr) {
        return false;
    }
    if (curTime - sendClient->GetSlsHostUpdateTime() < INT32_FLAG(sls_host_update_interval)) {
        return false;
    }
    sendClient->SetSlsHostUpdateTime(curTime);
    string endpoint = GetAvailableEndpointWithTopPriority(region);
    if (endpoint.empty()) {
        return false;
    }
    string originalEndpoint = sendClient->GetRawSlsHost();
    if (originalEndpoint == endpoint) {
        return false;
    }
    sendClient->SetSlsHost(endpoint);
    ResetClientPort(region, sendClient);
    LOG_INFO(
        sLogger,
        ("reset endpoint for sender, region", region)("uid", aliuid)("from", GetHostFromEndpoint(originalEndpoint))(
            "to", GetHostFromEndpoint(endpoint))("use https", ToString(sendClient->IsUsingHTTPS())));
    return true;
}

void SLSClientManager::ResetClientPort(const string& region, sdk::Client* sendClient) {
    sendClient->SetPort(AppConfig::GetInstance()->GetDataServerPort());
    if (AppConfig::GetInstance()->GetDataServerPort() == 80) {
        lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
        auto iter = mRegionEndpointEntryMap.find(region);
        if (iter != mRegionEndpointEntryMap.end()) {
            const string& defaultEndpoint = iter->second.mDefaultEndpoint;
            if (!defaultEndpoint.empty()) {
                if (IsHttpsEndpoint(defaultEndpoint)) {
                    sendClient->SetPort(443);
                }
            } else {
                if (IsHttpsEndpoint(sendClient->GetRawSlsHost())) {
                    sendClient->SetPort(443);
                }
            }
        }
    }
}

void SLSClientManager::CleanTimeoutClient() {
    lock_guard<mutex> lock(mClientMapMux);
    time_t curTime = time(nullptr);
    for (auto iter = mClientMap.begin(); iter != mClientMap.end();) {
        if ((curTime - (iter->second).second) > INT32_FLAG(send_client_timeout_interval)) {
            iter = mClientMap.erase(iter);
        } else {
            ++iter;
        }
    }
}

bool SLSClientManager::GetAccessKey(const std::string& aliuid,
                                    AuthType& type,
                                    std::string& accessKeyId,
                                    std::string& accessKeySecret) {
    accessKeyId = STRING_FLAG(default_access_key_id);
    accessKeySecret = STRING_FLAG(default_access_key);
    type = AuthType::AK;
    return true;
}

void SLSClientManager::AddEndpointEntry(const string& region,
                                        const string& endpoint,
                                        bool isProxy,
                                        const EndpointSourceType& endpointType) {
    lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
    RegionEndpointsInfo& info = mRegionEndpointEntryMap[region];
    if (!isProxy) {
        bool isDefault = false;
        if (info.AddDefaultEndpoint(endpoint, endpointType, isDefault)) {
            LOG_INFO(sLogger,
                     ("add data server endpoint, region", region)("endpoint", endpoint)(
                         "isDefault", isDefault ? "yes" : "no")("isProxy", "false")("#endpoint",
                                                                                    info.mEndpointInfoMap.size()));
        }
    } else {
        if (info.AddEndpoint(endpoint, true, isProxy)) {
            LOG_INFO(sLogger,
                     ("add data server endpoint, region", region)("endpoint", endpoint)("isProxy", ToString(isProxy))(
                         "#endpoint", info.mEndpointInfoMap.size()));
        }
    }
}

void SLSClientManager::UpdateEndpointStatus(const string& region,
                                            const string& endpoint,
                                            bool status,
                                            optional<uint32_t> latency) {
    lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
    auto iter = mRegionEndpointEntryMap.find(region);
    if (iter != mRegionEndpointEntryMap.end()) {
        (iter->second).UpdateEndpointInfo(endpoint, status, latency, false);
    }
}

string SLSClientManager::GetAvailableEndpointWithTopPriority(const string& region) const {
    static string emptyStr = "";
    lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
    auto iter = mRegionEndpointEntryMap.find(region);
    if (iter != mRegionEndpointEntryMap.end()) {
        return (iter->second).GetAvailableEndpointWithTopPriority();
    }
    return emptyStr;
}

string SLSClientManager::GetRegionFromEndpoint(const string& endpoint) {
    lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
    for (auto iter = mRegionEndpointEntryMap.begin(); iter != mRegionEndpointEntryMap.end(); ++iter) {
        for (auto epIter = ((iter->second).mEndpointInfoMap).begin(); epIter != ((iter->second).mEndpointInfoMap).end();
             ++epIter) {
            if (epIter->first == endpoint)
                return iter->first;
        }
    }
    return STRING_FLAG(default_region_name);
}

bool SLSClientManager::HasNetworkAvailable() {
    static time_t lastCheckTime = time(nullptr);
    time_t curTime = time(nullptr);
    if (curTime - lastCheckTime >= 3600) {
        lastCheckTime = curTime;
        return true;
    }
    {
        lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
        for (auto iter = mRegionEndpointEntryMap.begin(); iter != mRegionEndpointEntryMap.end(); ++iter) {
            for (auto epIter = ((iter->second).mEndpointInfoMap).begin();
                 epIter != ((iter->second).mEndpointInfoMap).end();
                 ++epIter) {
                if ((epIter->second).mValid) {
                    return true;
                }
            }
        }
    }
    return false;
}

void SLSClientManager::ProbeNetworkThread() {
    LOG_INFO(sLogger, ("sls endpoint probe", "started"));
    // pair<int32_t, string> represents the weight of each endpoint
    map<string, vector<pair<int32_t, string>>> unavaliableEndpoints;
    set<string> unavaliableRegions;
    int32_t lastCheckAllTime = 0;
    unique_lock<mutex> lock(mProbeNetworkThreadRunningMux);
    while (mIsProbeNetworkThreadRunning) {
        unavaliableEndpoints.clear();
        unavaliableRegions.clear();
        {
            lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
            for (auto iter = mRegionEndpointEntryMap.begin(); iter != mRegionEndpointEntryMap.end(); ++iter) {
                auto& endpoints = unavaliableEndpoints[iter->first];
                bool unavaliable = true;
                for (auto epIter = ((iter->second).mEndpointInfoMap).begin();
                     epIter != ((iter->second).mEndpointInfoMap).end();
                     ++epIter) {
                    if (!(epIter->second).mValid) {
                        if (epIter->first == iter->second.mDefaultEndpoint) {
                            endpoints.emplace_back(0, epIter->first);
                        } else {
                            endpoints.emplace_back(10, epIter->first);
                        }
                    } else {
                        unavaliable = false;
                    }
                }
                sort(endpoints.begin(), endpoints.end());
                if (unavaliable) {
                    unavaliableRegions.insert(iter->first);
                }
            }
        }
        if (unavaliableEndpoints.empty()) {
            if (mStopCV.wait_for(lock, chrono::seconds(INT32_FLAG(test_network_normal_interval)), [this]() {
                    return !mIsProbeNetworkThreadRunning;
                })) {
                break;
            }
            continue;
        }
        int32_t curTime = time(NULL);
        // bool wakeUp = false;
        for (const auto& value : unavaliableEndpoints) {
            const string& region = value.first;
            vector<string> uids = GetRegionAliuids(region);
            bool endpointChanged = false;
            for (const auto& item : value.second) {
                const string& endpoint = item.second;
                const int32_t priority = item.first;
                if (unavaliableRegions.find(region) == unavaliableRegions.end()) {
                    if (!endpointChanged && priority != 10) {
                        if (TestEndpoint(region, endpoint)) {
                            for (const auto& uid : uids) {
                                ResetClientEndpoint(uid, region, curTime);
                            }
                            endpointChanged = true;
                        }
                    } else {
                        if (curTime - lastCheckAllTime >= 1800) {
                            TestEndpoint(region, endpoint);
                        }
                    }
                } else {
                    if (TestEndpoint(region, endpoint)) {
                        // wakeUp = true;
                        // Sender::GetInstance()->OnRegionRecover(region);
                        if (!endpointChanged) {
                            for (const auto& uid : uids) {
                                ResetClientEndpoint(uid, region, curTime);
                            }
                            endpointChanged = true;
                        }
                    }
                }
            }
        }
        // if (wakeUp && (!mIsSendingBuffer)) {
        //     mSenderQueue.Signal();
        // }
        if (curTime - lastCheckAllTime >= 1800) {
            lastCheckAllTime = curTime;
        }
        if (mStopCV.wait_for(lock, chrono::seconds(INT32_FLAG(test_unavailable_endpoint_interval)), [this]() {
                return !mIsProbeNetworkThreadRunning;
            })) {
            break;
        }
    }
}

bool SLSClientManager::TestEndpoint(const string& region, const string& endpoint) {
    // TODO: this should be removed, since control-plane status is not the same as data-plane
    if (!FlusherSLS::GetRegionStatus(region)) {
        return false;
    }
    if (endpoint.empty()) {
        return false;
    }
    mProbeNetworkClient->SetSlsHost(endpoint);
    ResetClientPort(region, mProbeNetworkClient.get());

    bool status = true;
    uint64_t beginTime = GetCurrentTimeInMicroSeconds();
    try {
        status = mProbeNetworkClient->TestNetwork();
    } catch (sdk::LOGException& ex) {
        if (ConvertErrorCode(ex.GetErrorCode()) == SEND_NETWORK_ERROR) {
            status = false;
        }
    } catch (...) {
        LOG_ERROR(sLogger, ("test network", "send fail")("exception", "unknown"));
        status = false;
    }
    uint32_t latency = (GetCurrentTimeInMicroSeconds() - beginTime) / 1000;
    LOG_DEBUG(sLogger, ("TestEndpoint, region", region)("endpoint", endpoint)("status", status)("latency", latency));
    UpdateEndpointStatus(region, endpoint, status, latency);
    return status;
}

void SLSClientManager::ForceUpdateRealIp(const string& region) {
    lock_guard<mutex> lock(mRegionRealIpLock);
    auto iter = mRegionRealIpMap.find(region);
    if (iter != mRegionRealIpMap.end()) {
        iter->second->mForceFlushFlag = true;
    }
}

void SLSClientManager::UpdateSendClientRealIp(sdk::Client* client, const string& region) {
    string realIp;
    RealIpInfo* pInfo = NULL;
    {
        lock_guard<mutex> lock(mRegionRealIpLock);
        auto iter = mRegionRealIpMap.find(region);
        if (iter != mRegionRealIpMap.end()) {
            pInfo = iter->second;
        } else {
            pInfo = new RealIpInfo;
            mRegionRealIpMap.insert(make_pair(region, pInfo));
        }
        realIp = pInfo->mRealIp;
    }
    if (!realIp.empty()) {
        client->SetSlsHost(realIp);
        client->SetSlsRealIpUpdateTime(time(NULL));
    } else if (pInfo->mLastUpdateTime >= client->GetSlsRealIpUpdateTime()) {
        const string& defaultEndpoint = GetAvailableEndpointWithTopPriority(region);
        if (!defaultEndpoint.empty()) {
            client->SetSlsHost(defaultEndpoint);
            client->SetSlsRealIpUpdateTime(time(NULL));
        }
    }
}

void SLSClientManager::UpdateRealIpThread() {
    LOG_INFO(sLogger, ("sls real ip update", "started"));
    int32_t lastUpdateRealIpTime = 0;
    vector<string> regionEndpointArray;
    vector<string> regionArray;
    unique_lock<mutex> lock(mUpdateRealIpThreadRunningMux);
    while (mIsUpdateRealIpThreadRunning) {
        int32_t curTime = time(NULL);
        bool updateFlag = curTime - lastUpdateRealIpTime > INT32_FLAG(send_switch_real_ip_interval);
        {
            // check force update
            lock_guard<mutex> lock(mRegionRealIpLock);
            auto iter = mRegionRealIpMap.begin();
            for (; iter != mRegionRealIpMap.end(); ++iter) {
                if (iter->second->mForceFlushFlag) {
                    iter->second->mForceFlushFlag = false;
                    updateFlag = true;
                    LOG_INFO(sLogger, ("force update real ip", iter->first));
                }
            }
        }
        if (updateFlag) {
            LOG_DEBUG(sLogger, ("start update real ip", ""));
            regionEndpointArray.clear();
            regionArray.clear();
            {
                lock_guard<mutex> lock(mRegionEndpointEntryMapLock);
                auto iter = mRegionEndpointEntryMap.begin();
                for (; iter != mRegionEndpointEntryMap.end(); ++iter) {
                    regionEndpointArray.push_back((iter->second).GetAvailableEndpointWithTopPriority());
                    regionArray.push_back(iter->first);
                }
            }
            for (size_t i = 0; i < regionEndpointArray.size(); ++i) {
                // no available endpoint
                if (regionEndpointArray[i].empty()) {
                    continue;
                }

                EndpointStatus status = UpdateRealIp(regionArray[i], regionEndpointArray[i]);
                if (status == EndpointStatus::STATUS_ERROR) {
                    UpdateEndpointStatus(regionArray[i], regionEndpointArray[i], false);
                }
            }
            lastUpdateRealIpTime = time(NULL);
        }
        if (mStopCV.wait_for(lock, chrono::seconds(1), [this]() { return !mIsUpdateRealIpThreadRunning; })) {
            break;
        }
    }
}

SLSClientManager::EndpointStatus SLSClientManager::UpdateRealIp(const string& region, const string& endpoint) {
    mUpdateRealIpClient->SetSlsHost(endpoint);
    EndpointStatus status = EndpointStatus::STATUS_ERROR;
    int64_t beginTime = GetCurrentTimeInMicroSeconds();
    try {
        sdk::GetRealIpResponse rsp;
        rsp = mUpdateRealIpClient->GetRealIp();

        if (!rsp.realIp.empty()) {
            SetRealIp(region, rsp.realIp);
            status = EndpointStatus::STATUS_OK_WITH_IP;
        } else {
            status = EndpointStatus::STATUS_OK_WITH_ENDPOINT;
            static int32_t sUpdateRealIpWarningCount = 0;
            if (sUpdateRealIpWarningCount++ % 100 == 0) {
                sUpdateRealIpWarningCount %= 100;
                LOG_WARNING(sLogger,
                            ("get real ip request succeeded but server did not give real ip, region",
                             region)("endpoint", endpoint));
            }

            // we should set real ip to empty string if server did not give real ip
            SetRealIp(region, "");
        }
    }
    // GetRealIp's implement should not throw LOGException, but we catch it to hold implement changing
    catch (sdk::LOGException& ex) {
        const string& errorCode = ex.GetErrorCode();
        LOG_DEBUG(sLogger, ("get real ip", "send fail")("errorCode", errorCode)("errorMessage", ex.GetMessage()));
        SendResult sendRst = ConvertErrorCode(errorCode);
        if (sendRst == SEND_NETWORK_ERROR)
            status = EndpointStatus::STATUS_ERROR;
    } catch (...) {
        LOG_ERROR(sLogger, ("get real ip", "send fail")("exception", "unknown"));
    }
    int64_t endTime = GetCurrentTimeInMicroSeconds();
    int32_t latency = int32_t((endTime - beginTime) / 1000); // ms
    LOG_DEBUG(sLogger,
              ("Get real ip, region", region)("endpoint", endpoint)("status", int(status))("latency", latency));
    return status;
}

void SLSClientManager::SetRealIp(const string& region, const string& ip) {
    lock_guard<mutex> lock(mRegionRealIpLock);
    RealIpInfo* pInfo = NULL;
    auto iter = mRegionRealIpMap.find(region);
    if (iter != mRegionRealIpMap.end()) {
        pInfo = iter->second;
    } else {
        pInfo = new RealIpInfo;
        mRegionRealIpMap.insert(make_pair(region, pInfo));
    }
    LOG_DEBUG(sLogger, ("set real ip, last", pInfo->mRealIp)("now", ip)("region", region));
    pInfo->SetRealIp(ip);
}

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
        } else if (TryCurlEndpoint("http://100.100.100.200/latest/meta-data")) {
            // containers in ACK can be connected to the above address, see
            // https://help.aliyun.com/document_detail/108460.html#section-akf-lwh-1gb.
            // Note: we can not distinguish ACK from K8S built on ECS
            env = "ACK-Sidecar";
        } else {
            env = "K8S-Sidecar";
        }
    } else if (AppConfig::GetInstance()->IsPurageContainerMode() || getenv("ALIYUN_LOGTAIL_CONFIG")) {
        env = "Docker";
    } else if (TryCurlEndpoint("http://100.100.100.200/latest/meta-data")) {
        env = "ECS";
    } else {
        env = "Others";
    }
    return env;
}

bool SLSClientManager::TryCurlEndpoint(const string& endpoint) {
    CURL* curl;
    for (size_t retryTimes = 1; retryTimes <= 5; retryTimes++) {
        curl = curl_easy_init();
        if (curl) {
            break;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        if (curl_easy_perform(curl) != CURLE_OK) {
            curl_easy_cleanup(curl);
            return false;
        }
        curl_easy_cleanup(curl);
        return true;
    }

    LOG_WARNING(
        sLogger,
        ("curl handler cannot be initialized during user environment identification", "user agent may be mislabeled"));
    return false;
}

} // namespace logtail
