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

#include "common/Flags.h"
#include "config/Config.h"
#include "interface/type.h"


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
        static NetworkConfig* sConfig = new NetworkConfig;
        return sConfig;
    }

    void ReportAlarm();

    void BeginLoadConfig() {
        mOldestConfigCreateTime = 0;
        mEnabled = false;
        mLastApplyedConfig = NULL;
        mAllNetworkConfigs.clear();
    }

    void LoadConfig(Config* config);

    void EndLoadConfig();

    bool Enabled() const { return mEnabled; }

    bool NeedReload() const { return mNeedReload; }

    void Reload() { mNeedReload = false; }

    /**
     * @brief Set the From Json String object
     *{
     * "observer_sls_network" :{
     *     "EBPF" : {
     *        "Enabled" : true
     *     }
     *     "PCAP" : {
     *        "Enabled" : true,
     *        "Filter" : "tcp and port 80",
     *        "Interface" : "eth0",
     *        "TimeoutMs" : 0,
     *        "Promiscuous" : 1
     *     }
     *     "Common" : {
     *        "FlushInterval" : 30,
     *        "FlushMetaInterval" : 30
     *     }
     * }
     *}
     *
     * @return std::string empty str is OK, otherwise return error msg
     */
    std::string SetFromJsonString();

    /**
     * @brief
     *
     * @return std::string empty str is OK, otherwise return error msg
     */
    std::string CheckValid();

    bool isOpenPartialSelectDump();

    void Clear() {
        mEnabled = false;
        // for ebpf
        mEBPFEnabled = false;
        mSampling = 100;
        mPid = -1;

        // for PCAP
        mPCAPEnabled = false;
        mPCAPFilter.clear();
        mPCAPInterface.clear();
        mPCAPPromiscuous = true;
        mPCAPTimeoutMs = 0;

        // common config
        mFlushOutInterval = 30;
        mFlushMetaInterval = 30;
        mFlushNetlinkInterval = 10;
        mSaveToDisk = false;
        mDropUnixSocket = true;
        boost::regex emptyRegex;
        mIncludeCmdRegex = emptyRegex;
        mExcludeCmdRegex = emptyRegex;
        mIncludeContainerNameRegex = emptyRegex;
        mExcludeContainerNameRegex = emptyRegex;
        mIncludePodNameRegex = emptyRegex;
        mExcludePodNameRegex = emptyRegex;
        mIncludeNamespaceNameRegex = emptyRegex;
        mExcludeNamespaceNameRegex = emptyRegex;
        mIncludeContainerLabels.clear();
        mExcludeContainerLabels.clear();
        mIncludeK8sLabels.clear();
        mExcludeK8sLabels.clear();
        mIncludeEnvs.clear();
        mExcludeEnvs.clear();
        mTags.clear();
        mProtocolAggCfg.clear();
        mDropUnixSocket = true;
        mDropLocalConnections = true;
        mDropUnknownSocket = true;
        mProtocolProcessFlag = -1;
        mLocalPort = false;
    }

    bool IsLegalProtocol(ProtocolType type) const {
        if (type == ProtocolType_None || type == ProtocolType_NumProto) {
            return false;
        }
        return this->mProtocolProcessFlag & (1 << (type - 1));
    }

    static std::string label2String(const std::unordered_map<std::string, boost::regex>& map) {
        return std::accumulate(map.begin(),
                               map.end(),
                               std::string(),
                               [](const std::string& s, const std::pair<const std::string, boost::regex>& p) {
                                   return s + p.first + "=" + p.second.str() + ",";
                               });
    }

    static std::pair<uint32_t, uint32_t> GetProtocolAggSize(ProtocolType protocolType) {
        static auto sInstance = GetInstance();
        auto iter = sInstance->mProtocolAggCfg.find(protocolType);
        if (iter == sInstance->mProtocolAggCfg.end()) {
            return std::make_pair(500, 5000);
        }
        return iter->second;
    }


    std::string ToString() const {
        std::string rst;
        rst.append("EBPF : ").append(mEBPFEnabled ? "true" : "false").append("\t");
        if (mEBPFEnabled) {
            rst.append("EBPFFilter pid : ").append(std::to_string(mPid)).append("\t");
        }
        rst.append("PCAP : ").append(mPCAPEnabled ? "true" : "false").append("\t");
        if (mPCAPEnabled) {
            rst.append("PCAPFilter : ").append(mPCAPFilter).append("\t");
            rst.append("PCAPInterface : ").append(mPCAPInterface).append("\t");
            rst.append("PCAPTimeoutMs : ").append(std::to_string(mPCAPTimeoutMs)).append("\t");
            rst.append("PCAPPromiscuous : ").append(std::to_string(mPCAPPromiscuous)).append("\t");
        }
        rst.append("Sampling : ").append(std::to_string(mSampling)).append("\t");
        rst.append("FlushOutInterval : ").append(std::to_string(mFlushOutInterval)).append("\t");
        rst.append("FlushMetaInterval : ").append(std::to_string(mFlushMetaInterval)).append("\t");
        rst.append("FlushNetlinkInterval : ").append(std::to_string(mFlushNetlinkInterval)).append("\t");
        rst.append("IncludeCmdRegex : ").append(mIncludeCmdRegex.str()).append("\t");
        rst.append("ExcludeCmdRegex : ").append(mExcludeCmdRegex.str()).append("\t");
        rst.append("IncludeContainerNameRegex : ").append(mIncludeContainerNameRegex.str()).append("\t");
        rst.append("ExcludeContainerNameRegex : ").append(mExcludeContainerNameRegex.str()).append("\t");
        rst.append("IncludeContainerLabels : ").append(label2String(mIncludeContainerLabels)).append("\t");
        rst.append("ExcludeContainerLabels : ").append(label2String(mExcludeContainerLabels)).append("\t");
        rst.append("IncludePodNameRegex : ").append(mIncludePodNameRegex.str()).append("\t");
        rst.append("ExcludePodNameRegex : ").append(mExcludePodNameRegex.str()).append("\t");
        rst.append("IncludeNamespaceNameRegex : ").append(mIncludeNamespaceNameRegex.str()).append("\t");
        rst.append("ExcludeNamespaceNameRegex : ").append(mExcludeNamespaceNameRegex.str()).append("\t");
        rst.append("IncludeK8sLabels : ").append(label2String(mIncludeK8sLabels)).append("\t");
        rst.append("ExcludeK8sLabels : ").append(label2String(mExcludeK8sLabels)).append("\t");
        rst.append("IncludeEnvs : ").append(label2String(mIncludeEnvs)).append("\t");
        rst.append("ExcludeEnvs : ").append(label2String(mExcludeEnvs)).append("\t");
        rst.append("DropUnixSocket : ").append(std::to_string(mDropUnixSocket)).append("\t");
        rst.append("DropLocalConnections : ").append(std::to_string(mDropLocalConnections)).append("\t");
        rst.append("DropUnknownSocket : ").append(std::to_string(mDropUnknownSocket)).append("\t");
        rst.append("LocalPort : ").append(std::to_string(mLocalPort)).append("\t");
        rst.append("ProtocolProcess : ");
        for (int i = 1; i < ProtocolType_NumProto; ++i) {
            if (IsLegalProtocol((ProtocolType)i)) {
                auto pair = GetProtocolAggSize((ProtocolType)i);
                rst.append(ProtocolTypeToString((ProtocolType)i))
                    .append("#")
                    .append("ClientSize:")
                    .append(std::to_string(pair.first))
                    .append("#ServerSize:")
                    .append(std::to_string(pair.second))
                    .append(", ");
            }
        }
        rst.append("\t");
        rst.append("Tags : ");
        for (const auto& item : mTags) {
            rst.append(item.first).append("@").append(item.second).append(",");
        }
        rst.append("\t");
        return rst;
    }

    int32_t mOldestConfigCreateTime = 0; // used to check which config is best
    volatile bool mEnabled = false;
    std::string mLastApplyedConfigDetail;
    Config* mLastApplyedConfig = NULL;
    std::vector<Config*> mAllNetworkConfigs;
    bool mNeedReload = false;

    // enable ebpf
    bool mEBPFEnabled = false;
    int mPid = -1;
    // collect config
    uint64_t mFlushOutInterval = 30;
    uint64_t mFlushMetaInterval = 30;
    uint64_t mFlushNetlinkInterval = 10;
    int mSampling = 100;
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
    bool mLocalPort = false;
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

    // for PCAP(PCAP would be removed in the feature.)
    bool mPCAPEnabled = false;
    std::string mPCAPFilter;
    std::string mPCAPInterface;
    bool mPCAPPromiscuous = true;
    int mPCAPTimeoutMs = 0;
    uint32_t mPCAPCacheConnSize = 2000;
};

} // namespace logtail
