// Copyright 2023 iLogtail Authors
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

#pragma once

#include "models/ArrayView.h"
#include "models/StringView.h"

namespace logtail {
namespace ebpf {

/* AggregationType defination
NoAggregate means that this field doesn't engage in aggregate. level means the level of aggregation map this field will
exist in. Note that currently we use Level0 to represent the fixed fields of exporter struct ApplicationBatchMeasure.
they are expected to be aggregated first.
*/
enum class AggregationType {
    NoAggregate,
    Level0,
    Level1,
    AggregationTypeBoundry, // notice: DO NOT use AggregationTypeBoundry when assigning AggregationType.
};

constexpr int kMinAggregationLevel = static_cast<int>(AggregationType::NoAggregate) + 1;
constexpr int kMaxAggregationLevel = static_cast<int>(AggregationType::AggregationTypeBoundry) - 1;


class DataElement {
public:
    constexpr DataElement() = delete;
    constexpr DataElement(StringView name,
                          StringView metricKey,
                          StringView spanKey,
                          StringView logKey,
                          StringView desc,
                          AggregationType aggType = AggregationType::NoAggregate)
        : mName(name), mMetricKey(metricKey), mSpanKey(spanKey), mLogKey(logKey), mDesc(desc), mAggType(aggType) {}

    constexpr bool operator==(const DataElement& rhs) const { return mName == rhs.mName; }

    constexpr bool operator!=(const DataElement& rhs) const { return !(*this == rhs); }

    constexpr StringView Name() const { return mName; }
    constexpr StringView MetricKey() const { return mMetricKey; }
    constexpr StringView SpanKey() const { return mSpanKey; }
    constexpr StringView LogKey() const { return mLogKey; }
    constexpr StringView Desc() const { return mDesc; }

    constexpr AggregationType AggType() const { return mAggType; }

protected:
    const StringView mName;
    const StringView mMetricKey;
    const StringView mSpanKey;
    const StringView mLogKey;
    const StringView mDesc;
    const AggregationType mAggType = AggregationType::NoAggregate;
};

class DataTableSchema {
public:
    template <std::size_t N>
    constexpr DataTableSchema(StringView name, StringView desc, const DataElement (&elements)[N])
        : mName(name), mDesc(desc), mElements(elements) {}

    constexpr StringView Name() const { return mName; }
    constexpr StringView Desc() const { return mDesc; }
    constexpr ArrayView<DataElement> Elements() const { return mElements; }
    constexpr size_t Size() const { return mElements.size(); }

    constexpr uint32_t ColIndex(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < mElements.size(); i++) {
            if (mElements[i].Name() == key) {
                break;
            }
        }
        return i;
    }

    constexpr bool HasCol(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < mElements.size(); i++) {
            if (mElements[i].Name() == key) {
                return true;
            }
        }
        return false;
    }

    constexpr StringView ColName(size_t i) const { return mElements[i].Name(); }
    constexpr StringView ColMetricKey(size_t i) const { return mElements[i].MetricKey(); }
    constexpr StringView ColSpanKey(size_t i) const { return mElements[i].SpanKey(); }
    constexpr StringView ColLogKey(size_t i) const { return mElements[i].LogKey(); }

private:
    const StringView mName;
    const StringView mDesc;
    const ArrayView<DataElement> mElements;
};

} // namespace ebpf
} // namespace logtail
