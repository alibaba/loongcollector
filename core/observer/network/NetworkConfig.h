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

#include <ostream>
#include "common/Flags.h"
#include "config/Config.h"
#include "interface/type.h"
#include "bits/stl_numeric.h"


DECLARE_FLAG_INT64(sls_observer_network_gc_interval);
DECLARE_FLAG_INT64(sls_observer_network_probe_disable_process_interval);
DECLARE_FLAG_INT64(sls_observer_network_cleanall_disable_process_interval);
DECLARE_FLAG_INT64(sls_observer_network_process_timeout);
DECLARE_FLAG_INT64(sls_observer_network_hostname_timeout);
DECLARE_FLAG_INT64(sls_observer_network_process_no_connection_timeout);
DECLARE_FLAG_INT64(sls_observer_network_process_destroyed_timeout);
DECLARE_FLAG_INT64(sls_observer_network_connection_timeout);
DECLARE_FLAG_INT64(sls_observer_network_connection_closed_timeout);
DECLARE_FLAG_INT32(sls_observer_network_no_data_sleep_interval_ms);
DECLARE_FLAG_INT32(sls_observer_network_pcap_loop_count);
DECLARE_FLAG_BOOL(sls_observer_network_protocol_stat);


namespace logtail {
struct NetworkConfig {
    NetworkConfig();

    static NetworkConfig* GetInstance() {
        static auto sConfig = new NetworkConfig;
        return sConfig;
    }

    void ReportAlarm();

    void BeginLoadConfig() {
        mOldestConfigCreateTime = 0;
        mEnabled = false;
        mLastApplyedConfig = nullptr;
        mAllNetworkConfigs.clear();
    }

    void LoadConfig(Config* config);

    void EndLoadConfig();

    bool Enabled() const { return mEnabled; }

    bool NeedReload() const { return mNeedReload; }

    void Reload() { mNeedReload = false; }

    std::string SetFromJsonString();

    /**
     * @brief
     *
     * @return std::string empty str is OK, otherwise return error msg
     */
    std::string CheckValid();

    bool isOpenPartialSelectDump();

    void Clear();

    std::string ToString() const;


    static std::pair<uint32_t, uint32_t> GetProtocolAggSize(ProtocolType protocolType) {
        static auto sInstance = GetInstance();
        auto iter = sInstance->mProtocolAggCfg.find(protocolType);
        if (iter == sInstance->mProtocolAggCfg.end()) {
            return std::make_pair(500, 5000);
        }
        return iter->second;
    }

    static std::string label2String(const std::unordered_map<std::string, boost::regex>& map) {
        return std::accumulate(map.begin(),
                               map.end(),
                               std::string(),
                               [](const std::string& s, const std::pair<const std::string, boost::regex>& p) {
                                   return s + p.first + "=" + p.second.str() + ",";
                               });
    }

    bool IsLegalProtocol(ProtocolType type) const {
        if (type == ProtocolType_None || type == ProtocolType_NumProto) {
            return false;
        }
        return this->mProtocolProcessFlag & (1 << (type - 1));
    }


    int32_t mOldestConfigCreateTime = 0; // used to check which config is best
    volatile bool mEnabled = false;
    std::string mLastApplyedConfigDetail;
    Config* mLastApplyedConfig = nullptr;
    std::vector<Config*> mAllNetworkConfigs;
    bool mNeedReload = false;

    // enable ebpf
    bool mEBPFEnabled = false;
    int mEBPFPid = -1;
    // for PCAP(PCAP would be removed in the feature.)
    bool mPCAPEnabled = false;
    std::string mPCAPFilter;
    std::string mPCAPInterface;
    bool mPCAPPromiscuous = true;
    int mPCAPTimeoutMs = 0;
    uint32_t mPCAPCacheConnSize = 2000;
    // collect config
    int mSampling = 100;
    uint64_t mFlushOutL4Interval = 60;
    uint64_t mFlushOutL7Interval = 15;
    uint64_t mFlushMetaInterval = 30;
    uint64_t mFlushNetlinkInterval = 10;
    boost::regex mIncludeCmdRegex;
    boost::regex mExcludeCmdRegex;
    boost::regex mIncludeContainerNameRegex;
    boost::regex mExcludeContainerNameRegex;
    std::unordered_map<std::string, boost::regex> mIncludeContainerLabels;
    std::unordered_map<std::string, boost::regex> mExcludeContainerLabels;
    boost::regex mIncludePodNameRegex;
    boost::regex mExcludePodNameRegex;
    boost::regex mIncludeNamespaceNameRegex;
    boost::regex mExcludeNamespaceNameRegex;
    std::unordered_map<std::string, boost::regex> mIncludeK8sLabels;
    std::unordered_map<std::string, boost::regex> mExcludeK8sLabels;
    std::unordered_map<std::string, boost::regex> mIncludeEnvs;
    std::unordered_map<std::string, boost::regex> mExcludeEnvs;
    bool mDropUnixSocket = true;
    bool mDropLocalConnections = true;
    bool mDropUnknownSocket = true;
    uint32_t mProtocolProcessFlag = -1;
    std::vector<std::pair<std::string, std::string>> mTags;
    std::unordered_map<uint8_t, std::pair<uint32_t, uint32_t>> mProtocolAggCfg;

    // for local test
    bool mSaveToDisk = false;
    bool mLocalFileEnabled = false;
    int64_t localPickConnectionHashId = -1;
    int64_t localPickPID = -1;
    int64_t localPickDstPort = -1;
    int64_t localPickSrcPort = -1;
};

} // namespace logtail
