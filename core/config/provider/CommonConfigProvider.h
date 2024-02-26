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
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/provider/ConfigProvider.h"
#include "config_server_pb/agent.pb.h"

namespace logtail {

class CommonConfigProvider : public ConfigProvider {
public:
    CommonConfigProvider(const CommonConfigProvider&) = delete;
    CommonConfigProvider& operator=(const CommonConfigProvider&) = delete;

    static CommonConfigProvider* GetInstance() {
        static CommonConfigProvider instance;
        return &instance;
    }

    void Init(const std::string& dir) override;
    void Stop() override;

private:
    struct ConfigServerAddress {
        ConfigServerAddress() = default;
        ConfigServerAddress(const std::string& config_server_host, const std::int32_t& config_server_port)
            : host(config_server_host), port(config_server_port) {}

        std::string host;
        std::int32_t port;
    };

    CommonConfigProvider() = default;
    ~CommonConfigProvider() = default;

    ConfigServerAddress GetOneConfigServerAddress(bool changeConfigServer);
    const std::vector<std::string>& GetConfigServerTags() const { return mConfigServerTags; }

    void CheckUpdateThread();
    void GetConfigUpdate();
    bool GetConfigServerAvailable() { return mConfigServerAvailable; }
    void StopUsingConfigServer() { mConfigServerAvailable = false; }
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult>
    SendHeartbeat(const ConfigServerAddress& configServerAddress);
    google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail> FetchPipelineConfig(
        const ConfigServerAddress& configServerAddress,
        const google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult>& requestConfigs);
    void
    UpdateRemoteConfig(const google::protobuf::RepeatedPtrField<configserver::proto::ConfigCheckResult>& checkResults,
                       const google::protobuf::RepeatedPtrField<configserver::proto::ConfigDetail>& configDetails);

    std::vector<ConfigServerAddress> mConfigServerAddresses;
    int mConfigServerAddressId = 0;
    std::vector<std::string> mConfigServerTags;

    std::future<void> mThreadRes;
    mutable std::mutex mThreadRunningMux;
    bool mIsThreadRunning = true;
    mutable std::condition_variable mStopCV;
    std::unordered_map<std::string, int64_t> mConfigNameVersionMap;
    bool mConfigServerAvailable = false;
};

} // namespace logtail
