/*
 * Copyright 2023 iLogtail Authors
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
#include <string>

#include "models/StringView.h"
#include "models/MetricEvent.h"

namespace logtail {

extern const std::string METRIC_RESERVED_KEY_NAME;
extern const std::string METRIC_RESERVED_KEY_LABELS;
extern const std::string METRIC_RESERVED_KEY_VALUE;
extern const std::string METRIC_RESERVED_KEY_TIME_NANO;

extern const std::string METRIC_LABELS_SEPARATOR;
extern const std::string METRIC_LABELS_KEY_VALUE_SEPARATOR;

// see for detail: https://protobuf.dev/programming-guides/encoding/
class LogGroupSerializer {
public:
    void Prepare(size_t size);
    void StartToAddLog(size_t size);
    void AddLogTime(uint32_t logTime);
    void AddLogContent(StringView key, StringView value);
    void AddLogTimeNs(uint32_t logTimeNs);
    void AddTopic(StringView topic);
    void AddSource(StringView source);
    void AddMachineUUID(StringView machineUUID);
    void AddLogTag(StringView key, StringView value);
    std::string& GetResult() { return mRes; }

    void AddLogContentMetricLabel(const MetricEvent& e, size_t valueSZ);
    void AddLogContentMetricTimeNano(const MetricEvent& e);

private:
    void AddString(StringView value);

    std::string mRes;
};

size_t GetLogContentSize(size_t keySZ, size_t valueSZ);
size_t GetLogSize(size_t contentSZ, bool hasNs, size_t& logSZ);
size_t GetStringSize(size_t size);
size_t GetLogTagSize(size_t keySZ, size_t valueSZ);

size_t GetMetricLabelSize(const MetricEvent& e);

} // namespace logtail
