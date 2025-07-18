// Copyright 2023 iLogtail Authors
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

#include "CommonConfigProvider.h"

#include <filesystem>
#include <iostream>
#include <random>

#include "json/json.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "common/UUIDUtil.h"
#include "common/YamlUtil.h"
#include "common/http/Constant.h"
#include "common/http/Curl.h"
#include "common/version.h"
#include "config/CollectionConfig.h"
#include "config/ConfigUtil.h"
#include "config/feedbacker/ConfigFeedbackReceiver.h"
#include "constants/Constants.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"

using namespace std;

DEFINE_FLAG_INT32(heartbeat_interval, "second", 10);

namespace logtail {

const string AGENT = "/Agent";

string CommonConfigProvider::configVersion = "version";

void CommonConfigProvider::Init(const string& dir) {
    sName = "common config provider";

    ConfigProvider::Init(dir);
    LoadConfigFile();

    mStartTime = Application::GetInstance()->GetStartTime();

    mSequenceNum = 0;

    const Json::Value& confJson = AppConfig::GetInstance()->GetConfig();

    // configserver path
    /*** demo
     * {
     *     "config_server_list" : [
     *         {
     *             "cluster" : "community",
     *             "endpoint_list" : ["test.config.com:80"]
     *         }
     * }
     */
    if (confJson.isObject() && confJson.isMember("config_server_list") && confJson["config_server_list"].isArray()
        && confJson["config_server_list"].size() > 0 && confJson["config_server_list"][0].isObject()
        && confJson["config_server_list"][0].isMember("endpoint_list")
        && confJson["config_server_list"][0]["endpoint_list"].isArray()) {
        for (Json::Value::ArrayIndex i = 0; i < confJson["config_server_list"][0]["endpoint_list"].size(); ++i) {
            if (!confJson["config_server_list"][0]["endpoint_list"][i].isString()) {
                continue;
            }
            vector<string> configServerAddress
                = SplitString(TrimString(confJson["config_server_list"][0]["endpoint_list"][i].asString()), ":");

            if (configServerAddress.size() != 2) {
                LOG_WARNING(
                    sLogger,
                    ("configserver_address", "format error")(
                        "wrong address", TrimString(confJson["config_server_list"][0]["endpoint_list"][i].asString())));
                continue;
            }

            string host = configServerAddress[0];
            int32_t port = atoi(configServerAddress[1].c_str());

            if (port < 1 || port > 65535) {
                LOG_WARNING(sLogger, ("configserver_address", "illegal port")("port", port));
                continue;
            }
            mConfigServerAddresses.push_back(ConfigServerAddress(host, port));
        }

        mConfigServerAvailable = true;
        LOG_INFO(sLogger,
                 ("configserver_address", confJson["config_server_list"][0]["endpoint_list"].toStyledString()));
    }

    // tags for configserver
    if (confJson.isMember("ilogtail_tags") && confJson["ilogtail_tags"].isObject()) {
        Json::Value::Members members = confJson["ilogtail_tags"].getMemberNames();
        for (Json::Value::Members::iterator it = members.begin(); it != members.end(); it++) {
            mConfigServerTags[*it] = confJson["ilogtail_tags"][*it].asString();
        }
        LOG_INFO(sLogger, ("ilogtail_configserver_tags", confJson["ilogtail_tags"].toStyledString()));
    }

    GetConfigUpdate();

    mThreadRes = async(launch::async, &CommonConfigProvider::CheckUpdateThread, this);
}

void CommonConfigProvider::Stop() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        mIsThreadRunning = false;
    }
    mStopCV.notify_one();
    if (!mThreadRes.valid()) {
        return;
    }
    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, (sName, "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, (sName, "forced to stopped"));
    }
}

