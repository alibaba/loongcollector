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

#include <string>

namespace logtail {

enum SendResult {
    SEND_OK,
    SEND_NETWORK_ERROR,
    SEND_QUOTA_EXCEED,
    SEND_UNAUTHORIZED,
    SEND_SERVER_ERROR,
    SEND_DISCARD_ERROR,
    SEND_INVALID_SEQUENCE_ID,
    SEND_PARAMETER_INVALID
};

SendResult ConvertErrorCode(const std::string& errorCode);

} // namespace logtail
