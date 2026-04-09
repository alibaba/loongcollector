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

#include <string>
#include <vector>

#include "json/value.h"

namespace grpc {
class Service;
}

namespace logtail {

class BaseService {
public:
    BaseService(const std::string& address) : mAddress(address) {}
    virtual ~BaseService() = default;

    virtual bool Update(std::string configName, const Json::Value& config) = 0;
    virtual bool Remove(std::string configName, const Json::Value& config) = 0;
    [[nodiscard]] virtual const std::string& Name() const = 0;

    // Returns all gRPC services for registration with ServerBuilder.
    // Override in derived classes to return a list of gRPC Service objects.
    virtual std::vector<::grpc::Service*> GetGrpcServices() { return {}; }

protected:
    std::string mAddress;
};

}; // namespace logtail