void CommonConfigProvider::LoadConfigFile() {
    error_code ec;
    for (auto const& entry : filesystem::directory_iterator(mContinuousPipelineConfigDir, ec)) {
        Json::Value detail;
        if (LoadConfigDetailFromFile(entry, detail)) {
            ConfigInfo info;
            info.name = entry.path().stem().string();
            if (detail.isMember(CommonConfigProvider::configVersion)
                && detail[CommonConfigProvider::configVersion].isInt64()) {
                info.version = detail[CommonConfigProvider::configVersion].asInt64();
            }
            info.status = ConfigFeedbackStatus::APPLYING;
            info.detail = detail.toStyledString();
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                mContinuousPipelineConfigInfoMap[info.name] = info;
            }
            ConfigFeedbackReceiver::GetInstance().RegisterContinuousPipelineConfig(info.name, this);
        }
    }
    for (auto const& entry : filesystem::directory_iterator(mInstanceSourceDir, ec)) {
        Json::Value detail;
        if (LoadConfigDetailFromFile(entry, detail)) {
            ConfigInfo info;
            info.name = entry.path().stem().string();
            if (detail.isMember(CommonConfigProvider::configVersion)
                && detail[CommonConfigProvider::configVersion].isInt64()) {
                info.version = detail[CommonConfigProvider::configVersion].asInt64();
            }
            info.status = ConfigFeedbackStatus::APPLYING;
            info.detail = detail.toStyledString();
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                mInstanceConfigInfoMap[info.name] = info;
            }
            ConfigFeedbackReceiver::GetInstance().RegisterInstanceConfig(info.name, this);
        }
    }
}

void CommonConfigProvider::CheckUpdateThread() {
    LOG_INFO(sLogger, (sName, "started"));
    usleep((rand() % 10) * 100 * 1000);
    int32_t lastCheckTime = time(NULL);
    unique_lock<mutex> lock(mThreadRunningMux);
    while (mIsThreadRunning) {
        int32_t curTime = time(NULL);
        if (curTime - lastCheckTime >= INT32_FLAG(heartbeat_interval)) {
            GetConfigUpdate();
            lastCheckTime = curTime;
        }
        if (mStopCV.wait_for(lock, chrono::seconds(3), [this]() { return !mIsThreadRunning; })) {
            break;
        }
    }
}

CommonConfigProvider::ConfigServerAddress CommonConfigProvider::GetOneConfigServerAddress(bool changeConfigServer) {
    if (0 == mConfigServerAddresses.size()) {
        return ConfigServerAddress("", -1); // No address available
    }

    // Return a random address
    if (changeConfigServer) {
        random_device rd;
        int tmpId = rd() % mConfigServerAddresses.size();
        while (mConfigServerAddresses.size() > 1 && tmpId == mConfigServerAddressId) {
            tmpId = rd() % mConfigServerAddresses.size();
        }
        mConfigServerAddressId = tmpId;
    }
    return ConfigServerAddress(mConfigServerAddresses[mConfigServerAddressId].host,
                               mConfigServerAddresses[mConfigServerAddressId].port);
}

string CommonConfigProvider::GetInstanceId() {
    return Application::GetInstance()->GetInstanceId();
}

void CommonConfigProvider::FillAttributes(configserver::proto::v2::AgentAttributes& attributes) {
    attributes.set_hostname(LoongCollectorMonitor::mHostname);
    attributes.set_ip(LoongCollectorMonitor::mIpAddr);
    attributes.set_version(ILOGTAIL_VERSION);
    google::protobuf::Map<string, string>* extras = attributes.mutable_extras();
    extras->insert({"osDetail", LoongCollectorMonitor::mOsDetail});
}

