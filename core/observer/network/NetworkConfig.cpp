// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "NetworkConfig.h"
#include <sstream>
#include "LogtailAlarm.h"
#include "logger/Logger.h"
#include "json/json.h"
#include "common/JsonUtil.h"
#include "ExceptionBase.h"

DEFINE_FLAG_INT64(sls_observer_network_gc_interval, "SLS Observer NetWork GC interval seconds", 30);
DEFINE_FLAG_INT64(sls_observer_network_probe_disable_process_interval,
                  "SLS Observer NetWork probe disable processes interval seconds",
                  3);
DEFINE_FLAG_INT64(sls_observer_network_cleanall_disable_process_interval,
                  "SLS Observer NetWork clean all disable processes interval seconds",
                  1800);
DEFINE_FLAG_INT64(sls_observer_network_process_timeout, "SLS Observer NetWork normal process timeout seconds", 600);
DEFINE_FLAG_INT64(sls_observer_network_hostname_timeout, "SLS Observer NetWork dns hostname timeout seconds", 3600);
DEFINE_FLAG_INT64(sls_observer_network_process_no_connection_timeout,
                  "SLS Observer NetWork no connection process timeout seconds",
                  180);
DEFINE_FLAG_INT64(sls_observer_network_process_destroyed_timeout,
                  "SLS Observer NetWork destroyed process timeout seconds",
                  180);
DEFINE_FLAG_INT64(sls_observer_network_connection_timeout,
                  "SLS Observer NetWork normal connection timeout seconds",
                  300);
DEFINE_FLAG_INT64(sls_observer_network_connection_closed_timeout,
                  "SLS Observer NetWork closed connection timeout seconds",
                  5);
DEFINE_FLAG_INT32(sls_observer_network_no_data_sleep_interval_ms, "SLS Observer NetWork no data sleep interval ms", 10);
DEFINE_FLAG_INT32(sls_observer_network_pcap_loop_count, "SLS Observer NetWork PCAP loop count", 100);
DEFINE_FLAG_BOOL(sls_observer_network_protocol_stat, "SLS Observer NetWork protocol stat output", false);

