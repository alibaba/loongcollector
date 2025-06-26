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

#include "grpcpp/support/status.h"
#include "logger/Logger.h"

namespace logtail {

const std::string LoongSuiteForwardServiceImpl::sName = "LoongSuiteForwardService";

bool LoongSuiteForwardServiceImpl::Update(std::string configName, const Json::Value& config) {
    // Initialize the service with the provided configuration
    // For now, we just log the initialization
    LOG_INFO(sLogger, ("LoongSuiteForwardServiceImpl updated with config", config.toStyledString()));
    return true; // Return true to indicate successful initialization
}

bool LoongSuiteForwardServiceImpl::Remove(std::string configName) {
    // Handle the removal of the service configuration
    // For now, we just log the removal
    LOG_INFO(sLogger, ("LoongSuiteForwardServiceImpl removed for config", configName));
    return true; // Return true to indicate successful removal
}

grpc::ServerUnaryReactor* LoongSuiteForwardServiceImpl::Forward(grpc::CallbackServerContext* context,
                                                                const LoongSuiteForwardRequest* request,
                                                                LoongSuiteForwardResponse* response) {
    // Implement the logic to handle the request and fill the response
    // For now, we just return nullptr to indicate no further processing
    LOG_INFO(sLogger, ("Received LoongSuiteForwardRequest", "logic not implemented yet"));
    auto* reactor = context->DefaultReactor();
    reactor->Finish(grpc::Status::OK);
    return reactor;
}

} // namespace logtail
