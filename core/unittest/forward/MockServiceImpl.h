// Copyright 2025 iLogtail Authors
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

#pragma once

#include "forward/BaseService.h"
#include "protobuf/forward/loongsuite.grpc.pb.h"

namespace logtail {

class MockServiceImpl : public BaseService, public LoongSuiteForwardService::CallbackService {
public:
    MockServiceImpl() = default;
    ~MockServiceImpl() override = default;

    bool Update(std::string configName, const Json::Value& config) override { return true; }
    bool Remove(std::string configName) override { return true; }
    [[nodiscard]] const std::string& Name() const override { return sName; }

    grpc::ServerUnaryReactor* Forward(grpc::CallbackServerContext* context,
                                      const LoongSuiteForwardRequest* request,
                                      LoongSuiteForwardResponse* response) override {
        auto* reactor = context->DefaultReactor();
        reactor->Finish(grpc::Status::OK);
        return reactor;
    }

private:
    inline static const std::string sName = "MockServiceImpl";
};

} // namespace logtail
