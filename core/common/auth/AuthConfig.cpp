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

#include "common/auth/AuthConfig.h"

#include <string>

#include "common/ParamExtractor.h"

namespace logtail {

bool AuthConfig::Load(const Json::Value& auth, std::string& errorMsg) {
    errorMsg.clear();
    if (!auth.isObject()) {
        // If Authentication is not an object, ignore gracefully (let caller decide).
        return true;
    }

    if (auth.isMember("TLS") && auth["TLS"].isObject()) {
        const Json::Value& tls = auth["TLS"];
        if (!GetOptionalBoolParam(tls, "Enabled", TlsEnabled, errorMsg)) {
            return false;
        }
        if (TlsEnabled) {
            // optional paths/password
            GetOptionalStringParam(tls, "CAFile", TlsCaFile, errorMsg);
            GetOptionalStringParam(tls, "CertFile", TlsCertFile, errorMsg);
            GetOptionalStringParam(tls, "KeyFile", TlsKeyFile, errorMsg);
            GetOptionalStringParam(tls, "KeyPassword", TlsKeyPassword, errorMsg);
        }
    }

    return true;
}

bool AuthConfig::Validate(std::string& errorMsg) const {
    errorMsg.clear();

    if (TlsEnabled) {
        const bool hasCert = !TlsCertFile.empty();
        const bool hasKey = !TlsKeyFile.empty();
        if (hasCert != hasKey) {
            errorMsg = "Authentication.TLS: CertFile and KeyFile must be set together";
            return false;
        }
    }

    return true;
}

} // namespace logtail
