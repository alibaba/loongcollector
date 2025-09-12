/*
 * Copyright 2025 iLogtail Authors
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

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "collection_pipeline/queue/QueueKey.h"
#include "forward/BaseService.h"
#include "protobuf/forward/loongsuite.grpc.pb.h"

namespace logtail {

struct ForwardConfig {
    std::string configName;
    QueueKey queueKey;
    size_t inputIndex;
};

class RetryTimeController {
public:
    RetryTimeController() = default;
    ~RetryTimeController() = default;
    RetryTimeController(const RetryTimeController&) = delete;
    RetryTimeController& operator=(const RetryTimeController&) = delete;
    RetryTimeController(RetryTimeController&&) = delete;
    RetryTimeController& operator=(RetryTimeController&&) = delete;

    void InitRetryTimes(const std::string& configName, int32_t retryTimes) {
        std::unique_lock<std::shared_mutex> lock(mRetryTimesMutex);
        mRetryTimes[configName] = retryTimes;
    }

    int32_t GetRetryTimes(const std::string& configName) const {
        std::shared_lock<std::shared_mutex> lock(mRetryTimesMutex);
        auto it = mRetryTimes.find(configName);
        if (it != mRetryTimes.end()) {
            return it->second;
        }
        return 0;
    }

    void UpRetryTimes(const std::string& configName);

    void DownRetryTimes(const std::string& configName) {
        std::unique_lock<std::shared_mutex> lock(mRetryTimesMutex);
        mRetryTimes[configName] = std::max(mRetryTimes[configName] >> 1, 1);
    }

    void ClearRetryTimes(const std::string& configName) {
        std::unique_lock<std::shared_mutex> lock(mRetryTimesMutex);
        mRetryTimes.erase(configName);
    }

private:
    int32_t mMaxRetryTimes = 0;

    std::unordered_map<std::string, int32_t> mRetryTimes;
    mutable std::shared_mutex mRetryTimesMutex;
};

class LoongSuiteForwardServiceImpl : public BaseService, public LoongSuiteForwardService::CallbackService {
public:
    LoongSuiteForwardServiceImpl() = default;
    ~LoongSuiteForwardServiceImpl() override = default;

    bool Update(std::string configName, const Json::Value& config) override;
    bool Remove(std::string configName, const Json::Value& config) override;
    [[nodiscard]] const std::string& Name() const override { return sName; };

    grpc::ServerUnaryReactor* Forward(grpc::CallbackServerContext* context,
                                      const LoongSuiteForwardRequest* request,
                                      LoongSuiteForwardResponse* response) override;

private:
    static const std::string sName;


    // matchValue -> ForwardConfig
    std::unordered_map<std::string, std::shared_ptr<ForwardConfig>> mMatchIndex;
    mutable std::shared_mutex mMatchIndexMutex;

    RetryTimeController mRetryTimeController;

    bool AddToIndex(std::string& configName, ForwardConfig&& config, std::string& errorMsg);
    bool FindMatchingConfig(grpc::CallbackServerContext* context, std::shared_ptr<ForwardConfig>& config) const;
    void ProcessForwardRequest(const LoongSuiteForwardRequest* request,
                               std::shared_ptr<ForwardConfig> config,
                               int32_t retryTimes,
                               grpc::Status& status);
#ifdef APSARA_UNIT_TEST_MAIN
    friend class GrpcInputManagerUnittest;
    friend class LoongSuiteForwardServiceUnittest;
#endif
};

} // namespace logtail