#define OBSERVER_CONFIG_TO_REGEX(jsonvalue, param) \
    { \
        std::string str##param = GetStringValue(jsonvalue, #param, ""); \
        if (!str##param.empty()) { \
            this->m##param = boost::regex(str##param, boost::regex_constants::no_except); \
            if (this->m##param.status() != 0) { \
                throw ExceptionBase(std::string("regex illegal:") + #param); \
            } \
        } else { \
            this->m##param = boost::regex(); \
        } \
    }

#define OBSERVER_CONFIG_MAP_TO_REGEX(jsonvalue, param) \
    { \
        this->m##param.erase(this->m##param.begin(), this->m##param.end()); \
        if ((jsonvalue).isMember(#param) && commonValue[#param].isObject()) { \
            Json::Value& labelsVal = (jsonvalue)[#param]; \
            for (auto item = labelsVal.begin(); item != labelsVal.end(); item++) { \
                std::string key = item.memberName(); \
                std::string val = labelsVal[item.memberName()].asString(); \
                if (!val.empty()) { \
                    auto reg = boost::regex(val, boost::regex_constants::no_except); \
                    if (reg.status() != 0) { \
                        throw ExceptionBase(std::string("regex illegal:") + #param); \
                    } \
                    this->m##param.insert(std::make_pair(key, reg)); \
                } \
            } \
        } \
    }
namespace logtail {


NetworkConfig::NetworkConfig() {
}


void NetworkConfig::ReportAlarm() {
    if (mAllNetworkConfigs.size() < (size_t)2) {
        return;
    }
    std::stringstream ss;
    ss << "Load multi observer config, only one config is enabled, others not work. Enabled config is, project : "
       << mLastApplyedConfig->mProjectName << ", store : " << mLastApplyedConfig->mCategory << ".\n";
    ss << "Disabled configs : ";
    for (auto& config : mAllNetworkConfigs) {
        if (config == mLastApplyedConfig) {
            continue;
        }
        ss << "project : " << mLastApplyedConfig->mProjectName << ", store : " << mLastApplyedConfig->mCategory
           << ".\n";
    }
    std::string alarmStr = ss.str();
    for (auto& config : mAllNetworkConfigs) {
        LogtailAlarm::GetInstance()->SendAlarm(
            MULTI_OBSERVER_ALARM, alarmStr, config->mProjectName, config->mCategory, config->mRegion);
    }
    LOG_WARNING(sLogger, (alarmStr, ""));
}


void NetworkConfig::LoadConfig(Config* config) {
    if (!config->mObserverFlag) {
        return;
    }
    if (mLastApplyedConfig == NULL) {
        mLastApplyedConfig = config;
    }
    if (mOldestConfigCreateTime > config->mCreateTime) {
        mOldestConfigCreateTime = config->mCreateTime;
        mLastApplyedConfig = config;
    }
    mAllNetworkConfigs.push_back(config);
}

void NetworkConfig::EndLoadConfig() {
    if (mLastApplyedConfig == NULL) {
        mLastApplyedConfigDetail.clear();
        return;
    }
    ReportAlarm();
    if (mLastApplyedConfigDetail != mLastApplyedConfig->mObserverConfig) {
        LOG_INFO(
            sLogger,
            ("reload observer config, last", mLastApplyedConfigDetail)("new", mLastApplyedConfig->mObserverConfig));
        mLastApplyedConfigDetail = mLastApplyedConfig->mObserverConfig;
        std::string parseResult = SetFromJsonString();
        if (parseResult.empty()) {
            LOG_INFO(sLogger, ("new loaded observer config", ToString()));
        } else {
            LOG_WARNING(sLogger, ("parse observer config, result", parseResult));
        }
        mNeedReload = true;
    }
}

std::string NetworkConfig::CheckValid() {
    if (!mEBPFEnabled && !mPCAPEnabled) {
        return "both ebpf and pcap are not enabled";
    }
    // todo, check params
    return "";
}

std::string NetworkConfig::SetFromJsonString() {
    Clear();
    mEnabled = true;

    Json::Reader jsonReader;
    Json::Value jsonRoot;
    bool parseOk = jsonReader.parse(mLastApplyedConfigDetail, jsonRoot);
    if (parseOk == false) {
        return std::string("invalid config format : ") + jsonReader.getFormatedErrorMessages();
    }
    if (jsonRoot.isArray() && jsonRoot.size() == 1) {
        jsonRoot = jsonRoot[0];
    } else if (!jsonRoot.isObject()) {
        jsonRoot = {};
    }
    if (jsonRoot.empty()) {
        return std::string("invalid config format : ") + jsonReader.getFormatedErrorMessages();
    }

    try {
        if (!(jsonRoot.isMember("type") && jsonRoot["type"].asString() == "observer_ilogtail_network")
            || !(jsonRoot.isMember("detail") && jsonRoot["detail"].isObject())) {
            return "invalid format, observer_sls_network is not exist or error type";
        }
        Json::Value& obserValue = jsonRoot["detail"];

        if (obserValue.isMember("Common") && obserValue["Common"].isObject()) {
            Json::Value& commonValue = obserValue["Common"];
            mFlushOutInterval = GetIntValue(commonValue, "FlushOutInterval", 30);
            mFlushMetaInterval = GetIntValue(commonValue, "FlushMetaInterval", 30);
            mFlushNetlinkInterval = GetIntValue(commonValue, "FlushNetlinkInterval", 10);
            mSampling = GetIntValue(commonValue, "Sampling", 100);
            mSaveToDisk = GetBoolValue(commonValue, "SaveToDisk", false);
            mProtocolProcessFlag = GetBoolValue(commonValue, "ProtocolProcess", true) ? -1 : 0;
            mLocalPort = GetBoolValue(commonValue, "LocalPort", false);
            mDropUnixSocket = GetBoolValue(commonValue, "DropUnixSocket", true);
            mDropLocalConnections = GetBoolValue(commonValue, "DropLocalConnections", true);
            mDropUnknownSocket = GetBoolValue(commonValue, "DropUnknownSocket", true);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, IncludeContainerLabels);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, ExcludeContainerLabels);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, IncludeK8sLabels);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, ExcludeK8sLabels);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, IncludeEnvs);
            OBSERVER_CONFIG_MAP_TO_REGEX(commonValue, ExcludeEnvs);
            OBSERVER_CONFIG_TO_REGEX(commonValue, IncludeCmdRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, ExcludeCmdRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, IncludeContainerNameRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, ExcludeContainerNameRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, IncludePodNameRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, ExcludePodNameRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, IncludeNamespaceNameRegex);
            OBSERVER_CONFIG_TO_REGEX(commonValue, ExcludeNamespaceNameRegex);
            std::vector<std::string> includeProtocolVec;
            if (commonValue.isMember("IncludeProtocols") && commonValue["IncludeProtocols"].isArray()) {
                Json::Value& includeProtocols = commonValue["IncludeProtocols"];
                for (const auto& includeProtocol : includeProtocols) {
                    includeProtocolVec.push_back(includeProtocol.asString());
                }
            }
            if (!includeProtocolVec.empty()) {
                mProtocolProcessFlag = 0;
                for (int i = 1; i < ProtocolType_NumProto; ++i) {
                    bool enable = false;
                    for (const auto& item : includeProtocolVec) {
                        if (strcasecmp(ProtocolTypeToString((ProtocolType)i).c_str(), item.c_str()) == 0) {
                            enable = true;
                            break;
                        }
                    }
                    if (enable) {
                        mProtocolProcessFlag |= (1 << (i - 1));
                    }
                }
            }
            if (commonValue.isMember("Tags") && commonValue["Tags"].isObject()) {
                Json::Value& tags = commonValue["Tags"];
                for (const auto& item : tags.getMemberNames()) {
                    if (item.empty()) {
                        continue;
                    }
                    mTags.emplace_back("__tag__:" + item, GetStringValue(tags, item));
                }
            }
            if (commonValue.isMember("ProtocolAggCfg") && commonValue["ProtocolAggCfg"].isObject()) {
                Json::Value& protocolAggCfg = commonValue["ProtocolAggCfg"];
                for (const auto& item : protocolAggCfg.getMemberNames()) {
                    for (int i = 1; i < ProtocolType_NumProto; ++i) {
                        if (strcasecmp(ProtocolTypeToString((ProtocolType)i).c_str(), item.c_str()) == 0
                            && protocolAggCfg[item].isObject()) {
                            Json::Value& itemCfg = protocolAggCfg[item];
                            this->mProtocolAggCfg[i] = std::make_pair(GetIntValue(itemCfg, "ClientSize"),
                                                                      GetIntValue(itemCfg, "ServerSize"));
                        }
                    }
                }
            }
        }

        if (obserValue.isMember("EBPF") && obserValue["EBPF"].isObject()) {
            Json::Value& ebpfValue = obserValue["EBPF"];
            mEBPFEnabled = GetBoolValue(ebpfValue, "Enabled", false);
            mPid = GetIntValue(ebpfValue, "Pid", -1);
        }

        if (obserValue.isMember("PCAP") && obserValue["PCAP"].isObject()) {
            Json::Value& pcapValue = obserValue["PCAP"];
            mPCAPEnabled = GetBoolValue(pcapValue, "Enabled", false);
            mPCAPFilter = GetStringValue(pcapValue, "Filter", "");
            mPCAPInterface = GetStringValue(pcapValue, "Interface", "");
            mPCAPTimeoutMs = GetIntValue(pcapValue, "TimeoutMs", 0);
            mPCAPPromiscuous = GetBoolValue(pcapValue, "Promiscuous", true);
        }
    } catch (...) {
        return "invalid config pattern";
    }
    return CheckValid();
}

bool NetworkConfig::isOpenPartialSelectDump() {
    return -1 != this->localPickPID || -1 != this->localPickConnectionHashId || -1 != this->localPickSrcPort
        || -1 != this->localPickDstPort;
}

} // namespace logtail
