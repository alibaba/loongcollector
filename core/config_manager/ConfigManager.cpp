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

#include "ConfigManager.h"
#include <curl/curl.h>
#include <cctype>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(__linux__)
#include <unistd.h>
#include <fnmatch.h>
#endif
#include <limits.h>
#include <unordered_map>
#include <unordered_set>
#include <re2/re2.h>
#include <set>
#include <vector>
#include <fstream>
#include "common/util.h"
#include "common/LogtailCommonFlags.h"
#include "common/JsonUtil.h"
#include "common/HashUtil.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "common/Constants.h"
#include "common/ExceptionBase.h"
#include "common/CompressTools.h"
#include "common/ErrorUtil.h"
#include "common/TimeUtil.h"
#include "common/StringTools.h"
#include "common/GlobalPara.h"
#include "common/version.h"
#include "config/UserLogConfigParser.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "sdk/Common.h"
#include "sdk/CurlImp.h"
#include "sdk/Exception.h"
#include "app_config/AppConfig.h"
#include "checkpoint/CheckPointManager.h"
#include "event_handler/EventHandler.h"
#include "controller/EventDispatcher.h"
#include "sender/Sender.h"
#include "processor/LogProcess.h"
#include "processor/LogFilter.h"

using namespace std;
using namespace logtail;

DEFINE_FLAG_STRING(logtail_profile_aliuid, "default user's aliuid", "");
DEFINE_FLAG_STRING(logtail_profile_access_key_id, "default user's accessKeyId", "");
DEFINE_FLAG_STRING(logtail_profile_access_key, "default user's LogtailAccessKey", "");
DEFINE_FLAG_STRING(default_access_key_id, "", "");
DEFINE_FLAG_STRING(default_access_key, "", "");

DEFINE_FLAG_INT32(config_update_interval, "second", 10);

namespace logtail {
void ConfigManager::CleanUnusedUserAK() {
}

ConfigManager::ConfigManager() {
    SetDefaultProfileProjectName(STRING_FLAG(profile_project_name));
    SetDefaultProfileRegion(STRING_FLAG(default_region_name));
}

ConfigManager::~ConfigManager() {
    try {
        if (mCheckUpdateThreadPtr.get() != NULL)
            mCheckUpdateThreadPtr->GetValue(100);
    } catch (...) {
    }
}

// LoadConfig loads config by @configName.
bool ConfigManager::LoadConfig(const string& configName) {
    // Load logtail config at first, eg. user-defined-ids.
    ReloadLogtailSysConf();

    Json::Value userLogJson; // will contains the root value after parsing.
    ParseConfResult userLogRes = ParseConfig(configName, userLogJson);
    if (userLogRes != CONFIG_OK) {
        if (userLogRes == CONFIG_NOT_EXIST)
            LOG_DEBUG(sLogger, ("load user config fail, file not exist", configName));
        else if (userLogRes == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger, ("load user config fail, file content is not valid json", configName));
            LogtailAlarm::GetInstance()->SendAlarm(USER_CONFIG_ALARM, string("the user config is not valid json"));
        }
    } else {
        mConfigJson = userLogJson;
        // load global config
        if (userLogJson.isMember(GLOBAL_CONFIG_NODE)) {
            LoadGlobalConfig(userLogJson[GLOBAL_CONFIG_NODE]);
        }
    }

    // load single config as well as local config.
    return LoadAllConfig();
}

bool ConfigManager::UpdateAccessKey(const std::string& aliuid,
                                    std::string& accessKeyId,
                                    std::string& accessKey,
                                    int32_t& lastUpdateTime) {
    lastUpdateTime = GetUserAK(aliuid, accessKeyId, accessKey);
    if ((time(NULL) - lastUpdateTime) < INT32_FLAG(request_access_key_interval))
        return false;

    SetUserAK(aliuid, accessKeyId, accessKey);
    LOG_INFO(sLogger, ("GetAccessKey Success, accessKeyId", accessKeyId));
    return true;
}

// CheckUpdateThread is the routine of thread this->mCheckUpdateThreadPtr, created in function InitUpdateConfig.
//
// Its main job is to check whether there are config updates by calling GetLocalConfigUpdate.
// If any, it retrieves the updated data and stores it in mLocalYamlConfigDirMap,
// which EventDispatcher's Dispatch thread (main thread of Logtail) uses to perform the actual update.
//
// Synchronization between these two threads is implemented by value of mUpdateStat (with memory barrier):
// - mUpdateStat == NORMAL means nothing changed, shared datas are accessed by mCheckUpdateThreadPtr.
// - mUpdateStat == UPDATE_CONFIG, it means something changed and shared datas are available.
//   In this situation, GetConfigUpdate will stop checkiing (IsUpdate()==true), and dispatcher thread
//   will access shared datas to apply updates.
bool ConfigManager::CheckUpdateThread(bool configExistFlag) {
    usleep((rand() % 10) * 100 * 1000);
    int32_t lastCheckTime = 0;
    int32_t checkInterval = INT32_FLAG(config_update_interval);
    while (mThreadIsRunning) {
        int32_t curTime = time(NULL);

        if (curTime - lastCheckTime >= checkInterval) {
            AppConfig::ConfigServerAddress configServerAddress = AppConfig::GetInstance()->GetConfigServerAddress();
            if ("" != configServerAddress.host) {
                google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult> checkResults;
                google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail> configDetails;

                checkResults = SendHeartbeat(configServerAddress);
                if (checkResults.size() > 0) {
                    configDetails = FetchPipelineConfig(configServerAddress, checkResults);
                    UpdateRemoteConfig(checkResults, configDetails);
                }  
            }

            if (IsUpdate() && GetLocalConfigUpdate()) {
                StartUpdateConfig();
            }
            lastCheckTime = curTime;
        }

        if (mThreadIsRunning)
            sleep(1);
        else
            break;
    }
    return true;
}