void addConfigInfoToRequest(const std::pair<const string, logtail::ConfigInfo>& configInfo,
                            configserver::proto::v2::ConfigInfo* reqConfig) {
    reqConfig->set_name(configInfo.second.name);
    reqConfig->set_message(configInfo.second.message);
    reqConfig->set_version(configInfo.second.version);
    switch (configInfo.second.status) {
        case ConfigFeedbackStatus::UNSET:
            reqConfig->set_status(configserver::proto::v2::ConfigStatus::UNSET);
            break;
        case ConfigFeedbackStatus::APPLYING:
            reqConfig->set_status(configserver::proto::v2::ConfigStatus::APPLYING);
            break;
        case ConfigFeedbackStatus::APPLIED:
            reqConfig->set_status(configserver::proto::v2::ConfigStatus::APPLIED);
            break;
        case ConfigFeedbackStatus::FAILED:
            reqConfig->set_status(configserver::proto::v2::ConfigStatus::FAILED);
            break;
        case ConfigFeedbackStatus::DELETED:
            reqConfig->set_version(-1);
            break;
    }
}

void CommonConfigProvider::GetConfigUpdate() {
    if (!mConfigServerAvailable) {
        return;
    }
    auto heartbeatRequest = PrepareHeartbeat();
    configserver::proto::v2::HeartbeatResponse heartbeatResponse;
    if (!SendHeartbeat(heartbeatRequest, heartbeatResponse)) {
        return;
    }
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail> pipelineConfig;
    if (FetchPipelineConfig(heartbeatResponse, pipelineConfig) && !pipelineConfig.empty()) {
        LOG_DEBUG(sLogger, ("fetch pipelineConfig, config file number", pipelineConfig.size()));
        UpdateRemotePipelineConfig(pipelineConfig);
    }
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail> instanceConfig;
    if (FetchInstanceConfig(heartbeatResponse, instanceConfig) && !instanceConfig.empty()) {
        LOG_DEBUG(sLogger, ("fetch instanceConfig config, config file number", instanceConfig.size()));
        UpdateRemoteInstanceConfig(instanceConfig);
    }
    ++mSequenceNum;
}

configserver::proto::v2::HeartbeatRequest CommonConfigProvider::PrepareHeartbeat() {
    configserver::proto::v2::HeartbeatRequest heartbeatReq;
    string requestID = CalculateRandomUUID();
    heartbeatReq.set_request_id(requestID);
    heartbeatReq.set_sequence_num(mSequenceNum);
    heartbeatReq.set_capabilities(configserver::proto::v2::AcceptsInstanceConfig
                                  | configserver::proto::v2::AcceptsContinuousPipelineConfig);
    heartbeatReq.set_instance_id(GetInstanceId());
    heartbeatReq.set_agent_type("LoongCollector");
    FillAttributes(*heartbeatReq.mutable_attributes());

    for (auto tag : mConfigServerTags) {
        configserver::proto::v2::AgentGroupTag* agentGroupTag = heartbeatReq.add_tags();
        agentGroupTag->set_name(tag.first);
        agentGroupTag->set_value(tag.second);
    }
    heartbeatReq.set_running_status("running");
    heartbeatReq.set_startup_time(mStartTime);

    {
        lock_guard<mutex> lockInfoMap(mInfoMapMux);
        for (const auto& configInfo : mContinuousPipelineConfigInfoMap) {
            addConfigInfoToRequest(configInfo, heartbeatReq.add_continuous_pipeline_configs());
        }

        for (const auto& configInfo : mInstanceConfigInfoMap) {
            addConfigInfoToRequest(configInfo, heartbeatReq.add_instance_configs());
        }

        for (const auto& configInfo : mOnetimePipelineConfigInfoMap) {
            addConfigInfoToRequest(configInfo, heartbeatReq.add_onetime_pipeline_configs());
        }
    }

    return heartbeatReq;
}

