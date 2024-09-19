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

#include "application/Application.h"
#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "common/compression/CompressType.h"
#include "plugin/flusher/sls/FlusherSLS.h"


DEFINE_FLAG_INT32(max_send_log_group_size, "bytes", 10 * 1024 * 1024);

const std::string METRIC_RESERVED_KEY_NAME = "__name__";
const std::string METRIC_RESERVED_KEY_LABELS = "__labels__";
const std::string METRIC_RESERVED_KEY_VALUE = "__value__";
const std::string METRIC_RESERVED_KEY_TIME_NANO = "__time_nano__";

const std::string METRIC_LABELS_SEPARATOR = "|";
const std::string METRIC_LABELS_KEY_VALUE_SEPARATOR = "#$#";

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
    mInItemsCnt->Add(1);
    mInItemSizeBytes->Add(inputSize);

    auto before = std::chrono::system_clock::now();
    auto res = Serialize(std::move(p), output, errorMsg);
    mTotalDelayMs->Add(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - before).count());

    if (res) {
        mOutItemsCnt->Add(1);
        mOutItemSizeBytes->Add(output.size());
    } else {
        mDiscardedItemsCnt->Add(1);
        mDiscardedItemSizeBytes->Add(inputSize);
    }
    return res;
}

bool SLSEventGroupSerializer::Serialize(BatchedEvents&& group, string& res, string& errorMsg) {
    sls_logs::LogGroup logGroup;
    for (const auto& e : group.mEvents) {
        if (e.Is<LogEvent>()) {
            const auto& logEvent = e.Cast<LogEvent>();
            auto log = logGroup.add_logs();
            for (const auto& kv : logEvent) {
                auto contPtr = log->add_contents();
                contPtr->set_key(kv.first.to_string());
                contPtr->set_value(kv.second.to_string());
            }
            log->set_time(logEvent.GetTimestamp());
            if (mFlusher->GetContext().GetGlobalConfig().mEnableTimestampNanosecond
                && logEvent.GetTimestampNanosecond()) {
                log->set_time_ns(logEvent.GetTimestampNanosecond().value());
            }
        } else if (e.Is<MetricEvent>()) {
            const auto& metricEvent = e.Cast<MetricEvent>();
            if (metricEvent.Is<std::monostate>()) {
                continue;
            }
            auto log = logGroup.add_logs();
            std::ostringstream oss;
            // set __labels__
            bool hasPrev = false;
            for (auto it = metricEvent.TagsBegin(); it != metricEvent.TagsEnd(); ++it) {
                if (hasPrev) {
                    oss << METRIC_LABELS_SEPARATOR;
                }
                hasPrev = true;
                oss << it->first << METRIC_LABELS_KEY_VALUE_SEPARATOR << it->second;
            }
            auto logPtr = log->add_contents();
            logPtr->set_key(METRIC_RESERVED_KEY_LABELS);
            logPtr->set_value(oss.str());
            // set time, no need to set nanosecond for metric
            log->set_time(metricEvent.GetTimestamp());
            // set __time_nano__
            logPtr = log->add_contents();
            logPtr->set_key(METRIC_RESERVED_KEY_TIME_NANO);
            if (metricEvent.GetTimestampNanosecond()) {
                logPtr->set_value(std::to_string(metricEvent.GetTimestamp())
                                  + NumberToDigitString(metricEvent.GetTimestampNanosecond().value(), 9));
            } else {
                logPtr->set_value(std::to_string(metricEvent.GetTimestamp()));
            }
            // set __value__
            if (metricEvent.Is<UntypedSingleValue>()) {
                double value = metricEvent.GetValue<UntypedSingleValue>()->mValue;
                logPtr = log->add_contents();
                logPtr->set_key(METRIC_RESERVED_KEY_VALUE);
                logPtr->set_value(std::to_string(value));
            }
            // set __name__
            logPtr = log->add_contents();
            logPtr->set_key(METRIC_RESERVED_KEY_NAME);
            logPtr->set_value(metricEvent.GetName().to_string());
        } else {
            errorMsg = "unsupported event type in event group";
            return false;
        }
    }
    for (const auto& tag : group.mTags.mInner) {
        if (tag.first == LOG_RESERVED_KEY_TOPIC) {
            logGroup.set_topic(tag.second.to_string());
        } else if (tag.first == LOG_RESERVED_KEY_SOURCE) {
            logGroup.set_source(tag.second.to_string());
        } else if (tag.first == LOG_RESERVED_KEY_MACHINE_UUID) {
            logGroup.set_machineuuid(tag.second.to_string());
        } else {
            auto logTag = logGroup.add_logtags();
            logTag->set_key(tag.first.to_string());
            logTag->set_value(tag.second.to_string());
        }
    }
    // loggroup.category is deprecated, no need to set
    size_t size = logGroup.ByteSizeLong();
    if (static_cast<int32_t>(size) > INT32_FLAG(max_send_log_group_size)) {
        errorMsg = "log group exceeds size limit\tgroup size: " + ToString(size)
            + "\tsize limit: " + ToString(INT32_FLAG(max_send_log_group_size));
        return false;
    }
    res = logGroup.SerializeAsString();
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