void ConfigManager::InitUpdateConfig(bool configExistFlag) {
    ConfigManagerBase::InitUpdateConfig(configExistFlag);

    mCheckUpdateThreadPtr = CreateThread([this, configExistFlag]() { CheckUpdateThread(configExistFlag); });
}

void ConfigManager::GetRemoteConfigUpdate() {
}

bool ConfigManager::GetRegionStatus(const string& region) {
    return true;
}

void ConfigManager::SetStartWorkerStatus(const std::string& result, const std::string& message) {
}

void ConfigManager::CreateCustomizedFuseConfig() {
}

std::string ConfigManager::CheckPluginFlusher(Json::Value& configJSON) {
    return configJSON.toStyledString();
}

Json::Value& ConfigManager::CheckPluginProcessor(Json::Value& pluginConfigJson, const Json::Value& rootConfigJson) {
    if (pluginConfigJson.isMember("processors")
        && (pluginConfigJson["processors"].isObject() || pluginConfigJson["processors"].isArray())) {
        // patch enable_log_position_meta to split processor if exists ...
        if (rootConfigJson["advanced"] && rootConfigJson["advanced"]["enable_log_position_meta"]) {
            for (int i = 0; i < pluginConfigJson["processors"].size(); i++) {
                Json::Value& processorConfigJson = pluginConfigJson["processors"][i];
                if (processorConfigJson["type"] == "processor_split_log_string"
                    || processorConfigJson["type"] == "processor_split_log_regex") {
                    if (processorConfigJson["detail"]) {
                        processorConfigJson["detail"]["EnableLogPositionMeta"]
                            = rootConfigJson["advanced"]["enable_log_position_meta"];
                    }
                    break;
                }
                pluginConfigJson["processors"][i] = processorConfigJson;
            }
        }
    }
    return pluginConfigJson;
}

// ConfigServer
google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult> ConfigManager::SendHeartbeat(AppConfig::ConfigServerAddress configServerAddress) {
    configserver::proto::HeartBeatRequest heartBeatReq;
    configserver::proto::AgentAttributes attributes;
    std::string requestID = sdk::Base64Enconde(string("heartbeat").append(to_string(time(NULL))));
    heartBeatReq.set_request_id(requestID);
    heartBeatReq.set_agent_id(GetInstanceId());
    heartBeatReq.set_agent_type("iLogtail");
    attributes.set_version(ILOGTAIL_VERSION);
    attributes.set_ip(LogFileProfiler::mIpAddr);
    heartBeatReq.set_allocated_attributes(&attributes);
    heartBeatReq.mutable_tags()->CopyFrom({AppConfig::GetInstance()->GetConfigServerTags().begin(), AppConfig::GetInstance()->GetConfigServerTags().end()});
    heartBeatReq.set_running_status("");
    heartBeatReq.set_startup_time(0);
    heartBeatReq.set_interval(INT32_FLAG(config_update_interval));

    google::protobuf::RepeatedPtrField<configserver::proto::ConfigInfo> pipelineConfigs;
    for (std::unordered_map<std::string, int64_t>::iterator it = mServerYamlConfigVersionMap.begin(); it != mServerYamlConfigVersionMap.end(); it++) {
        configserver::proto::ConfigInfo info;
        info.set_type(configserver::proto::PIPELINE_CONFIG);
        info.set_name(it->first);
        info.set_version(it->second);
        pipelineConfigs.AddAllocated(&info);
    }
    heartBeatReq.mutable_pipeline_configs()->CopyFrom(pipelineConfigs);

    string operation = sdk::CONFIGSERVERAGENT;
    operation.append("/").append("HeartBeat");
    map<string, string> httpHeader;
    httpHeader[sdk::CONTENT_TYPE] = sdk::TYPE_LOG_PROTOBUF;
    std::string reqBody;
    heartBeatReq.SerializeToString(&reqBody);
    sdk::HttpMessage httpResponse;

    sdk::CurlClient client;
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult> emptyResult;
    try {
        client.Send(sdk::HTTP_POST,
                    configServerAddress.host,
                    configServerAddress.port,
                    operation,
                    "",
                    httpHeader,
                    reqBody,
                    INT32_FLAG(sls_client_send_timeout),
                    httpResponse,
                    "",
                    false
        );
        configserver::proto::HeartBeatResponse heartBeatResp;
        heartBeatResp.ParseFromString(httpResponse.content);

        if (0 != strcmp(heartBeatResp.request_id().c_str(), requestID.c_str())) return emptyResult;

        LOG_DEBUG(sLogger, ("SendHeartBeat","success")("reqBody", reqBody)
                  ("requestId", heartBeatResp.request_id())("statusCode", heartBeatResp.code()));

        return heartBeatResp.pipeline_check_results();
    } catch (const sdk::LOGException& e) {
        LOG_ERROR(sLogger, ("SendHeartBeat", "fail")("reqBody", reqBody)("errCode", e.GetErrorCode())("errMsg", e.GetMessage()));
        return emptyResult;
    }
}

