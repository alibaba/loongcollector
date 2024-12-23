// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SendResult.h"

#include "plugin/flusher/sls/SLSConstant.h"

namespace logtail {

SendResult ConvertErrorCode(const std::string& errorCode) {
    if (errorCode == LOGE_REQUEST_ERROR || errorCode == LOGE_CLIENT_OPERATION_TIMEOUT
        || errorCode == LOGE_REQUEST_TIMEOUT) {
        return SEND_NETWORK_ERROR;
    } else if (errorCode == LOGE_SERVER_BUSY || errorCode == LOGE_INTERNAL_SERVER_ERROR) {
        return SEND_SERVER_ERROR;
    } else if (errorCode == LOGE_WRITE_QUOTA_EXCEED || errorCode == LOGE_SHARD_WRITE_QUOTA_EXCEED) {
        return SEND_QUOTA_EXCEED;
    } else if (errorCode == LOGE_UNAUTHORIZED) {
        return SEND_UNAUTHORIZED;
    } else if (errorCode == LOGE_INVALID_SEQUENCE_ID) {
        return SEND_INVALID_SEQUENCE_ID;
    } else if (errorCode == LOGE_PARAMETER_INVALID) {
        return SEND_PARAMETER_INVALID;
    } else {
        return SEND_DISCARD_ERROR;
    }
}

} // namespace logtail
