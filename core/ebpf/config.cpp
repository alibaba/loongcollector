// Copyright 2023 iLogtail Authors
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

#include <string>
#include <unordered_set>

#include "logger/Logger.h"
#include "ebpf/config.h"
#include "common/ParamExtractor.h"
#include "common/Flags.h"

DEFINE_FLAG_INT32(ebpf_receive_event_chan_cap, "ebpf receive kernel event queue size", 4096);
DEFINE_FLAG_BOOL(ebpf_admin_config_debug_mode, "ebpf admin config debug mode", false);
DEFINE_FLAG_STRING(ebpf_admin_config_log_level, "ebpf admin config log level", "warn");
DEFINE_FLAG_BOOL(ebpf_admin_config_push_all_span, "if admin config push all span", false);
DEFINE_FLAG_INT32(ebpf_aggregation_config_agg_window_second, "ebpf data aggregation window time", 15);
DEFINE_FLAG_STRING(ebpf_converage_config_strategy, "ebpf converage strategy", "combine");
DEFINE_FLAG_STRING(ebpf_sample_config_strategy, "ebpf sample strategy", "fixedRate");
DEFINE_FLAG_DOUBLE(ebpf_sample_config_config_rate, "ebpf sample rate", 0.01);
DEFINE_FLAG_INT32(ebpf_socket_probe_config_slow_request_threshold_ms, "ebpf socket probe slow request threshold", 500);
DEFINE_FLAG_INT32(ebpf_socket_probe_config_max_conn_trackers, "ebpf socket probe max connect trackers", 10000);
DEFINE_FLAG_INT32(ebpf_socket_probe_config_max_band_width_mb_per_sec, "ebpf socket probe max bandwidth per sec", 30);
DEFINE_FLAG_INT32(ebpf_socket_probe_config_max_raw_record_per_sec, "ebpf socket probe max raw record per sec", 100000);
DEFINE_FLAG_INT32(ebpf_profile_probe_config_profile_sample_rate, "ebpf profile probe profile sample rate", 10);
DEFINE_FLAG_INT32(ebpf_profile_probe_config_profile_upload_duration, "ebpf profile probe profile upload duration", 10);
DEFINE_FLAG_BOOL(ebpf_process_probe_config_enable_oom_detect, "if ebpf process probe enable oom detect", false);

