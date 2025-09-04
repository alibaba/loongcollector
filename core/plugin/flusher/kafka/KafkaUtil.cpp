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

#include "plugin/flusher/kafka/KafkaUtil.h"

#include <map>
#include <sstream>
#include <vector>

#include "plugin/flusher/kafka/KafkaConstant.h"

namespace logtail {

std::string KafkaUtil::BrokersToString(const std::vector<std::string>& brokers) {
    if (brokers.empty()) {
        return "";
    }

    std::stringstream ss;
    for (size_t i = 0; i < brokers.size(); ++i) {
        if (i > 0) {
            ss << ",";
        }
        ss << brokers[i];
    }
    return ss.str();
}

static bool SplitVersion(const std::string& s, std::vector<int>& parts) {
    parts.clear();
    if (s.empty()) {
        return false;
    }
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, '.')) {
        if (item.empty()) {
            return false;
        }
        for (char c : item) {
            if (c < '0' || c > '9') {
                return false;
            }
        }
        try {
            int v = std::stoi(item);
            if (v < 0) {
                return false;
            }
            parts.push_back(v);
        } catch (...) {
            return false;
        }
        if (parts.size() > 4) {
            return false;
        }
    }
    return !parts.empty();
}

bool KafkaUtil::ParseKafkaVersion(const std::string& in, Version& out) {
    std::vector<int> parts;
    if (!SplitVersion(in, parts)) {
        return false;
    }
    while (parts.size() < 3) {
        parts.push_back(0);
    }
    if (parts.size() == 3) {
        parts.push_back(0);
    }
    out.major = parts[0];
    out.minor = parts[1];
    out.patch = parts[2];
    out.build = parts[3];
    return true;
}

static int CmpVersion(const KafkaUtil::Version& a, const KafkaUtil::Version& b) {
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    if (a.build != b.build)
        return a.build < b.build ? -1 : 1;
    return 0;
}

void KafkaUtil::DeriveApiVersionConfigs(const std::string& userKafkaVersion,
                                        std::map<std::string, std::string>& outConfigs) {
    outConfigs.clear();

    if (userKafkaVersion.empty()) {
        return;
    }

    Version v{};
    if (!ParseKafkaVersion(userKafkaVersion, v)) {
        return;
    }

    Version v0100{0, 10, 0, 0};

    if (CmpVersion(v, v0100) >= 0) {
        outConfigs[KAFKA_CONFIG_API_VERSION_REQUEST] = "true";
        outConfigs[KAFKA_CONFIG_API_VERSION_FALLBACK_MS] = "0";
        return;
    }

    if (v.major == 0 && v.minor == 9 && v.patch == 0) {
        outConfigs[KAFKA_CONFIG_API_VERSION_REQUEST] = "false";
        outConfigs[KAFKA_CONFIG_BROKER_VERSION_FALLBACK] = userKafkaVersion;
        return;
    }

    if (v.major == 0 && v.minor == 8) {
        outConfigs[KAFKA_CONFIG_API_VERSION_REQUEST] = "false";
        outConfigs[KAFKA_CONFIG_BROKER_VERSION_FALLBACK] = userKafkaVersion;
        return;
    }

    outConfigs[KAFKA_CONFIG_API_VERSION_REQUEST] = "false";
    outConfigs[KAFKA_CONFIG_BROKER_VERSION_FALLBACK] = userKafkaVersion;
}

} // namespace logtail