bool CommonConfigProvider::SendHeartbeat(const configserver::proto::v2::HeartbeatRequest& heartbeatReq,
                                         configserver::proto::v2::HeartbeatResponse& heartbeatResponse) {
    string operation = AGENT;
    operation.append("/").append("Heartbeat");
    string reqBody;
    heartbeatReq.SerializeToString(&reqBody);
    std::string heartbeatResp;
    if (SendHttpRequest(operation, reqBody, "SendHeartbeat", heartbeatReq.request_id(), heartbeatResp)) {
        configserver::proto::v2::HeartbeatResponse heartbeatRespPb;
        heartbeatRespPb.ParseFromString(heartbeatResp);
        heartbeatResponse.Swap(&heartbeatRespPb);
        return true;
    } else {
        return false;
    }
}

bool CommonConfigProvider::SendHttpRequest(const string& operation,
                                           const string& reqBody,
                                           const string& configType,
                                           const std::string& requestId,
                                           std::string& resp) {
    // LCOV_EXCL_START
    ConfigServerAddress configServerAddress = GetOneConfigServerAddress(false);
    map<string, string> httpHeader;
    httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;

    HttpResponse httpResponse;
    if (!logtail::SendHttpRequest(make_unique<HttpRequest>(HTTP_POST,
                                                           false,
                                                           configServerAddress.host,
                                                           configServerAddress.port,
                                                           operation,
                                                           "",
                                                           httpHeader,
                                                           reqBody),
                                  httpResponse)) {
        LOG_WARNING(sLogger,
                    (configType, "fail")("reqBody",
                                         reqBody)("host", configServerAddress.host)("port", configServerAddress.port));
        return false;
    }
    resp = *httpResponse.GetBody<string>();
    return true;
    // LCOV_EXCL_STOP
}

bool CommonConfigProvider::FetchPipelineConfig(
    configserver::proto::v2::HeartbeatResponse& heartbeatResponse,
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>& result) {
    if (heartbeatResponse.flags() & ::configserver::proto::v2::FetchContinuousPipelineConfigDetail) {
        return FetchPipelineConfigFromServer(heartbeatResponse, result);
    } else {
        result.Swap(heartbeatResponse.mutable_continuous_pipeline_config_updates());
        return true;
    }
}

bool CommonConfigProvider::FetchInstanceConfig(
    configserver::proto::v2::HeartbeatResponse& heartbeatResponse,
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>& result) {
    if (heartbeatResponse.flags() & ::configserver::proto::v2::FetchContinuousPipelineConfigDetail) {
        return FetchInstanceConfigFromServer(heartbeatResponse, result);
    } else {
        result.Swap(heartbeatResponse.mutable_instance_config_updates());
        return true;
    }
}

bool CommonConfigProvider::DumpConfigFile(const configserver::proto::v2::ConfigDetail& config,
                                          const filesystem::path& sourceDir) {
    filesystem::path filePath = sourceDir / (config.name() + ".json");
    filesystem::path tmpFilePath = sourceDir / (config.name() + ".json.new");
    Json::Value detail;
    std::string errorMsg;
    if (!ParseConfigDetail(config.detail(), ".json", detail, errorMsg)) {
        LOG_WARNING(sLogger, ("failed to parse config detail", config.detail()));
        return false;
    }
    detail[CommonConfigProvider::configVersion] = config.version();
    string configDetail = detail.toStyledString();
    {
        ofstream fout(tmpFilePath);
        if (!fout) {
            LOG_WARNING(sLogger, ("failed to open config file", filePath.string()));
            return false;
        }
        fout << configDetail;
    }

    error_code ec;
    filesystem::rename(tmpFilePath, filePath, ec);
    if (ec) {
        LOG_WARNING(
            sLogger,
            ("failed to dump config file", filePath.string())("error code", ec.value())("error msg", ec.message()));
        filesystem::remove(tmpFilePath, ec);
    }
    return true;
}

