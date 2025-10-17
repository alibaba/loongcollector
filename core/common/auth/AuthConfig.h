/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <json/json.h>

#include <string>

namespace logtail {

// A general authentication config holder with TLS support only for now.
// Future extensions (SASL/Kerberos) can be added here without changing callers.
class AuthConfig {
public:
    // TLS/SSL
    bool TlsEnabled = false;
    std::string TlsCaFile;
    std::string TlsCertFile;
    std::string TlsKeyFile;
    std::string TlsKeyPassword;

    // Load authentication (TLS-only for this phase) from a JSON object.
    // The input should be the value of config["Authentication"].
    bool Load(const Json::Value& auth, std::string& errorMsg);
    // Validate the loaded config. TLS cert/key must be paired when provided.
    bool Validate(std::string& errorMsg) const;
};

} // namespace logtail
