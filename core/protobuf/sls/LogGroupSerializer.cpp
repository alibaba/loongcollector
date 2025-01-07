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

#include "protobuf/sls/LogGroupSerializer.h"

#include "common/TimeUtil.h"

using namespace std;

namespace logtail {

const string METRIC_RESERVED_KEY_NAME = "__name__";
const string METRIC_RESERVED_KEY_LABELS = "__labels__";
const string METRIC_RESERVED_KEY_VALUE = "__value__";
const string METRIC_RESERVED_KEY_TIME_NANO = "__time_nano__";

const string METRIC_LABELS_SEPARATOR = "|";
const string METRIC_LABELS_KEY_VALUE_SEPARATOR = "#$#";

/**
 * Return the number of bytes required to store a variable-length unsigned
 * 32-bit integer in base-128 varint encoding.
 *
 * \param v
 *      Value to encode.
 * \return
 *      Number of bytes required.
 */
static inline size_t uint32_size(uint32_t v) {
    if (v < (1UL << 7)) {
        return 1;
    } else if (v < (1UL << 14)) {
        return 2;
    } else if (v < (1UL << 21)) {
        return 3;
    } else if (v < (1UL << 28)) {
        return 4;
    } else {
        return 5;
    }
}

/**
 * Pack an unsigned 32-bit integer in base-128 varint encoding and return the
 * number of bytes written, which must be 5 or less.
 *
 * \param value
 *      Value to encode.
 * \param[out] out
 *      Packed value.
 * \return
 *      Number of bytes written to `out`.
 */
static inline size_t uint32_pack(uint32_t value, string& output) {
    unsigned rv = 1;

    if (value >= 0x80) {
        output.push_back(value | 0x80);
        ++rv;
        value >>= 7;
        if (value >= 0x80) {
            output.push_back(value | 0x80);
            ++rv;
            value >>= 7;
            if (value >= 0x80) {
                output.push_back(value | 0x80);
                ++rv;
                value >>= 7;
                if (value >= 0x80) {
                    output.push_back(value | 0x80);
                    ++rv;
                    value >>= 7;
                }
            }
        }
    }
    /* assert: value<128 */
    output.push_back(value);
    return rv;
}

static inline void fixed32_pack(uint32_t value, string& output) {
    for (size_t i = 0; i < 4; ++i) {
        output.push_back(value & 0xFF);
        value >>= 8;
    }
}

void LogGroupSerializer::Prepare(size_t size) {
    mRes.clear();
    mRes.reserve(size);
}

void LogGroupSerializer::StartToAddLog(size_t size) {
    // field = 1, wire_type = 2
    mRes.push_back(0x0A);
    uint32_pack(size, mRes);
}

void LogGroupSerializer::AddLogTime(uint32_t logTime) {
    // limit logTime's min value, ensure varint size is 5, which is 1978-07-05 05:24:16
    static uint32_t minLogTime = 1UL << 28;
    if (logTime < minLogTime) {
        logTime = minLogTime;
    }
    // field = 1, wire_type = 0
    mRes.push_back(0x08);
    uint32_pack(logTime, mRes);
}

void LogGroupSerializer::AddLogContent(StringView key, StringView value) {
    // Contents
    // field = 2, wire_type = 2
    mRes.push_back(0x12);
    uint32_pack(GetStringSize(key.size()) + GetStringSize(value.size()), mRes);
    // Key
    // field = 1, wire_type = 2
    mRes.push_back(0x0A);
    uint32_pack(key.size(), mRes);
    mRes.append(key.data(), key.size());
    // Value
    // field = 2, wire_type = 2
    mRes.push_back(0x12);
    uint32_pack(value.size(), mRes);
    mRes.append(value.data(), value.size());
}

void LogGroupSerializer::AddLogTimeNs(uint32_t logTimeNs) {
    // field = 4, wire_type = 5
    mRes.push_back(0x25);
    fixed32_pack(logTimeNs, mRes);
}

void LogGroupSerializer::AddTopic(StringView topic) {
    // field = 3, wire_type = 2
    mRes.push_back(0x1A);
    AddString(topic);
}

void LogGroupSerializer::AddSource(StringView source) {
    // field = 4, wire_type = 2
    mRes.push_back(0x22);
    AddString(source);
}

void LogGroupSerializer::AddMachineUUID(StringView machineUUID) {
    // field = 5, wire_type = 2
    mRes.push_back(0x2A);
    AddString(machineUUID);
}

void LogGroupSerializer::AddLogTag(StringView key, StringView value) {
    // LogTags
    // field = 6, wire_type = 2
    mRes.push_back(0x32);
    uint32_pack(GetStringSize(key.size()) + GetStringSize(value.size()), mRes);
    // Key
    // field = 1, wire_type = 2
    mRes.push_back(0x0A);
    uint32_pack(key.size(), mRes);
    mRes.append(key.data(), key.size());
    // Value
    // field = 2, wire_type = 2
    mRes.push_back(0x12);
    uint32_pack(value.size(), mRes);
    mRes.append(value.data(), value.size());
}

void LogGroupSerializer::AddString(StringView value) {
    uint32_pack(value.size(), mRes);
    mRes.append(value.data(), value.size());
}

void LogGroupSerializer::AddLogContentMetricLabel(const MetricEvent& e, size_t valueSZ) {
    // Contents
    mRes.push_back(0x12);
    uint32_pack(GetStringSize(METRIC_RESERVED_KEY_LABELS.size()) + GetStringSize(valueSZ), mRes);
    // Key
    mRes.push_back(0x0A);
    uint32_pack(METRIC_RESERVED_KEY_LABELS.size(), mRes);
    mRes.append(METRIC_RESERVED_KEY_LABELS);
    // Value
    mRes.push_back(0x12);
    uint32_pack(valueSZ, mRes);
    bool hasPrev = false;
    for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
        if (hasPrev) {
            mRes.append(METRIC_LABELS_SEPARATOR);
        }
        hasPrev = true;
        mRes.append(it->first.data(), it->first.size());
        mRes.append(METRIC_LABELS_KEY_VALUE_SEPARATOR);
        mRes.append(it->second.data(), it->second.size());
    }
}

