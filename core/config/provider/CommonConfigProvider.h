/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/feedbacker/ConfigFeedbackable.h"
#include "config/provider/ConfigProvider.h"
#include "protobuf/config_server/v2/agent.pb.h"

namespace logtail {

struct ConfigInfo {
    std::string name;
    int64_t version;
    ConfigFeedbackStatus status;
    std::string message;
    std::string detail;
};

struct CommandInfo {
    std::string type;
    std::string name;
    ConfigFeedbackStatus status;
    std::string message;
};

class CommonConfigProvider : public ConfigProvider, ConfigFeedbackable {
public:
    std::string sName;
    int64_t mSequenceNum;

    CommonConfigProvider(const CommonConfigProvider&) = delete;
    CommonConfigProvider& operator=(const CommonConfigProvider&) = delete;

    static CommonConfigProvider* GetInstance() {
        static CommonConfigProvider instance;
        return &instance;
    }

    void Init(const std::string& dir) override;
    void Stop() override;

    void FeedbackPipelineConfigStatus(const std::string& name, ConfigFeedbackStatus status) override;
    void FeedbackInstanceConfigStatus(const std::string& name, ConfigFeedbackStatus status) override;
    void
    FeedbackCommandConfigStatus(const std::string& type, const std::string& name, ConfigFeedbackStatus status) override;
    CommonConfigProvider() = default;
    ~CommonConfigProvider() = default;

protected:
    virtual configserver::proto::v2::HeartbeatRequest PrepareHeartbeat();
    virtual bool SendHeartbeat(const configserver::proto::v2::HeartbeatRequest&,
                               configserver::proto::v2::HeartbeatResponse&);

    virtual bool FetchInstanceConfig(::configserver::proto::v2::HeartbeatResponse&,
                                    ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>&);

    virtual bool FetchPipelineConfig(::configserver::proto::v2::HeartbeatResponse&,
                                     ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>&);

    virtual std::string GetInstanceId();
    virtual void FillAttributes(::configserver::proto::v2::AgentAttributes& attributes);
    void UpdateRemotePipelineConfig(
        const google::protobuf::RepeatedPtrField<configserver::proto::v2::ConfigDetail>& configs);
    void
    UpdateRemoteInstanceConfig(const google::protobuf::RepeatedPtrField<configserver::proto::v2::ConfigDetail>& configs);

    virtual bool
    FetchInstanceConfigFromServer(::configserver::proto::v2::HeartbeatResponse&,
                                 ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>&);
    virtual bool
    FetchPipelineConfigFromServer(::configserver::proto::v2::HeartbeatResponse&,
                                  ::google::protobuf::RepeatedPtrField< ::configserver::proto::v2::ConfigDetail>&);

    void CheckUpdateThread();
    void GetConfigUpdate();
    void StopUsingConfigServer() { mConfigServerAvailable = false; }

    int32_t mStartTime;
    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mStopCV;
    bool mConfigServerAvailable = false;

    mutable std::mutex mInstanceInfoMapMux;
    mutable std::mutex mPipelineInfoMapMux;
    mutable std::mutex mCommondInfoMapMux;

    std::unordered_map<std::string, ConfigInfo> mPipelineConfigInfoMap;
    std::unordered_map<std::string, ConfigInfo> mInstanceConfigInfoMap;
    std::unordered_map<std::string, CommandInfo> mCommandInfoMap;

private:
    static std::string configVersion;
    struct ConfigServerAddress {
        ConfigServerAddress() = default;
        ConfigServerAddress(const std::string& config_server_host, const std::int32_t& config_server_port)
            : host(config_server_host), port(config_server_port) {}

        std::string host;
        std::int32_t port;
    };

    ConfigServerAddress GetOneConfigServerAddress(bool changeConfigServer);

    VIRTUAL bool SendHttpRequest(const std::string& operation,
                                 const std::string& reqBody,
                                 const std::string& configType,
                                 const std::string& requestId,
                                 std::string& resp);
    void LoadConfigFile();
    bool DumpConfigFile(const configserver::proto::v2::ConfigDetail& config, const std::filesystem::path& sourceDir);

    std::vector<ConfigServerAddress> mConfigServerAddresses;
    int mConfigServerAddressId = 0;
    std::map<std::string, std::string> mConfigServerTags;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CommonConfigProviderUnittest;
#endif
};

} // namespace logtail
