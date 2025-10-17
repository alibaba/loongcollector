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

#include "forward/loongsuite/LoongSuiteForwardService.h"

#include <grpcpp/support/status.h>

#include <memory>

#include "common/Flags.h"
#include "common/ParamExtractor.h"
#include "grpcpp/support/status.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"
#include "runner/ProcessorRunner.h"


DECLARE_FLAG_INT32(grpc_server_forward_max_retry_times);

const std::string kProtocolMetadataKey = "x-loongsuite-apm-configname";

namespace logtail {

const std::string LoongSuiteForwardServiceImpl::sName = "LoongSuiteForwardService";

void RetryTimeController::UpRetryTimes(const std::string& configName) {
    std::unique_lock<std::shared_mutex> lock(mRetryTimesMutex);
    mRetryTimes[configName] = std::min(mRetryTimes[configName] + 1, INT32_FLAG(grpc_server_forward_max_retry_times));
}

bool LoongSuiteForwardServiceImpl::Update(std::string configName, const Json::Value& config) {
    ForwardConfig forwardConfig;
    forwardConfig.configName = configName;

    std::string errorMsg;
    int32_t queueKey = -1;
    if (!GetMandatoryIntParam(config, "QueueKey", queueKey, errorMsg)) {
        return false;
    }
    forwardConfig.queueKey = static_cast<QueueKey>(queueKey);

    int inputIndex = -1;
    if (!GetMandatoryIntParam(config, "InputIndex", inputIndex, errorMsg)) {
        return false;
    }
    forwardConfig.inputIndex = static_cast<size_t>(inputIndex);
    if (!AddToIndex(configName, std::move(forwardConfig), errorMsg)) {
        LOG_ERROR(sLogger, ("Update LoongSuite forward match rule failed", configName)("error", errorMsg));
        return false;
    }
    mRetryTimeController.InitRetryTimes(configName, INT32_FLAG(grpc_server_forward_max_retry_times));

    LOG_INFO(
        sLogger,
        ("LoongSuiteForwardServiceImpl config updated", configName)("queueKey", queueKey)("inputIndex", inputIndex));
    return true;
}

bool LoongSuiteForwardServiceImpl::Remove(std::string configName, const Json::Value& config) {
    std::string errorMsg;

    std::unique_lock<std::shared_mutex> lock(mMatchIndexMutex);
    auto it = mMatchIndex.find(configName);
    if (it != mMatchIndex.end()) {
        mMatchIndex.erase(it);
        mRetryTimeController.ClearRetryTimes(configName);
        LOG_INFO(sLogger, ("LoongSuiteForwardServiceImpl config removed", configName));
    } else {
        LOG_WARNING(sLogger, ("Config not found for removal", configName));
    }
    return true;
}

grpc::ServerUnaryReactor* LoongSuiteForwardServiceImpl::Forward(grpc::CallbackServerContext* context,
                                                                const LoongSuiteForwardRequest* request,
                                                                LoongSuiteForwardResponse* response) {
    auto* reactor = context->DefaultReactor();
    grpc::Status status(grpc::StatusCode::NOT_FOUND, "No matching config found for forward request");

    std::shared_ptr<ForwardConfig> config;
    if (FindMatchingConfig(context, config)) {
        // config maybe removed, so we need to copy config here
        int32_t retryTimes = mRetryTimeController.GetRetryTimes(config->configName);
        ProcessForwardRequest(request, config, retryTimes, status);

        if (status.ok()) {
            mRetryTimeController.UpRetryTimes(config->configName);
        } else {
            mRetryTimeController.DownRetryTimes(config->configName);
        }
    }

    reactor->Finish(status);
    return reactor;
}

void LoongSuiteForwardServiceImpl::ProcessForwardRequest(const LoongSuiteForwardRequest* request,
                                                         std::shared_ptr<ForwardConfig> config,
                                                         int32_t retryTimes,
                                                         grpc::Status& status) {
    if (!request) {
        status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid request");
        return;
    }

    const std::string& data = request->data();
    if (data.empty()) {
        status = grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Empty data in forward request");
        return;
    }

    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto* event = eventGroup.AddRawEvent();
    event->SetContent(request->data());
    event->SetTimestamp(time(nullptr), 0);

    bool result = ProcessorRunner::GetInstance()->PushQueue(
        config->queueKey, config->inputIndex, std::move(eventGroup), retryTimes);

    if (!result) {
        status = grpc::Status(grpc::StatusCode::UNAVAILABLE, "Queue is full, please retry later");
    } else {
        status = grpc::Status::OK;
    }
}

bool LoongSuiteForwardServiceImpl::AddToIndex(std::string& configName, ForwardConfig&& config, std::string& errorMsg) {
    errorMsg.clear();
    std::unique_lock<std::shared_mutex> lock(mMatchIndexMutex);
    if (!configName.empty()) {
        mMatchIndex[configName] = std::make_shared<ForwardConfig>(std::move(config));
        return true;
    }
    errorMsg = "Empty config name";
    return false;
}


bool LoongSuiteForwardServiceImpl::FindMatchingConfig(grpc::CallbackServerContext* context,
                                                      std::shared_ptr<ForwardConfig>& config) const {
    std::shared_lock<std::shared_mutex> lock(mMatchIndexMutex);
    const auto& metadata = context->client_metadata();

    for (const auto& metadataPair : metadata) {
        if (metadataPair.first != kProtocolMetadataKey) {
            continue;
        }
        std::string value(metadataPair.second.data(), metadataPair.second.size());
        auto it = mMatchIndex.find(value);
        if (it != mMatchIndex.end()) {
            config = it->second;
            return true;
        }
    }

    return false;
}

} // namespace logtail
