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

#include <algorithm>

#include "common/TimeUtil.h"
#include "google/protobuf/io/coded_stream.h"

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
    if (v < ((size_t)1 << 7)) {
        return 1;
    } else if (v < ((size_t)1 << 14)) {
        return 2;
    } else if (v < ((size_t)1 << 21)) {
        return 3;
    } else if (v < ((size_t)1 << 28)) {
        return 4;
    } else {
        return 5;
    }
}

namespace {

using google::protobuf::io::CodedOutputStream;

// Each protobuf field tag here fits in a byte, followed by up to five varint bytes.
constexpr size_t kMaxTaggedVarintSize = 6;

inline uint8_t* WriteTaggedVarint(uint8_t* out, uint8_t tag, uint32_t value) {
    *out++ = tag;
    return CodedOutputStream::WriteVarint32ToArray(value, out);
}

inline void AppendTaggedVarint(string& output, uint8_t tag, uint32_t value) {
    uint8_t prefix[kMaxTaggedVarintSize];
    const auto* end = WriteTaggedVarint(prefix, tag, value);
    output.append(reinterpret_cast<const char*>(prefix), end - prefix);
}

inline void AppendKeyPrefix(string& output, uint8_t tag, uint32_t messageSize, uint32_t keySize) {
    uint8_t prefix[2 * kMaxTaggedVarintSize];
    auto* end = WriteTaggedVarint(prefix, tag, messageSize);
    end = WriteTaggedVarint(end, 0x0A, keySize);
    output.append(reinterpret_cast<const char*>(prefix), end - prefix);
}

inline void AppendStringField(string& output, uint8_t tag, StringView value) {
    AppendTaggedVarint(output, tag, value.size());
    output.append(value.data(), value.size());
}

inline void AppendKeyValue(string& output, uint8_t tag, StringView key, StringView value) {
    AppendKeyPrefix(output, tag, GetStringSize(key.size()) + GetStringSize(value.size()), key.size());
    output.append(key.data(), key.size());
    AppendStringField(output, 0x12, value);
}

} // namespace

void LogGroupSerializer::Prepare(size_t size) {
    mRes.clear();
    mRes.reserve(size);
}

void LogGroupSerializer::StartToAddLog(size_t size) {
    AppendTaggedVarint(mRes, 0x0A, size);
}

void LogGroupSerializer::AddLogTime(uint32_t logTime) {
    // Keep the timestamp varint at five bytes for the precomputed log size.
    constexpr uint32_t kMinLogTime = uint32_t{1} << 28;
    AppendTaggedVarint(mRes, 0x08, max(logTime, kMinLogTime));
}

void LogGroupSerializer::AddLogContent(StringView key, StringView value) {
    AppendKeyValue(mRes, 0x12, key, value);
}

void LogGroupSerializer::AddLogTimeNs(uint32_t logTimeNs) {
    uint8_t field[5] = {0x25};
    CodedOutputStream::WriteLittleEndian32ToArray(logTimeNs, field + 1);
    mRes.append(reinterpret_cast<const char*>(field), sizeof(field));
}

void LogGroupSerializer::AddTopic(StringView topic) {
    AppendStringField(mRes, 0x1A, topic);
}

void LogGroupSerializer::AddSource(StringView source) {
    AppendStringField(mRes, 0x22, source);
}

void LogGroupSerializer::AddMachineUUID(StringView machineUUID) {
    AppendStringField(mRes, 0x2A, machineUUID);
}

void LogGroupSerializer::AddLogTag(StringView key, StringView value) {
    AppendKeyValue(mRes, 0x32, key, value);
}

void LogGroupSerializer::AddLogContentMetricLabel(const MetricEvent& e, size_t valueSZ) {
    AppendKeyPrefix(mRes,
                    0x12,
                    GetStringSize(METRIC_RESERVED_KEY_LABELS.size()) + GetStringSize(valueSZ),
                    METRIC_RESERVED_KEY_LABELS.size());
    mRes.append(METRIC_RESERVED_KEY_LABELS);
    AppendTaggedVarint(mRes, 0x12, valueSZ);
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
    AppendKeyPrefix(mRes,
                    0x12,
                    GetStringSize(METRIC_RESERVED_KEY_TIME_NANO.size()) + GetStringSize(valueSZ),
                    METRIC_RESERVED_KEY_TIME_NANO.size());
    mRes.append(METRIC_RESERVED_KEY_TIME_NANO);
    AppendTaggedVarint(mRes, 0x12, valueSZ);
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

    if (e.TagsSize() == 0) {
        return 0;
    }

    size_t valueSZ = e.TagsSize() * keyValSepSZ + (e.TagsSize() - 1) * labelSepSZ;
    for (auto it = e.TagsBegin(); it != e.TagsEnd(); ++it) {
        valueSZ += it->first.size() + it->second.size();
    }
    return valueSZ;
}

} // namespace logtail
