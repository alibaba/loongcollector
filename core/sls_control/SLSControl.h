/*
 * Copyright 2022 iLogtail Authors
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

#include <cstdint>
#include <string>

#include "sdk/Client.h"

namespace logtail {

class SLSControl {
protected:
    std::string mUserAgent;

    SLSControl() = default;
    virtual ~SLSControl() = default;

    virtual void GenerateUserAgent();
    virtual std::string GetRunningEnvironment();
    bool TryCurlEndpoint(const std::string& endpoint);

public:
    SLSControl(const SLSControl&) = delete;
    SLSControl& operator=(const SLSControl&) = delete;

    static SLSControl* GetInstance();

    void Init();
    virtual void SetSlsSendClientCommonParam(sdk::Client* sendClient);
    virtual bool
    SetSlsSendClientAuth(const std::string aliuid, const bool init, sdk::Client* sendClient, int32_t& lastUpdateTime);
};

} // namespace logtail