void CommonConfigProvider::UpdateRemotePipelineConfig(
    const google::protobuf::RepeatedPtrField<configserver::proto::v2::ConfigDetail>& configs) {
    error_code ec;
    const std::filesystem::path& sourceDir = mContinuousPipelineConfigDir;
    filesystem::create_directories(sourceDir, ec);
    if (ec) {
        StopUsingConfigServer();
        LOG_ERROR(sLogger,
                  ("failed to create dir for common configs", "stop receiving config from common config server")(
                      "dir", sourceDir.string())("error code", ec.value())("error msg", ec.message()));
        return;
    }
    // 保证每次往磁盘上dump文件的时候，config watcher不会读到一半的内容，相当于是个目录锁
    lock_guard<mutex> lock(mContinuousPipelineMux);
    for (const auto& config : configs) {
        filesystem::path filePath = sourceDir / (config.name() + ".json");
        if (config.version() == -1) {
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                mContinuousPipelineConfigInfoMap.erase(config.name());
            }
            filesystem::remove(filePath, ec);
            ConfigFeedbackReceiver::GetInstance().UnregisterContinuousPipelineConfig(config.name());
        } else {
            if (!DumpConfigFile(config, sourceDir)) {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                ConfigInfo info;
                info.name = config.name();
                info.version = config.version();
                info.status = ConfigFeedbackStatus::FAILED;
                info.detail = config.detail();
                mContinuousPipelineConfigInfoMap[config.name()] = std::move(info);
                continue;
            }
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                ConfigInfo info;
                info.name = config.name();
                info.version = config.version();
                info.status = ConfigFeedbackStatus::APPLYING;
                info.detail = config.detail();
                mContinuousPipelineConfigInfoMap[config.name()] = std::move(info);
            }
            ConfigFeedbackReceiver::GetInstance().RegisterContinuousPipelineConfig(config.name(), this);
        }
    }
}

void CommonConfigProvider::UpdateRemoteInstanceConfig(
    const google::protobuf::RepeatedPtrField<configserver::proto::v2::ConfigDetail>& configs) {
    error_code ec;
    const std::filesystem::path& sourceDir = mInstanceSourceDir;
    filesystem::create_directories(sourceDir, ec);
    if (ec) {
        StopUsingConfigServer();
        LOG_ERROR(sLogger,
                  ("failed to create dir for common configs", "stop receiving config from common config server")(
                      "dir", sourceDir.string())("error code", ec.value())("error msg", ec.message()));
        return;
    }
    // 保证每次往磁盘上dump文件的时候，config watcher不会读到一半的内容，相当于是个目录锁
    lock_guard<mutex> lock(mInstanceMux);
    for (const auto& config : configs) {
        filesystem::path filePath = sourceDir / (config.name() + ".json");
        if (config.version() == -1) {
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                mInstanceConfigInfoMap.erase(config.name());
            }
            filesystem::remove(filePath, ec);
            ConfigFeedbackReceiver::GetInstance().UnregisterInstanceConfig(config.name());
        } else {
            if (!DumpConfigFile(config, sourceDir)) {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                ConfigInfo info;
                info.name = config.name();
                info.version = config.version();
                info.status = ConfigFeedbackStatus::FAILED;
                info.detail = config.detail();
                mInstanceConfigInfoMap[config.name()] = std::move(info);
                continue;
            }
            {
                lock_guard<mutex> lockInfoMap(mInfoMapMux);
                ConfigInfo info;
                info.name = config.name();
                info.version = config.version();
                info.detail = config.detail();
                info.status = ConfigFeedbackStatus::APPLYING;
                mInstanceConfigInfoMap[config.name()] = std::move(info);
            }
            ConfigFeedbackReceiver::GetInstance().RegisterInstanceConfig(config.name(), this);
        }
    }
}