void LogGroupSerializer::AddLogContentMetricTimeNano(const MetricEvent& e) {
    size_t valueSZ = e.GetTimestampNanosecond() ? 19U : 10U;
    // Contents
    mRes.push_back(0x12);
    uint32_pack(GetStringSize(METRIC_RESERVED_KEY_TIME_NANO.size()) + GetStringSize(valueSZ), mRes);
    // Key
    mRes.push_back(0x0A);
    uint32_pack(METRIC_RESERVED_KEY_TIME_NANO.size(), mRes);
    mRes.append(METRIC_RESERVED_KEY_TIME_NANO);
    // Value
    mRes.push_back(0x12);
    uint32_pack(valueSZ, mRes);
    // TODO: avoid copy
    mRes.append(to_string(e.GetTimestamp()));
    if (e.GetTimestampNanosecond()) {
        mRes.append(NumberToDigitString(e.GetTimestampNanosecond().value(), 9));
    }
}

size_t GetLogContentSize(size_t keySZ, size_t valueSZ) {
    size_t res = 0;
    res += GetStringSize(keySZ) + GetStringSize(valueSZ);
    res += 1 + uint32_size(res);
    return res;
}

size_t GetLogSize(size_t contentSZ, bool hasNs, size_t& logSZ) {
    // Contents
    size_t res = contentSZ;
    // Time, assume varint size is 5
    res += 1 + 5;
    // Time_ns
    if (hasNs) {
        // fixed32 size is always 4
        res += 1 + 4;
    }
    logSZ = res;
    // Logs
    res += 1 + uint32_size(res);
    return res;
}

size_t GetStringSize(size_t size) {
    return 1 + uint32_size(size) + size;
}

size_t GetLogTagSize(size_t keySZ, size_t valueSZ) {
    size_t res = 0;
    res += GetStringSize(keySZ) + GetStringSize(valueSZ);
    res += 1 + uint32_size(res);
    return res;
}

size_t GetMetricLabelSize(const MetricEvent& e) {
    static size_t labelSepSZ = METRIC_LABELS_SEPARATOR.size();
    static size_t keyValSepSZ = METRIC_LABELS_KEY_VALUE_SEPARATOR.size();

    size_t valueSZ = e.TagsSize() * keyValSepSZ + (e.TagsSize() - 1) * labelSepSZ;
    for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
        valueSZ += it->first.size() + it->second.size();
    }
    return valueSZ;
}

} // namespace logtail
