/*
 * Copyright 2024 iLogtail Authors
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
#include <vector>

#include "collection_pipeline/serializer/Serializer.h"
#include "protobuf/sls/LogGroupSerializer.h"

namespace logtail {

struct MetricEventContentCacheItem {
    std::vector<std::string> mMetricEventContentCache;
    size_t mLabelSize = 0;
};

class SLSEventGroupSerializer : public Serializer<BatchedEvents> {
public:
    SLSEventGroupSerializer(Flusher* f) : Serializer<BatchedEvents>(f) {}

private:
    bool Serialize(BatchedEvents&& p, std::string& res, std::string& errorMsg) override;

    void CalculateLogEventSize(const BatchedEvents& group,
                               size_t& logGroupSZ,
                               std::vector<size_t>& logSZ,
                               bool enableNs) const;
    void CalculateMetricEventSize(const BatchedEvents& group,
                                  size_t& logGroupSZ,
                                  std::vector<MetricEventContentCacheItem>& metricEventContentCache,
                                  std::vector<size_t>& logSZ) const;
    void CalculateSpanEventSize(const BatchedEvents& group,
                                size_t& logGroupSZ,
                                std::vector<std::array<std::string, 6>>& spanEventContentCache,
                                std::vector<size_t>& logSZ) const;
    void CalculateRawEventSize(const BatchedEvents& group,
                               size_t& logGroupSZ,
                               std::vector<size_t>& logSZ,
                               bool enableNs) const;

    void SerializeLogEvent(LogGroupSerializer& serializer,
                           const BatchedEvents& group,
                           std::vector<size_t>& logSZ,
                           bool enableNs) const;
    void SerializeMetricEvent(LogGroupSerializer& serializer,
                              BatchedEvents& group,
                              std::vector<MetricEventContentCacheItem>& metricEventContentCache,
                              std::vector<size_t>& logSZ) const;
    void SerializeSpanEvent(LogGroupSerializer& serializer,
                            const BatchedEvents& group,
                            std::vector<std::array<std::string, 6>>& spanEventContentCache,
                            std::vector<size_t>& logSZ) const;
    void SerializeRawEvent(LogGroupSerializer& serializer,
                           const BatchedEvents& group,
                           std::vector<size_t>& logSZ,
                           bool enableNs) const;
};

struct CompressedLogGroup {
    std::string mData;
    size_t mRawSize;

    CompressedLogGroup(std::string&& data, size_t rawSize) : mData(std::move(data)), mRawSize(rawSize) {}
};

template <>
bool Serializer<std::vector<CompressedLogGroup>>::DoSerialize(std::vector<CompressedLogGroup>&& p,
                                                              std::string& output,
                                                              std::string& errorMsg);

class SLSEventGroupListSerializer : public Serializer<std::vector<CompressedLogGroup>> {
public:
    SLSEventGroupListSerializer(Flusher* f) : Serializer<std::vector<CompressedLogGroup>>(f) {}

private:
    bool Serialize(std::vector<CompressedLogGroup>&& v, std::string& res, std::string& errorMsg) override;
};

} // namespace logtail