google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail> ConfigManager::FetchPipelineConfig(
    AppConfig::ConfigServerAddress configServerAddress,
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult> requestConfigs
) {
    configserver::proto::FetchPipelineConfigRequest fetchConfigReq;
    string requestID = sdk::Base64Enconde(GetInstanceId().append("_").append(to_string(time(NULL)))); 
    fetchConfigReq.set_request_id(requestID);
    fetchConfigReq.set_agent_id(GetInstanceId());

    google::protobuf::RepeatedPtrField<configserver::proto::ConfigInfo> configInfos;
    for (int i = 0; i < requestConfigs.size(); i++) {
        if (requestConfigs[i].check_status() != configserver::proto::DELETED) {
            configserver::proto::ConfigInfo info;
            info.set_type(configserver::proto::PIPELINE_CONFIG);
            info.set_name(requestConfigs[i].name());
            info.set_version(requestConfigs[i].new_version());
            info.set_context(requestConfigs[i].context());
            configInfos.AddAllocated(&info);
        }
    }
    fetchConfigReq.mutable_req_configs()->CopyFrom(configInfos);

    string operation = sdk::CONFIGSERVERAGENT;
    operation.append("/").append("GetConfigList");
    map<string, string> httpHeader;
    httpHeader[sdk::CONTENT_TYPE] = sdk::TYPE_LOG_PROTOBUF;
    string reqBody;
    fetchConfigReq.SerializeToString(&reqBody);
    sdk::HttpMessage httpResponse;

    sdk::CurlClient client;
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail> emptyResult;
    try {
        client.Send(sdk::HTTP_POST,
                    configServerAddress.host,
                    configServerAddress.port,
                    operation,
                    "",
                    httpHeader,
                    reqBody,
                    INT32_FLAG(sls_client_send_timeout),
                    httpResponse,
                    "",
                    false
        );

        configserver::proto::FetchPipelineConfigResponse fetchConfigResp;
        fetchConfigResp.ParseFromString(httpResponse.content);

        if (0 != strcmp(fetchConfigResp.request_id().c_str(), requestID.c_str())) return emptyResult;

        LOG_DEBUG(sLogger, ("GetConfigUpdateInfos","success")("reqBody", reqBody)
                  ("requestId", fetchConfigResp.request_id())("statusCode", fetchConfigResp.code()));
        
        return fetchConfigResp.config_details();
    } catch (const sdk::LOGException& e) {
        LOG_ERROR(sLogger, ("GetConfigUpdateInfos", "fail")("reqBody", reqBody)("errCode", e.GetErrorCode())("errMsg", e.GetMessage()));
        return emptyResult;
    }
}

void ConfigManager::UpdateRemoteConfig(
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult> checkResults,
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail> configDetails
) {
    static string serverConfigDirPath = AppConfig::GetInstance()->GetLocalUserYamlConfigDirPath() + "remote_config" + PATH_SEPARATOR;

    string configName, oldConfigPath, newConfigPath;
    ofstream newConfig;
    configserver::proto::ConfigDetail configDetail;

    for (int i = 0; i < checkResults.size(); i++) {
        configName = checkResults[i].name();

        if (configserver::proto::NEW != checkResults[i].check_status()) {
            oldConfigPath = serverConfigDirPath + configName + "@" + to_string(checkResults[i].old_version()) + ".yaml";
        }
        if (configserver::proto::DELETED != checkResults[i].check_status()) {
            for (int j = 0; j < configDetails.size(); j++) 
                if (configDetails[j].name() == configName) {
                    configDetail = configDetails[j];
                    break;
                }
        }
        newConfigPath = serverConfigDirPath + configName + "@" + to_string(checkResults[i].new_version()) + ".yaml";

        switch (checkResults[i].check_status()) {
            case configserver::proto::DELETED: 
                remove(oldConfigPath.c_str());
                break;
            case configserver::proto::MODIFIED:
                remove(oldConfigPath.c_str());
                newConfig.open(newConfigPath.c_str(),ios::out);
                newConfig << configDetail.detail();
                newConfig.close();
                break;
            case configserver::proto::NEW: 
                newConfig.open(newConfigPath.c_str(),ios::out);
                newConfig << configDetail.detail();
                newConfig.close();
                break;
            default:
                break;
        }
    }
}

} // namespace logtail
