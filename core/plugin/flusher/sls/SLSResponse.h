/*
 * Copyright 2024 iLogtail Authors
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
#include <ctime>
#include <string>

#include "common/http/HttpResponse.h"

namespace logtail {

struct SLSResponse {
    int32_t mStatusCode = 0;
    std::string mRequestId;
    std::string mErrorCode;
    std::string mErrorMsg;

    bool Parse(const HttpResponse& response);
};

SLSResponse ParseHttpResponse(const HttpResponse& response);
bool IsSLSResponse(const HttpResponse& response);
time_t GetServerTime(const HttpResponse& response);

} // namespace logtail
