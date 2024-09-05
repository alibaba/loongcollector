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
#include <json/json.h>
#include "common/JsonUtil.h"
#include "ExceptionBase.h"
#include "plugin/input/InputObserverNetwork.h"

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

#define OBSERVER_CONFIG_EXTRACT_REGEXP(jsonvalue, param) \
    do { \
        std::string str##param = GetStringValue(jsonvalue, #param, ""); \
        if (!str##param.empty()) { \
            this->m##param = boost::regex(str##param, boost::regex_constants::no_except); \
            if (this->m##param.status() != 0) { \
                throw ExceptionBase(std::string("regex illegal:") + #param); \
            } \
        } else { \
            this->m##param = boost::regex(); \
        } \
    } while (0)

#define OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(jsonvalue, param) \
    do { \
        this->m##param.erase(this->m##param.begin(), this->m##param.end()); \
        if ((jsonvalue).isMember(#param) && commonValue[#param].isObject()) { \
            Json::Value& labelsVal = (jsonvalue)[#param]; \
            for (auto item = labelsVal.begin(); item != labelsVal.end(); item++) { \
                std::string key = item.name(); \
                std::string val = labelsVal[item.name()].asString(); \
                if (!val.empty()) { \
                    auto reg = boost::regex(val, boost::regex_constants::no_except); \
                    if (reg.status() != 0) { \
                        throw ExceptionBase(std::string("regex illegal:") + #param); \
                    } \
                    this->m##param.insert(std::make_pair(key, reg)); \
                } \
            } \
        } \
    } while (0)
#define OBSERVER_CONFIG_EXTRACT_INT(jsonvalue, param, defaultVal, prefix) \
    do { \
        this->m##prefix##param = GetIntValue(jsonvalue, #param, defaultVal); \
    } while (0)

#define OBSERVER_CONFIG_EXTRACT_BOOL(jsonvalue, param, defaultVal, prefix) \
    do { \
        this->m##prefix##param = GetBoolValue(jsonvalue, #param, defaultVal); \
    } while (0)

#define OBSERVER_CONFIG_EXTRACT_STRING(jsonvalue, param, defaultVal, prefix) \
    do { \
        this->m##prefix##param = GetStringValue(jsonvalue, #param, defaultVal); \
    } while (0)


namespace logtail {


NetworkConfig::NetworkConfig() {
}


void NetworkConfig::ReportAlarm() {
    if (mAllNetworkConfigs.size() < (size_t)2) {
        return;
    }
    std::stringstream ss;
    ss << "Load multi observer config, only one config is enabled, others not work. Enabled config is, project : "
       << mLastApplyedConfig->GetContext().GetProjectName()
       << ", store : " << mLastApplyedConfig->GetContext().GetLogstoreName() << ".\n";
    ss << "Disabled configs : ";
    for (auto& config : mAllNetworkConfigs) {
        if (config.second == mLastApplyedConfig) {
            continue;
        }
        ss << "project : " << mLastApplyedConfig->GetContext().GetProjectName()
           << ", store : " << mLastApplyedConfig->GetContext().GetLogstoreName() << ".\n";
    }
    std::string alarmStr = ss.str();
    for (auto& config : mAllNetworkConfigs) {
        LogtailAlarm::GetInstance()->SendAlarm(MULTI_OBSERVER_ALARM,
                                               alarmStr,
                                               config.second->GetContext().GetProjectName(),
                                               config.second->GetContext().GetLogstoreName(),
                                               config.second->GetContext().GetRegion());
    }
    LOG_WARNING(sLogger, (alarmStr, ""));
}


void NetworkConfig::LoadConfig(const Pipeline* config) {
    if (mLastApplyedConfig == NULL) {
        mLastApplyedConfig = config;
    }
    if (mOldestConfigCreateTime > config->GetContext().GetCreateTime()) {
        mOldestConfigCreateTime = config->GetContext().GetCreateTime();
        mLastApplyedConfig = config;
    }
    // mAllNetworkConfigs.push_back(config);
}

void NetworkConfig::EndLoadConfig() {
    if (mLastApplyedConfig == NULL) {
        mLastApplyedConfigDetail.clear();
        return;
    }
    ReportAlarm();
    const InputObserverNetwork* plugin
        = static_cast<const InputObserverNetwork*>(mLastApplyedConfig->GetInputs()[0]->GetPlugin());
    if (mLastApplyedConfigDetail != plugin->mDetail) {
        LOG_INFO(sLogger, ("reload observer config, last", mLastApplyedConfigDetail)("new", plugin->mDetail));
        mLastApplyedConfigDetail = plugin->mDetail;
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

    Json::Value jsonRoot;
    Json::CharReaderBuilder builder;
    builder["collectComments"] = false;
    std::unique_ptr<Json::CharReader> jsonReader(builder.newCharReader());
    std::string jsonParseErrs;
    if (!jsonReader->parse(mLastApplyedConfigDetail.data(),
                           mLastApplyedConfigDetail.data() + mLastApplyedConfigDetail.size(),
                           &jsonRoot,
                           &jsonParseErrs)) {
        return std::string("invalid config format : ") + jsonParseErrs;
    }
    if (jsonRoot.isArray() && jsonRoot.size() == 1) {
        jsonRoot = jsonRoot[0];
    } else if (!jsonRoot.isObject()) {
        jsonRoot = {};
    }
    if (jsonRoot.empty()) {
        return std::string("invalid config format : ") + jsonParseErrs;
    }

    try {
        if (jsonRoot.isMember("EBPF") && jsonRoot["EBPF"].isObject()) {
            Json::Value& ebpfValue = jsonRoot["EBPF"];
            OBSERVER_CONFIG_EXTRACT_BOOL(ebpfValue, Enabled, false, EBPF);
            OBSERVER_CONFIG_EXTRACT_INT(ebpfValue, Pid, -1, EBPF);
        }
        if (jsonRoot.isMember("PCAP") && jsonRoot["PCAP"].isObject()) {
            Json::Value& pcapValue = jsonRoot["PCAP"];
            OBSERVER_CONFIG_EXTRACT_BOOL(pcapValue, Enabled, false, PCAP);
            OBSERVER_CONFIG_EXTRACT_BOOL(pcapValue, Promiscuous, true, PCAP);
            OBSERVER_CONFIG_EXTRACT_INT(pcapValue, TimeoutMs, 0, PCAP);
            OBSERVER_CONFIG_EXTRACT_STRING(pcapValue, Filter, "", PCAP);
            OBSERVER_CONFIG_EXTRACT_STRING(pcapValue, Interface, "", PCAP);
        }

        if (jsonRoot.isMember("Common") && jsonRoot["Common"].isObject()) {
            Json::Value& commonValue = jsonRoot["Common"];
            OBSERVER_CONFIG_EXTRACT_INT(commonValue, FlushOutL4Interval, 60, );
            OBSERVER_CONFIG_EXTRACT_INT(commonValue, FlushOutL7Interval, 15, );
            OBSERVER_CONFIG_EXTRACT_INT(commonValue, FlushMetaInterval, 30, );
            OBSERVER_CONFIG_EXTRACT_INT(commonValue, FlushNetlinkInterval, 10, );
            OBSERVER_CONFIG_EXTRACT_INT(commonValue, Sampling, 100, );
            OBSERVER_CONFIG_EXTRACT_BOOL(commonValue, SaveToDisk, false, );
            OBSERVER_CONFIG_EXTRACT_BOOL(commonValue, DropUnixSocket, true, );
            OBSERVER_CONFIG_EXTRACT_BOOL(commonValue, DropLocalConnections, true, );
            OBSERVER_CONFIG_EXTRACT_BOOL(commonValue, DropUnknownSocket, true, );
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, IncludeContainerLabels);
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, ExcludeContainerLabels);
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, IncludeK8sLabels);
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, ExcludeK8sLabels);
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, IncludeEnvs);
            OBSERVER_CONFIG_EXTRACT_REGEXP_MAP(commonValue, ExcludeEnvs);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, IncludeCmdRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, ExcludeCmdRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, IncludeContainerNameRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, ExcludeContainerNameRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, IncludePodNameRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, ExcludePodNameRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, IncludeNamespaceNameRegex);
            OBSERVER_CONFIG_EXTRACT_REGEXP(commonValue, ExcludeNamespaceNameRegex);
            // partial open protocol processing.
            mProtocolProcessFlag = GetBoolValue(commonValue, "ProtocolProcess", true) ? -1 : 0;
            if (mProtocolProcessFlag != 0) {
                std::vector<std::string> includeProtocolVec;
                if (commonValue.isMember("IncludeProtocols") && commonValue["IncludeProtocols"].isArray()) {
                    Json::Value& includeProtocols = commonValue["IncludeProtocols"];
                    if (commonValue["IncludeProtocols"].size() >= 0) {
                        mProtocolProcessFlag = 0; // reset flag to open selected protocol.
                        for (const auto& includeProtocol : includeProtocols) {
                            for (int i = 1; i < ProtocolType_NumProto; ++i) {
                                if (strcasecmp(ProtocolTypeToString((ProtocolType)i).c_str(),
                                               includeProtocol.asString().c_str())
                                    == 0) {
                                    mProtocolProcessFlag |= (1 << (i - 1));
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            // config protocol advanced config.
            if (commonValue.isMember("ProtocolAggCfg") && commonValue["ProtocolAggCfg"].isObject()) {
                Json::Value& protocolAggCfg = commonValue["ProtocolAggCfg"];
                for (const auto& item : protocolAggCfg.getMemberNames()) {
                    for (int i = 1; i < ProtocolType_NumProto; ++i) {
                        if (strcasecmp(ProtocolTypeToString((ProtocolType)i).c_str(), item.c_str()) == 0
                            && protocolAggCfg[item].isObject() && (mProtocolProcessFlag & 1 << (i - 1))) {
                            Json::Value& itemCfg = protocolAggCfg[item];
                            this->mProtocolAggCfg[i] = std::make_pair(GetIntValue(itemCfg, "ClientSize"),
                                                                      GetIntValue(itemCfg, "ServerSize"));
                        }
                    }
                }
            }
            std::string cluster = GetStringValue(commonValue, "Cluster", "");
            if (!cluster.empty()) {
                mTags.emplace_back("__tag__:cluster", cluster);
            }
            // append global tags.
            if (commonValue.isMember("Tags") && commonValue["Tags"].isObject()) {
                Json::Value& tags = commonValue["Tags"];
                for (const auto& item : tags.getMemberNames()) {
                    if (item.empty()) {
                        continue;
                    }
                    mTags.emplace_back("__tag__:" + item, GetStringValue(tags, item));
                }
            }
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


std::string NetworkConfig::ToString() const {
    std::string rst;
    rst.append("EBPF : ").append(mEBPFEnabled ? "true" : "false").append("\t");
    if (mEBPFEnabled) {
        rst.append("EBPFFilter pid : ").append(std::to_string(mEBPFPid)).append("\t");
    }
    rst.append("PCAP : ").append(mPCAPEnabled ? "true" : "false").append("\t");
    if (mPCAPEnabled) {
        rst.append("PCAPFilter : ").append(mPCAPFilter).append("\t");
        rst.append("PCAPInterface : ").append(mPCAPInterface).append("\t");
        rst.append("PCAPTimeoutMs : ").append(std::to_string(mPCAPTimeoutMs)).append("\t");
        rst.append("PCAPPromiscuous : ").append(std::to_string(mPCAPPromiscuous)).append("\t");
    }
    rst.append("Sampling : ").append(std::to_string(mSampling)).append("\t");
    rst.append("FlushOutL4Interval : ").append(std::to_string(mFlushOutL4Interval)).append("\t");
    rst.append("FlushOutL7Interval : ").append(std::to_string(mFlushOutL7Interval)).append("\t");
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
    rst.append("DropUnixSocket : ").append(mDropUnixSocket ? "true" : "false").append("\t");
    rst.append("DropLocalConnections : ").append(mDropLocalConnections ? "true" : "false").append("\t");
    rst.append("DropUnknownSocket : ").append(mDropUnknownSocket ? "true" : "false").append("\t");
    rst.append("ProtocolProcess : {");
    for (int i = 1; i < ProtocolType_NumProto; ++i) {
        if (this->IsLegalProtocol(static_cast<ProtocolType>(i))) {
            auto pair = GetProtocolAggSize(static_cast<ProtocolType>(i));
            rst.append(ProtocolTypeToString(static_cast<ProtocolType>(i)))
                .append(" ClientSize: ")
                .append(std::to_string(pair.first))
                .append(" ServerSize: ")
                .append(std::to_string(pair.second))
                .append("\t");
        }
    }
    rst.append("} Tags : ");
    for (const auto& item : mTags) {
        rst.append(item.first).append("@").append(item.second).append(",");
    }
    rst.append("\t");
    return rst;
}
void NetworkConfig::Clear() {
    mEnabled = false;
    mEBPFEnabled = false;
    mSampling = 100;
    mEBPFPid = -1;
    mPCAPEnabled = false;
    mPCAPFilter.clear();
    mPCAPInterface.clear();
    mPCAPPromiscuous = true;
    mPCAPTimeoutMs = 0;
    mFlushOutL4Interval = 60;
    mFlushOutL7Interval = 15;
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
}


} // namespace logtail
