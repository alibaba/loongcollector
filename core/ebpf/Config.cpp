// Copyright 2025 iLogtail Authors
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

#include "ebpf/Config.h"

#include <string>
#include <unordered_set>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "logger/Logger.h"

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
DEFINE_FLAG_INT32(ebpf_file_filter_max_num, "ebpf file filter max num", 64);

namespace logtail::ebpf {

static const std::unordered_map<SecurityProbeType, std::unordered_set<std::string>> kCallNameDict
    = {{SecurityProbeType::PROCESS,
        {"sys_enter_execve", "sys_enter_clone", "disassociate_ctty", "acct_process", "wake_up_new_task"}},
       {SecurityProbeType::FILE, {"security_file_permission", "security_mmap_file", "security_path_truncate"}},
       {SecurityProbeType::NETWORK, {"tcp_connect", "tcp_close", "tcp_sendmsg"}}};

bool InitObserverNetworkOptionInner(const Json::Value& probeConfig,
                                    ObserverNetworkOption& thisObserverNetworkOption,
                                    const CollectionPipelineContext* mContext,
                                    const std::string& sName) {
    std::string errorMsg;
    // ==== l7 config
    if (!IsValidMap(probeConfig, "L7Config", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    Json::Value l7Config;
    l7Config = probeConfig["L7Config"];
    // Enable (Optional)
    if (!GetMandatoryBoolParam(l7Config, "Enable", thisObserverNetworkOption.mL7Config.mEnable, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    // EnableSpan (Optional)
    if (!GetOptionalBoolParam(l7Config, "EnableSpan", thisObserverNetworkOption.mL7Config.mEnableSpan, errorMsg)) {
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
    if (!GetOptionalBoolParam(l7Config, "EnableMetric", thisObserverNetworkOption.mL7Config.mEnableMetric, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // EnableLog (Optional)
    if (!GetOptionalBoolParam(l7Config, "EnableLog", thisObserverNetworkOption.mL7Config.mEnableLog, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // SampleRate (Optional)
    if (!GetOptionalDoubleParam(l7Config, "SampleRate", thisObserverNetworkOption.mL7Config.mSampleRate, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // ==== l4 config
    if (!IsValidMap(probeConfig, "L4Config", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    Json::Value l4Config;
    l4Config = probeConfig["L4Config"];
    // Enable (Optional)
    if (!GetMandatoryBoolParam(l4Config, "Enable", thisObserverNetworkOption.mL4Config.mEnable, errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }

    // ==== app config
    if (!IsValidMap(probeConfig, "ApmConfig", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    Json::Value appConfig;
    appConfig = probeConfig["ApmConfig"];
    // Workspace (Optional)
    if (!GetOptionalStringParam(appConfig, "Workspace", thisObserverNetworkOption.mApmConfig.mWorkspace, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }
    // AppName (Optional)
    if (!GetOptionalStringParam(appConfig, "AppName", thisObserverNetworkOption.mApmConfig.mAppName, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // AppId (Optional)
    if (!GetOptionalStringParam(appConfig, "AppId", thisObserverNetworkOption.mApmConfig.mAppId, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // Language (Optional)
    if (!GetOptionalStringParam(appConfig, "Language", thisObserverNetworkOption.mApmConfig.mLanguage, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    // ServiceId (Optional)
    if (!GetOptionalStringParam(appConfig, "ServiceId", thisObserverNetworkOption.mApmConfig.mServiceId, errorMsg)) {
        PARAM_WARNING_IGNORE(mContext->GetLogger(),
                             mContext->GetAlarm(),
                             errorMsg,
                             sName,
                             mContext->GetConfigName(),
                             mContext->GetProjectName(),
                             mContext->GetLogstoreName(),
                             mContext->GetRegion());
    }

    std::string selectorStr;
    if (GetOptionalStringParam(probeConfig, "WorkloadSelectors", selectorStr, errorMsg) && selectorStr.empty()) {
        // no selectors ...
        return true;
    }

    // ==== workload selectors
    if (!IsValidList(probeConfig, "WorkloadSelectors", errorMsg)) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           errorMsg,
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    Json::Value selectors;
    selectors = probeConfig["WorkloadSelectors"];
    std::vector<WorkloadSelector> selectorVec;
    for (const auto& selector : selectors) {
        // AppId (Optional)
        WorkloadSelector item;
        if (!GetMandatoryStringParam(selector, "Namespace", item.mNamespace, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        if (!GetMandatoryStringParam(selector, "WorkloadName", item.mWorkloadName, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        if (!GetMandatoryStringParam(selector, "WorkloadKind", item.mWorkloadKind, errorMsg)) {
            PARAM_ERROR_RETURN(mContext->GetLogger(),
                               mContext->GetAlarm(),
                               errorMsg,
                               sName,
                               mContext->GetConfigName(),
                               mContext->GetProjectName(),
                               mContext->GetLogstoreName(),
                               mContext->GetRegion());
        }
        item.mWorkloadKind = ToLowerCaseString(item.mWorkloadKind);
        selectorVec.push_back(item);
    }
    thisObserverNetworkOption.mSelectors = std::move(selectorVec);
    return true;
}

bool ExtractProbeConfig(const Json::Value& config,
                        const CollectionPipelineContext* mContext,
                        const std::string& sName,
                        Json::Value& probeConfig) {
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
                               ObserverNetworkOption& thisObserverNetworkOption,
                               const CollectionPipelineContext* mContext,
                               const std::string& sName) {
    Json::Value probeConfig;
    if (!ExtractProbeConfig(config, mContext, sName, probeConfig)) {
        return false;
    }

    return InitObserverNetworkOptionInner(probeConfig, thisObserverNetworkOption, mContext, sName);
}

void InitSecurityFileFilter(const Json::Value& config,
                            SecurityFileFilter& thisFileFilter,
                            const CollectionPipelineContext* mContext,
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
    } else {
        // FilePathFilter succeeded, deduplication
        size_t originalSize = thisFileFilter.mFilePathList.size();
        std::unordered_set<std::string> uniquePaths;
        std::vector<std::string> deduplicatedPaths;
        deduplicatedPaths.reserve(originalSize);

        const auto maxFilterCount = static_cast<unsigned int>(INT32_FLAG(ebpf_file_filter_max_num));
        for (const auto& path : thisFileFilter.mFilePathList) {
            if (uniquePaths.size() >= maxFilterCount) {
                LOG_WARNING(sLogger, ("file filter count exceeds limit", maxFilterCount));
                break;
            }

            if (uniquePaths.insert(path).second) {
                deduplicatedPaths.push_back(path);
            }
        }

        if (originalSize > deduplicatedPaths.size()) {
            LOG_INFO(sLogger,
                     ("FilePathFilter deduplicated", originalSize - deduplicatedPaths.size())(
                         "original_count", originalSize)("deduplicated_count", deduplicatedPaths.size()));
        }

        thisFileFilter.mFilePathList = std::move(deduplicatedPaths);
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
                               SecurityNetworkFilter& thisNetworkFilter,
                               const CollectionPipelineContext* mContext,
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
    callNames.assign(kCallNameDict.at(type).begin(), kCallNameDict.at(type).end());
}

bool CheckProbeConfigValid(const Json::Value& config, std::string& errorMsg) {
    errorMsg.clear();
    if (!config.isMember("ProbeConfig")) {
        // No ProbeConfig, use default, no warning
        return false;
    }
    if (!config["ProbeConfig"].isObject()) {
        // ProbeConfig is not empty but of wrong type, use default
        errorMsg = "ProbeConfig is not of type map, use probe config with default filter";
        return false;
    }
    return true;
}

bool SecurityOptions::Init(SecurityProbeType probeType,
                           const Json::Value& config,
                           const CollectionPipelineContext* mContext,
                           const std::string& sName) {
    std::string errorMsg;
    SecurityOption thisSecurityOption;

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
    } else {
        const auto& innerConfig = config["ProbeConfig"];
        // Genral Filter (Optional)
        std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> thisFilter;
        switch (probeType) {
            case SecurityProbeType::FILE: {
                SecurityFileFilter thisFileFilter;
                InitSecurityFileFilter(innerConfig, thisFileFilter, mContext, sName);
                thisFilter.emplace<SecurityFileFilter>(std::move(thisFileFilter));
                break;
            }
            case SecurityProbeType::NETWORK: {
                SecurityNetworkFilter thisNetworkFilter;
                InitSecurityNetworkFilter(innerConfig, thisNetworkFilter, mContext, sName);
                thisFilter.emplace<SecurityNetworkFilter>(std::move(thisNetworkFilter));
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
        thisSecurityOption.mFilter = thisFilter;
    }
    GetSecurityProbeDefaultCallName(probeType, thisSecurityOption.mCallNames);
    mOptionList.emplace_back(std::move(thisSecurityOption));
    mProbeType = probeType;
    return true;
}

//////
void eBPFAdminConfig::LoadEbpfConfig(const Json::Value& confJson) {
    // receive_event_chan_cap (Optional)
    mReceiveEventChanCap = INT32_FLAG(ebpf_receive_event_chan_cap);
    // admin_config (Optional)
    mAdminConfig = AdminConfig{BOOL_FLAG(ebpf_admin_config_debug_mode),
                               STRING_FLAG(ebpf_admin_config_log_level),
                               BOOL_FLAG(ebpf_admin_config_push_all_span)};
    // aggregation_config (Optional)
    mAggregationConfig = AggregationConfig{INT32_FLAG(ebpf_aggregation_config_agg_window_second)};
    // converage_config (Optional)
    mConverageConfig = ConverageConfig{STRING_FLAG(ebpf_converage_config_strategy)};
    // sample_config (Optional)
    mSampleConfig
        = SampleConfig{STRING_FLAG(ebpf_sample_config_strategy), {DOUBLE_FLAG(ebpf_sample_config_config_rate)}};
    // socket_probe_config (Optional)
    mSocketProbeConfig = SocketProbeConfig{INT32_FLAG(ebpf_socket_probe_config_slow_request_threshold_ms),
                                           INT32_FLAG(ebpf_socket_probe_config_max_conn_trackers),
                                           INT32_FLAG(ebpf_socket_probe_config_max_band_width_mb_per_sec),
                                           INT32_FLAG(ebpf_socket_probe_config_max_raw_record_per_sec)};
    // profile_probe_config (Optional)
    mProfileProbeConfig = ProfileProbeConfig{INT32_FLAG(ebpf_profile_probe_config_profile_sample_rate),
                                             INT32_FLAG(ebpf_profile_probe_config_profile_upload_duration)};
    // process_probe_config (Optional)
    mProcessProbeConfig = ProcessProbeConfig{BOOL_FLAG(ebpf_process_probe_config_enable_oom_detect)};
}

} // namespace logtail::ebpf
