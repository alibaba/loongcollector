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

#include <map>
#include <string>
#include <vector>

#include "common/ParamExtractor.h"
#include "plugin/flusher/kafka/KafkaUtil.h"

namespace logtail {

struct KafkaConfig {
    std::vector<std::string> Brokers;
    std::string Topic;

    std::string Version = "1.0.0";

    std::string PartitionerType;
    std::vector<std::string> HashKeys;
    std::string Partitioner;

    uint32_t QueueBufferingMaxKbytes = 1048576;
    uint32_t QueueBufferingMaxMessages = 100000;

    uint32_t BulkFlushFrequency = 0;
    uint32_t BulkMaxSize = 2048;
    uint32_t MaxMessageBytes = 1000000;

    std::string Compression;
    int32_t CompressionLevel = -1;

    int32_t RequiredAcks = 1;
    uint32_t Timeout = 30000;
    uint32_t MessageTimeoutMs = 300000;
    uint32_t MaxRetries = 3;
    uint32_t RetryBackoffMs = 100;

    std::map<std::string, std::string> CustomConfig;

    bool EnableTLS = false;
    std::string TLSCaFile;
    std::string TLSCertFile;
    std::string TLSKeyFile;
    std::string TLSKeyPassword;

    bool EnableKerberos = false;
    std::string SaslMechanisms = "GSSAPI";
    std::string KerberosPrincipal;
    std::string KerberosServiceName = "kafka";
    std::string KerberosKeytab;
    std::string KerberosKinitCmd;

    using HeaderEntry = std::pair<std::string, std::string>;
    std::vector<HeaderEntry> Headers;

    bool Load(const Json::Value& config, std::string& errorMsg) {
        if (!GetMandatoryListParam<std::string>(config, "Brokers", Brokers, errorMsg)) {
            return false;
        }

        if (!GetMandatoryStringParam(config, "Topic", Topic, errorMsg)) {
            return false;
        }

        std::string versionStr;
        if (!GetOptionalStringParam(config, "Version", versionStr, errorMsg)) {
            return false;
        }
        if (versionStr.empty()) {
            GetOptionalStringParam(config, "KafkaVersion", versionStr, errorMsg);
        }
        if (!versionStr.empty()) {
            Version = versionStr;
        }

        KafkaUtil::Version parsed;
        if (!KafkaUtil::ParseKafkaVersion(Version, parsed)) {
            errorMsg = "invalid Version format, expected x.y.z[.n]";
            return false;
        }

        GetOptionalUIntParam(config, "BulkFlushFrequency", BulkFlushFrequency, errorMsg);
        GetOptionalUIntParam(config, "BulkMaxSize", BulkMaxSize, errorMsg);
        GetOptionalUIntParam(config, "MaxMessageBytes", MaxMessageBytes, errorMsg);
        GetOptionalIntParam(config, "RequiredAcks", RequiredAcks, errorMsg);
        GetOptionalUIntParam(config, "Timeout", Timeout, errorMsg);
        GetOptionalUIntParam(config, "MessageTimeoutMs", MessageTimeoutMs, errorMsg);
        GetOptionalUIntParam(config, "MaxRetries", MaxRetries, errorMsg);
        GetOptionalUIntParam(config, "RetryBackoffMs", RetryBackoffMs, errorMsg);

        GetOptionalUIntParam(config, "QueueBufferingMaxKbytes", QueueBufferingMaxKbytes, errorMsg);
        GetOptionalUIntParam(config, "QueueBufferingMaxMessages", QueueBufferingMaxMessages, errorMsg);

        GetOptionalStringParam(config, "PartitionerType", PartitionerType, errorMsg);
        GetOptionalListParam<std::string>(config, "HashKeys", HashKeys, errorMsg);

        GetOptionalStringParam(config, "Compression", Compression, errorMsg);
        GetOptionalIntParam(config, "CompressionLevel", CompressionLevel, errorMsg);

        if (config.isMember("Authentication") && config["Authentication"].isObject()) {
            const Json::Value& auth = config["Authentication"];
            if (auth.isMember("TLS") && auth["TLS"].isObject()) {
                const Json::Value& tls = auth["TLS"];
                if (!GetOptionalBoolParam(tls, "Enabled", EnableTLS, errorMsg)) {
                    return false;
                }
                if (EnableTLS) {
                    GetMandatoryStringParam(tls, "CAFile", TLSCaFile, errorMsg);
                    GetOptionalStringParam(tls, "CertFile", TLSCertFile, errorMsg);
                    GetOptionalStringParam(tls, "KeyFile", TLSKeyFile, errorMsg);
                    GetOptionalStringParam(tls, "KeyPassword", TLSKeyPassword, errorMsg);
                }
            }

            if (auth.isMember("Kerberos") && auth["Kerberos"].isObject()) {
                const Json::Value& krb = auth["Kerberos"];
                if (!GetOptionalBoolParam(krb, "Enabled", EnableKerberos, errorMsg)) {
                    return false;
                }
                if (EnableKerberos) {
                    GetOptionalStringParam(krb, "Mechanisms", SaslMechanisms, errorMsg);
                    GetOptionalStringParam(krb, "ServiceName", KerberosServiceName, errorMsg);
                    GetOptionalStringParam(krb, "Principal", KerberosPrincipal, errorMsg);
                    GetOptionalStringParam(krb, "Keytab", KerberosKeytab, errorMsg);
                    GetOptionalStringParam(krb, "KinitCmd", KerberosKinitCmd, errorMsg);
                }
            }
        }

        if (config.isMember("Kafka") && config["Kafka"].isObject()) {
            const Json::Value& kafkaConfig = config["Kafka"];
            for (const auto& key : kafkaConfig.getMemberNames()) {
                CustomConfig[key] = kafkaConfig[key].asString();
            }
        }

        if (config.isMember("Headers") && config["Headers"].isArray()) {
            const Json::Value& headers = config["Headers"];
            for (const auto& h : headers) {
                if (!h.isObject()) {
                    continue;
                }
                std::string key;
                std::string val;
                if (h.isMember("key") && h["key"].isString())
                    key = h["key"].asString();
                if (h.isMember("value") && h["value"].isString())
                    val = h["value"].asString();
                if (!key.empty())
                    Headers.emplace_back(std::move(key), std::move(val));
            }
        }

        return true;
    }
};

} // namespace logtail
