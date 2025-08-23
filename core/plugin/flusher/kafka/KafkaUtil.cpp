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

#include <sstream>

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

} // namespace logtail
