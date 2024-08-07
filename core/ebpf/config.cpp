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

//////
bool IsProcessNamespaceFilterTypeValid(const std::string& type);

bool InitObserverNetworkOptionInner(const Json::Value& probeConfig,
                               nami::ObserverNetworkOption& thisObserverNetworkOption,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    std::string errorMsg;
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

//////
bool InitSecurityFileFilter(const Json::Value& config,
                            nami::SecurityFileFilter& thisFileFilter,
                            const PipelineContext* mContext,
                            const std::string& sName) {
    std::string errorMsg;
    for (auto& fileFilterItem : config["FilePathFilter"]) {
        nami::SecurityFileFilterItem thisFileFilterItem;
        // FilePath (Mandatory)
        if (!GetMandatoryStringParam(fileFilterItem, "FilePath", thisFileFilterItem.mFilePath, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        // FileName (Optional)
        if (!GetOptionalStringParam(fileFilterItem, "FileName", thisFileFilterItem.mFileName, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
        thisFileFilter.mFileFilterItem.emplace_back(thisFileFilterItem);
    }
    return true;
}

bool InitSecurityProcessFilter(const Json::Value& config,
                               nami::SecurityProcessFilter& thisProcessFilter,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    std::string errorMsg;
    // NamespaceFilter (Optional)
    if (config.isMember("NamespaceFilter")) {
        if (!config["NamespaceFilter"].isArray()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "NamespaceFilter is not of type list",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else {
            for (auto& namespaceFilterConfig : config["NamespaceFilter"]) {
                nami::SecurityProcessNamespaceFilter thisProcessNamespaceFilter;
                // NamespaceType (Mandatory)
                if (!GetMandatoryStringParam(
                        namespaceFilterConfig, "NamespaceType", thisProcessNamespaceFilter.mNamespaceType, errorMsg)
                    || !IsProcessNamespaceFilterTypeValid(thisProcessNamespaceFilter.mNamespaceType)) {
                    PARAM_ERROR_RETURN(mContext->GetLogger(),
                                       mContext->GetAlarm(),
                                       errorMsg,
                                       sName,
                                       mContext->GetConfigName(),
                                       mContext->GetProjectName(),
                                       mContext->GetLogstoreName(),
                                       mContext->GetRegion());
                }
                // ValueList (Mandatory)
                if (!GetMandatoryListParam<std::string>(
                        namespaceFilterConfig, "ValueList", thisProcessNamespaceFilter.mValueList, errorMsg)) {
                    PARAM_ERROR_RETURN(mContext->GetLogger(),
                                       mContext->GetAlarm(),
                                       errorMsg,
                                       sName,
                                       mContext->GetConfigName(),
                                       mContext->GetProjectName(),
                                       mContext->GetLogstoreName(),
                                       mContext->GetRegion());
                }
                thisProcessFilter.mNamespaceFilter.emplace_back(thisProcessNamespaceFilter);
            }
        }
    }

    // NamespaceBlackFilter (Optional)
    if (config.isMember("NamespaceBlackFilter")) {
        // 如果用户两个filter都配置了，不去显式阻塞流水线，但是会打印警告并只执行白名单
        if (config.isMember("NamespaceFilter")) {
            PARAM_WARNING_IGNORE(
                mContext->GetLogger(),
                mContext->GetAlarm(),
                "Both NamespaceFilter and NamespaceBlackFilter are configured, only NamespaceFilter will be executed",
                sName,
                mContext->GetConfigName(),
                mContext->GetProjectName(),
                mContext->GetLogstoreName(),
                mContext->GetRegion());
        } else if (!config["NamespaceBlackFilter"].isArray()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "NamespaceBlackFilter is not of type list",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else {
            for (auto& namespaceBlackFilterConfig : config["NamespaceBlackFilter"]) {
                nami::SecurityProcessNamespaceFilter thisProcessNamespaceFilter;
                // NamespaceType (Mandatory)
                if (!GetMandatoryStringParam(namespaceBlackFilterConfig,
                                             "NamespaceType",
                                             thisProcessNamespaceFilter.mNamespaceType,
                                             errorMsg)
                    || !IsProcessNamespaceFilterTypeValid(thisProcessNamespaceFilter.mNamespaceType)) {
                    PARAM_ERROR_RETURN(mContext->GetLogger(),
                                       mContext->GetAlarm(),
                                       errorMsg,
                                       sName,
                                       mContext->GetConfigName(),
                                       mContext->GetProjectName(),
                                       mContext->GetLogstoreName(),
                                       mContext->GetRegion());
                }
                // ValueList (Mandatory)
                if (!GetMandatoryListParam<std::string>(
                        namespaceBlackFilterConfig, "ValueList", thisProcessNamespaceFilter.mValueList, errorMsg)) {
                    PARAM_ERROR_RETURN(mContext->GetLogger(),
                                       mContext->GetAlarm(),
                                       errorMsg,
                                       sName,
                                       mContext->GetConfigName(),
                                       mContext->GetProjectName(),
                                       mContext->GetLogstoreName(),
                                       mContext->GetRegion());
                }
                thisProcessFilter.mNamespaceBlackFilter.emplace_back(thisProcessNamespaceFilter);
            }
        }
    }
    return true;
}

bool InitSecurityNetworkFilter(const Json::Value& config,
                               nami::SecurityNetworkFilter& thisNetworkFilter,
                               const PipelineContext* mContext,
                               const std::string& sName) {
    std::string errorMsg;
    // DestAddrList (Optional)
    if (!GetOptionalListParam<std::string>(config, "DestAddrList", thisNetworkFilter.mDestAddrList, errorMsg)) {
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
    if (!GetOptionalListParam<uint32_t>(config, "DestPortList", thisNetworkFilter.mDestPortList, errorMsg)) {
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
    if (!GetOptionalListParam<std::string>(config, "DestAddrBlackList", thisNetworkFilter.mDestAddrBlackList, errorMsg)) {
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
    if (!GetOptionalListParam<uint32_t>(config, "DestPortBlackList", thisNetworkFilter.mDestPortBlackList, errorMsg)) {
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
    if (!GetOptionalListParam<std::string>(config, "SourceAddrList", thisNetworkFilter.mSourceAddrList, errorMsg)) {
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
    if (!GetOptionalListParam<uint32_t>(config, "SourcePortList", thisNetworkFilter.mSourcePortList, errorMsg)) {
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
    if (!GetOptionalListParam<std::string>(
            config, "SourceAddrBlackList", thisNetworkFilter.mSourceAddrBlackList, errorMsg)) {
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
    if (!GetOptionalListParam<uint32_t>(
            config, "SourcePortBlackList", thisNetworkFilter.mSourcePortBlackList, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    return true;
}

bool IsProcessNamespaceFilterTypeValid(const std::string& type) {
    const std::unordered_set<std::string> dic
        = {"Uts", "Ipc", "Mnt", "Pid", "PidForChildren", "Net", "Cgroup", "User", "Time", "TimeForChildren"};
    return dic.find(type) != dic.end();
}


bool SecurityOptions::Init(SecurityFilterType filterType,
                           const Json::Value& config,
                           const PipelineContext* mContext,
                           const std::string& sName) {
    std::string errorMsg;
    // ProbeConfig (Mandatory)
    if (!IsValidList(config, "ProbeConfig", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    for (auto& innerConfig : config["ProbeConfig"]) {
        nami::SecurityOption thisSecurityOption;

        std::string errorMsg;
        // CallName (Optional)
        if (!GetOptionalListParam<std::string>(innerConfig, "CallName", thisSecurityOption.call_names_, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                mContext->GetAlarm(),
                                errorMsg,
                                sName,
                                mContext->GetConfigName(),
                                mContext->GetProjectName(),
                                mContext->GetLogstoreName(),
                                mContext->GetRegion());
        }

        // Filter
        switch (filterType) {
            case SecurityFilterType::FILE: {
                nami::SecurityFileFilter thisFileFilter;
                if (!IsValidList(innerConfig, "FilePathFilter", errorMsg)) {
                    PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                        mContext->GetAlarm(),
                                        errorMsg,
                                        sName,
                                        mContext->GetConfigName(),
                                        mContext->GetProjectName(),
                                        mContext->GetLogstoreName(),
                                        mContext->GetRegion());
                } else {
                    if (!InitSecurityFileFilter(innerConfig, thisFileFilter, mContext, sName)) {
                        return false;
                    }
                }
                thisSecurityOption.filter_.emplace<nami::SecurityFileFilter>(thisFileFilter);
                break;
            }
            case SecurityFilterType::PROCESS: {
                nami::SecurityProcessFilter thisProcessFilter;
                if (!InitSecurityProcessFilter(innerConfig, thisProcessFilter, mContext, sName)) {
                    return false;
                }
                thisSecurityOption.filter_.emplace<nami::SecurityProcessFilter>(thisProcessFilter);
                break;
            }
            case SecurityFilterType::NETWORK: {
                nami::SecurityNetworkFilter thisNetworkFilter;
                if (!IsValidMap(innerConfig, "AddrFilter", errorMsg)) {
                    PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                        mContext->GetAlarm(),
                                        errorMsg,
                                        sName,
                                        mContext->GetConfigName(),
                                        mContext->GetProjectName(),
                                        mContext->GetLogstoreName(),
                                        mContext->GetRegion());
                } else {
                    const Json::Value& filterConfig = innerConfig["AddrFilter"];
                    if (!InitSecurityNetworkFilter(filterConfig, thisNetworkFilter, mContext, sName)) {
                        return false;
                    }
                }
                thisSecurityOption.filter_.emplace<nami::SecurityNetworkFilter>(thisNetworkFilter);
                break;
            }
            default:
                PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                    mContext->GetAlarm(),
                                    "Unknown filter type",
                                    sName,
                                    mContext->GetConfigName(),
                                    mContext->GetProjectName(),
                                    mContext->GetLogstoreName(),
                                    mContext->GetRegion());
                return false;
        }


        // if (!thisSecurityOption.Init(filterType, innerConfig, mContext, sName)) {
        //     return false;
        // }
        mOptionList.emplace_back(thisSecurityOption);
    }
    mFilterType = filterType;
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
