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

#include "pipeline/serializer/SLSSerializer.h"

#include "common/Flags.h"
#include "common/compression/CompressType.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "protobuf/sls/LogGroupSerializer.h"

DEFINE_FLAG_INT32(max_send_log_group_size, "bytes", 10 * 1024 * 1024);

using namespace std;

namespace logtail {

template <>
bool Serializer<vector<CompressedLogGroup>>::DoSerialize(vector<CompressedLogGroup>&& p,
                                                         std::string& output,
                                                         std::string& errorMsg) {
    auto inputSize = 0;
    for (auto& item : p) {
        inputSize += item.mData.size();
    }
    mInItemsTotal->Add(1);
    mInItemSizeBytes->Add(inputSize);

    auto before = std::chrono::system_clock::now();
    auto res = Serialize(std::move(p), output, errorMsg);
    mTotalProcessMs->Add(std::chrono::system_clock::now() - before);

    if (res) {
        mOutItemsTotal->Add(1);
        mOutItemSizeBytes->Add(output.size());
    } else {
        mDiscardedItemsTotal->Add(1);
        mDiscardedItemSizeBytes->Add(inputSize);
    }
    return res;
}

bool SLSEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    if (group.mEvents.empty()) {
        errorMsg = "empty event group";
        return false;
    }

    PipelineEvent::Type eventType = group.mEvents[0]->GetType();
    if (eventType == PipelineEvent::Type::NONE) {
        // should not happen
        errorMsg = "unsupported event type in event group";
        return false;
    }

    bool enableNs = mFlusher->GetContext().GetGlobalConfig().mEnableTimestampNanosecond;

    // caculate serialized logGroup size first, where some critical results can be cached
    vector<size_t> logSZ(group.mEvents.size());
    vector<pair<string, size_t>> metricEventContentCache(group.mEvents.size());
    size_t logGroupSZ = 0;
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<LogEvent>();
                if (e.Empty()) {
                    continue;
                }
                size_t contentSZ = 0;
                for (const auto& kv : e) {
                    contentSZ += GetLogContentSize(kv.first.size(), kv.second.size());
                }
                logGroupSZ += GetLogSize(contentSZ, enableNs && e.GetTimestampNanosecond(), logSZ[i]);
            }
            break;
        case PipelineEvent::Type::METRIC:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& e = group.mEvents[i].Cast<MetricEvent>();
                if (e.Is<UntypedSingleValue>()) {
                    metricEventContentCache[i].first = to_string(e.GetValue<UntypedSingleValue>()->mValue);
                } else {
                    // should not happen
                    LOG_ERROR(sLogger,
                              ("unexpected error",
                               "invalid metric event type")("config", mFlusher->GetContext().GetConfigName()));
                    continue;
                }
                metricEventContentCache[i].second = GetMetricLabelSize(e);

                size_t contentSZ = 0;
                contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_NAME.size(), e.GetName().size());
                contentSZ
                    += GetLogContentSize(METRIC_RESERVED_KEY_VALUE.size(), metricEventContentCache[i].first.size());
                contentSZ
                    += GetLogContentSize(METRIC_RESERVED_KEY_TIME_NANO.size(), e.GetTimestampNanosecond() ? 19U : 10U);
                contentSZ += GetLogContentSize(METRIC_RESERVED_KEY_LABELS.size(), metricEventContentCache[i].second);
                logGroupSZ += GetLogSize(contentSZ, false, logSZ[i]);
            }
            break;
        case PipelineEvent::Type::SPAN:
            break;
        default:
            break;
    }
    if (logGroupSZ == 0) {
        errorMsg = "all empty logs";
        return false;
    }

    // loggroup.category is deprecated, no need to set
    for (const auto& tag : group.mTags.mInner) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC || tag.first == LOG_RESERVED_KEY_SOURCE
            || tag.first == LOG_RESERVED_KEY_MACHINE_UUID) {
            logGroupSZ += GetStringSize(tag.second.size());
        } else {
            logGroupSZ += GetLogTagSize(tag.first.size(), tag.second.size());
        }
    }

    if (static_cast<int32_t>(logGroupSZ) > INT32_FLAG(max_send_log_group_size)) {
        errorMsg = "log group exceeds size limit\tgroup size: " + ToString(logGroupSZ)
            + "\tsize limit: " + ToString(INT32_FLAG(max_send_log_group_size));
        return false;
    }

    thread_local LogGroupSerializer serializer;
    serializer.Prepare(logGroupSZ);
    switch (eventType) {
        case PipelineEvent::Type::LOG:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& logEvent = group.mEvents[i].Cast<LogEvent>();
                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(logEvent.GetTimestamp());
                for (const auto& kv : logEvent) {
                    serializer.AddLogContent(kv.first, kv.second);
                }
                if (enableNs && logEvent.GetTimestampNanosecond()) {
                    serializer.AddLogTimeNs(logEvent.GetTimestampNanosecond().value());
                }
            }
            break;
        case PipelineEvent::Type::METRIC:
            for (size_t i = 0; i < group.mEvents.size(); ++i) {
                const auto& metricEvent = group.mEvents[i].Cast<MetricEvent>();
                if (metricEvent.Is<std::monostate>()) {
                    continue;
                }
                serializer.StartToAddLog(logSZ[i]);
                serializer.AddLogTime(metricEvent.GetTimestamp());
                serializer.AddLogContentMetricLabel(metricEvent, metricEventContentCache[i].second);
                serializer.AddLogContentMetricTimeNano(metricEvent);
                serializer.AddLogContent(METRIC_RESERVED_KEY_VALUE, metricEventContentCache[i].first);
                serializer.AddLogContent(METRIC_RESERVED_KEY_NAME, metricEvent.GetName());
            }
            break;
        case PipelineEvent::Type::SPAN:
            break;
        default:
            break;
    }
    for (const auto& tag : group.mTags.mInner) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC) {
            serializer.AddTopic(tag.second);
        } else if (tag.first == LOG_RESERVED_KEY_SOURCE) {
            serializer.AddSource(tag.second);
        } else if (tag.first == LOG_RESERVED_KEY_MACHINE_UUID) {
            serializer.AddMachineUUID(tag.second);
        } else {
            serializer.AddLogTag(tag.first, tag.second);
        }
    }
    res = std::move(serializer.GetResult());
    return true;
}

bool SLSEventGroupListSerializer::Serialize(vector<CompressedLogGroup>&& v, string& res, string& errorMsg) {
    sls_logs::SlsLogPackageList logPackageList;
    for (const auto& item : v) {
        auto package = logPackageList.add_packages();
        package->set_data(item.mData);
        package->set_uncompress_size(item.mRawSize);

        CompressType compressType = static_cast<const FlusherSLS*>(mFlusher)->GetCompressType();
        sls_logs::SlsCompressType slsCompressType = sls_logs::SLS_CMP_LZ4;
        if (compressType == CompressType::NONE) {
            slsCompressType = sls_logs::SLS_CMP_NONE;
        } else if (compressType == CompressType::ZSTD) {
            slsCompressType = sls_logs::SLS_CMP_ZSTD;
        }
        package->set_compress_type(slsCompressType);
    }
    res = logPackageList.SerializeAsString();
    return true;
}

} // namespace logtail
