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

// A general authentication config holder.
// Currently supports TLS and SASL(PLAIN/SCRAM). Kerberos parameters
// are temporarily kept in KafkaConfig for backward compatibility.
class AuthConfig {
public:
    // TLS/SSL
    bool TlsEnabled = false;
    std::string TlsCaFile;
    std::string TlsCertFile;
    std::string TlsKeyFile;
    std::string TlsKeyPassword;

    // SASL (PLAIN/SCRAM)
    std::string SaslMechanism;
    std::string SaslUsername;
    std::string SaslPassword;

    // Kerberos (GSSAPI)
    bool KerberosEnabled = false;
    std::string KerberosMechanisms = "GSSAPI";
    std::string KerberosServiceName = "kafka";
    std::string KerberosPrincipal;
    std::string KerberosKeytab;
    std::string KerberosKinitCmd;

    // Load authentication (TLS & SASL) from a JSON object.
    // The input should be the value of config["Authentication"].
    bool Load(const Json::Value& auth, std::string& errorMsg);
    // Validate the loaded config. TLS cert/key must be paired when provided.
    // For SASL, username/password are required when mechanism is set.
    bool Validate(std::string& errorMsg) const;
};

} // namespace logtail