bool CommonConfigProvider::FetchInstanceConfigFromServer(
    ::configserver::proto::v2::HeartbeatResponse& heartbeatResponse,
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>& res) {
    configserver::proto::v2::FetchConfigRequest fetchConfigRequest;
    string requestID = CalculateRandomUUID();
    fetchConfigRequest.set_request_id(requestID);
    fetchConfigRequest.set_instance_id(GetInstanceId());
    for (const auto& config : heartbeatResponse.instance_config_updates()) {
        auto reqConfig = fetchConfigRequest.add_instance_configs();
        reqConfig->set_name(config.name());
        reqConfig->set_version(config.version());
    }
    string operation = AGENT;
    operation.append("/FetchInstanceConfig");
    string reqBody;
    fetchConfigRequest.SerializeToString(&reqBody);
    string fetchConfigResponse;
    if (SendHttpRequest(
            operation, reqBody, "FetchInstanceConfig", fetchConfigRequest.request_id(), fetchConfigResponse)) {
        configserver::proto::v2::FetchConfigResponse fetchConfigResponsePb;
        fetchConfigResponsePb.ParseFromString(fetchConfigResponse);
        res.Swap(fetchConfigResponsePb.mutable_instance_config_updates());
        return true;
    }
    return false;
}

bool CommonConfigProvider::FetchPipelineConfigFromServer(
    ::configserver::proto::v2::HeartbeatResponse& heartbeatResponse,
    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>& res) {
    configserver::proto::v2::FetchConfigRequest fetchConfigRequest;
    string requestID = CalculateRandomUUID();
    fetchConfigRequest.set_request_id(requestID);
    fetchConfigRequest.set_instance_id(GetInstanceId());
    for (const auto& config : heartbeatResponse.continuous_pipeline_config_updates()) {
        auto reqConfig = fetchConfigRequest.add_continuous_pipeline_configs();
        reqConfig->set_name(config.name());
        reqConfig->set_version(config.version());
    }
    string operation = AGENT;
    operation.append("/FetchPipelineConfig");
    string reqBody;
    fetchConfigRequest.SerializeToString(&reqBody);
    string fetchConfigResponse;
    if (SendHttpRequest(
            operation, reqBody, "FetchPipelineConfig", fetchConfigRequest.request_id(), fetchConfigResponse)) {
        configserver::proto::v2::FetchConfigResponse fetchConfigResponsePb;
        fetchConfigResponsePb.ParseFromString(fetchConfigResponse);
        res.Swap(fetchConfigResponsePb.mutable_continuous_pipeline_config_updates());
        return true;
    }
    return false;
}

void CommonConfigProvider::FeedbackContinuousPipelineConfigStatus(const std::string& name,
                                                                  ConfigFeedbackStatus status) {
    lock_guard<mutex> lockInfoMap(mInfoMapMux);
    auto info = mContinuousPipelineConfigInfoMap.find(name);
    if (info != mContinuousPipelineConfigInfoMap.end()) {
        info->second.status = status;
    }
    LOG_DEBUG(sLogger,
              ("CommonConfigProvider", "FeedbackContinuousPipelineConfigStatus")("name", name)("status",
                                                                                               ToStringView(status)));
}
void CommonConfigProvider::FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status) {
    lock_guard<mutex> lockInfoMap(mInfoMapMux);
    auto info = mInstanceConfigInfoMap.find(name);
    if (info != mInstanceConfigInfoMap.end()) {
        info->second.status = status;
    }
    LOG_DEBUG(sLogger,
              ("CommonConfigProvider", "FeedbackInstanceConfigStatus")("name", name)("status", ToStringView(status)));
}
void CommonConfigProvider::FeedbackOnetimePipelineConfigStatus(const std::string& type,
                                                               const std::string& name,
                                                               ConfigFeedbackStatus status) {
    lock_guard<mutex> lockInfoMap(mInfoMapMux);
    auto info = mOnetimePipelineConfigInfoMap.find(GenerateOnetimePipelineConfigFeedBackKey(type, name));
    if (info != mOnetimePipelineConfigInfoMap.end()) {
        info->second.status = status;
    }
    LOG_DEBUG(sLogger,
              ("CommonConfigProvider",
               "FeedbackOnetimePipelineConfigStatus")("type", type)("name", name)("status", ToStringView(status)));
}

} // namespace logtail
