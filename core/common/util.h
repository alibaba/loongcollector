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
#include <string>
#include <json/json.h>

//#define LOGTAIL_DEBUG_FLAG

namespace logtail {

// Two built-in addresses for debugging/testing (passed to CheckAddress).
extern const std::string DEBUG_SERVER_FILE;
extern const std::string DEBUG_SERVER_NULL;

// Logtail utility.
// CheckAddress checks if @definedAddress is valid and return a valid address.
// @return a valid HTTP(s) address without the last '/'.
// @defaultAdderss will be returned if @definedAddress only has protocol parts, which
// means @definedAddress is equal to "http://" or "https://".
// If no protocol part in @definedAddress, http protocol will be prepended.
std::string CheckAddress(const std::string& definedAddress, const std::string& defaultAdderss);

// CalculateXXXUUID generates UUID according to DMI or random.
std::string CalculateDmiUUID();
std::string CalculateRandomUUID();

// Endpoint is classified in three kinds by its format:
// 1. -intranet.log.aliyuncs.com: INTRANET.
// 2. .log.aliyuncs.com excluding -share.log.aliyuncs.com: PUBLIC.
// 3. others including -share.log.aliyuncs.com: INNER.
enum class EndpointAddressType { INNER, INTRANET, PUBLIC };

namespace util {

    extern const std::string kConfigPrefix;
    extern const std::string kConfigSuffix;
    extern const std::string kConfigTemplate;
    extern const std::string kLogEndpointSuffix;
    extern const std::string kAccelerationDataEndpoint;
    extern const std::string kHangzhouCorp;

} // namespace util

bool GenerateAPPConfigByConfigPath(const std::string& filePath, Json::Value& confJson);

std::string GenerateLogtailConfigByInstallParam(const std::string& installParam);

} // namespace logtail