namespace logtail {
namespace ebpf {

static const std::unordered_map<SecurityProbeType, std::unordered_set<std::string>> callNameDict
    = {{SecurityProbeType::PROCESS,
        {"sys_enter_execve", "sys_enter_clone", "disassociate_ctty", "acct_process", "wake_up_new_task"}},
       {SecurityProbeType::FILE, {"security_file_permission", "security_mmap_file", "security_path_truncate"}},
       {SecurityProbeType::NETWORK, {"tcp_connect", "tcp_close", "tcp_sendmsg"}}};

bool InitObserverNetworkOptionInner(const Json::Value& probeConfig,
                               nami::ObserverNetworkOption& thisObserverNetworkOption,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    std::string errorMsg;
    // EnableEvent (Optional)
    if (!GetOptionalBoolParam(probeConfig, "EnableLog", thisObserverNetworkOption.mEnableLog, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // EnableSpan (Optional)
    if (!GetOptionalBoolParam(probeConfig, "EnableSpan", thisObserverNetworkOption.mEnableSpan, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // EnableMetric (Optional)
    if (!GetOptionalBoolParam(probeConfig, "EnableMetric", thisObserverNetworkOption.mEnableMetric, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // MeterHandlerType (Optional)
    if (!GetOptionalStringParam(probeConfig, "MeterHandlerType", thisObserverNetworkOption.mMeterHandlerType, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // SpanHandlerType (Optional)
    if (!GetOptionalStringParam(probeConfig, "SpanHandlerType", thisObserverNetworkOption.mSpanHandlerType, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // EnableProtocols (Optional)
    if (!GetOptionalListParam(probeConfig, "EnableProtocols", thisObserverNetworkOption.mEnableProtocols, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // EnableProtocols (Optional)
    if (!GetOptionalBoolParam(
            probeConfig, "DisableProtocolParse", thisObserverNetworkOption.mDisableProtocolParse, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    // DisableConnStats (Optional)
    if (!GetOptionalBoolParam(probeConfig, "DisableConnStats", thisObserverNetworkOption.mDisableConnStats, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    // EnableConnTrackerDump (Optional)
    if (!GetOptionalBoolParam(
            probeConfig, "EnableConnTrackerDump", thisObserverNetworkOption.mEnableConnTrackerDump, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    }
    return true;
}

bool ExtractProbeConfig(const Json::Value& config, const PipelineContext* mContext, const std::string& sName, Json::Value& probeConfig) {
    std::string errorMsg;
    if (!IsValidMap(config, "ProbeConfig", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    probeConfig = config["ProbeConfig"];
    return true;
}

bool InitObserverNetworkOption(const Json::Value& config, 
                               nami::ObserverNetworkOption& thisObserverNetworkOption,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    Json::Value probeConfig;
    if (!ExtractProbeConfig(config, mContext, sName, probeConfig)) {
        return false;
    }

    return InitObserverNetworkOptionInner(probeConfig, thisObserverNetworkOption, mContext, sName);
}

void InitSecurityFileFilter(const Json::Value& config,
                            nami::SecurityFileFilter& thisFileFilter,
                            const PipelineContext* mContext,
                            const std::string& sName) {
    std::string errorMsg;
    // FilePathFilter (Optional)
    if (!config.isMember("FilePathFilter")) {
        // No FilePathFilter, do nothing, no warning
    } else if (!config["FilePathFilter"].isArray()) {
        // FilePathFilter is not empty but of wrong type
        errorMsg = "FilePathFilter is not of type list";
    } else if (!GetOptionalListFilterParam<std::string>(
                   config, "FilePathFilter", thisFileFilter.mFilePathList, errorMsg)) {
        // FilePathFilter has element of wrong type
    }
    if (!errorMsg.empty()) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
}

void InitSecurityNetworkFilter(const Json::Value& config,
                               nami::SecurityNetworkFilter& thisNetworkFilter,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    std::string errorMsg;
    // AddrFilter (Optional)
    if (!config.isMember("AddrFilter")) {
        // No AddrFilter, do nothing
    } else if (!config["AddrFilter"].isObject()) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             "AddrFilter is not of type map",
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    } else {
        auto addrFilterConfig = config["AddrFilter"];
        // DestAddrList (Optional)
        if (!GetOptionalListFilterParam<std::string>(
                addrFilterConfig, "DestAddrList", thisNetworkFilter.mDestAddrList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // DestPortList (Optional)
        if (!GetOptionalListFilterParam<uint32_t>(
                addrFilterConfig, "DestPortList", thisNetworkFilter.mDestPortList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // DestAddrBlackList (Optional)
        if (!GetOptionalListFilterParam<std::string>(
                addrFilterConfig, "DestAddrBlackList", thisNetworkFilter.mDestAddrBlackList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // DestPortBlackList (Optional)
        if (!GetOptionalListFilterParam<uint32_t>(
                addrFilterConfig, "DestPortBlackList", thisNetworkFilter.mDestPortBlackList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // SourceAddrList (Optional)
        if (!GetOptionalListFilterParam<std::string>(
                addrFilterConfig, "SourceAddrList", thisNetworkFilter.mSourceAddrList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // SourcePortList (Optional)
        if (!GetOptionalListFilterParam<uint32_t>(
                addrFilterConfig, "SourcePortList", thisNetworkFilter.mSourcePortList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // SourceAddrBlackList (Optional)
        if (!GetOptionalListFilterParam<std::string>(
                addrFilterConfig, "SourceAddrBlackList", thisNetworkFilter.mSourceAddrBlackList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        // SourcePortBlackList (Optional)
        if (!GetOptionalListFilterParam<uint32_t>(
                addrFilterConfig, "SourcePortBlackList", thisNetworkFilter.mSourcePortBlackList, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
    }
}

void GetSecurityProbeDefaultCallName(SecurityProbeType type, std::vector<std::string>& callNames) {
    callNames.assign(callNameDict.at(type).begin(), callNameDict.at(type).end());
}

bool CheckProbeConfigValid(const Json::Value& config, std::string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember("ProbeConfig")) {
        // No ProbeConfig, use default, no warning
        return false;
    } else if (!config["ProbeConfig"].isObject()) {
        // ProbeConfig is not empty but of wrong type, use default
        errorMsg = "ProbeConfig is not of type map, use probe config with default filter";
        return false;
    }
    return true;
}

bool SecurityOptions::Init(SecurityProbeType probeType,
                           const Json::Value& config,
                           const PipelineContext* mContext,
                           const std::string& sName) {
    std::string errorMsg;

    // ProbeConfig (Optional)
    if (!CheckProbeConfigValid(config, errorMsg)) {
        if (!errorMsg.empty()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        nami::SecurityOption thisSecurityOption;
        GetSecurityProbeDefaultCallName(probeType, thisSecurityOption.call_names_);
        mOptionList.emplace_back(thisSecurityOption);
        return true;
    }
    auto innerConfig = config["ProbeConfig"];
    nami::SecurityOption thisSecurityOption;
    // Genral Filter (Optional)
    std::variant<std::monostate, nami::SecurityFileFilter, nami::SecurityNetworkFilter> thisFilter;
    switch (probeType) {
        case SecurityProbeType::FILE: {
            nami::SecurityFileFilter thisFileFilter;
            InitSecurityFileFilter(innerConfig, thisFileFilter, mContext, sName);
            thisFilter.emplace<nami::SecurityFileFilter>(thisFileFilter);
            break;
        }
        case SecurityProbeType::NETWORK: {
            nami::SecurityNetworkFilter thisNetworkFilter;
            InitSecurityNetworkFilter(innerConfig, thisNetworkFilter, mContext, sName);
            thisFilter.emplace<nami::SecurityNetworkFilter>(thisNetworkFilter);
            break;
        }
        case SecurityProbeType::PROCESS: {
            break;
        }
        default:
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                    mContext->GetAlarm(),
                                    "Unknown security eBPF probe type",
                                    sName,
                                    mContext->GetConfigName(),
                                    mContext->GetProjectName(),
                                    mContext->GetLogstoreName(),
                                    mContext->GetRegion());
    }
    thisSecurityOption.filter_ = thisFilter;
    GetSecurityProbeDefaultCallName(probeType, thisSecurityOption.call_names_);    
    mOptionList.emplace_back(thisSecurityOption);
    mProbeType = probeType;
    return true;
}

//////
void eBPFAdminConfig::LoadEbpfConfig(const Json::Value& confJson) {
    // receive_event_chan_cap (Optional)
    mReceiveEventChanCap = INT32_FLAG(ebpf_receive_event_chan_cap);
    // admin_config (Optional)
    mAdminConfig = AdminConfig{BOOL_FLAG(ebpf_admin_config_debug_mode), STRING_FLAG(ebpf_admin_config_log_level), BOOL_FLAG(ebpf_admin_config_push_all_span)};
    // aggregation_config (Optional)
    mAggregationConfig = AggregationConfig{INT32_FLAG(ebpf_aggregation_config_agg_window_second)};
    // converage_config (Optional)
    mConverageConfig = ConverageConfig{STRING_FLAG(ebpf_converage_config_strategy)};
    // sample_config (Optional)
    mSampleConfig = SampleConfig{STRING_FLAG(ebpf_sample_config_strategy), {DOUBLE_FLAG(ebpf_sample_config_config_rate)}};
    // socket_probe_config (Optional)
    mSocketProbeConfig = SocketProbeConfig{INT32_FLAG(ebpf_socket_probe_config_slow_request_threshold_ms), INT32_FLAG(ebpf_socket_probe_config_max_conn_trackers), INT32_FLAG(ebpf_socket_probe_config_max_band_width_mb_per_sec), INT32_FLAG(ebpf_socket_probe_config_max_raw_record_per_sec)};
    // profile_probe_config (Optional)
    mProfileProbeConfig = ProfileProbeConfig{INT32_FLAG(ebpf_profile_probe_config_profile_sample_rate), INT32_FLAG(ebpf_profile_probe_config_profile_upload_duration)};
    // process_probe_config (Optional)
    mProcessProbeConfig = ProcessProbeConfig{BOOL_FLAG(ebpf_process_probe_config_enable_oom_detect)};
}

} // ebpf
} // logtail
