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

#include <chrono>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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

constexpr int MinAggregationLevel = static_cast<int>(AggregationType::NoAggregate) + 1;
constexpr int MaxAggregationLevel = static_cast<int>(AggregationType::AggregationTypeBoundry) - 1;


class DataElement {
public:
    constexpr DataElement() = delete;
    constexpr DataElement(StringView name,
                          StringView metric_key,
                          StringView span_key,
                          StringView log_key,
                          StringView desc,
                          AggregationType agg_type = AggregationType::NoAggregate)
        : name_(name),
          metric_key_(metric_key),
          span_key_(span_key),
          log_key_(log_key),
          desc_(desc),
          agg_type_(agg_type) {}

    constexpr StringView name() const { return name_; }
    constexpr StringView metric_key() const { return metric_key_; }
    constexpr StringView span_key() const { return span_key_; }
    constexpr StringView log_key() const { return log_key_; }
    constexpr StringView desc() const { return desc_; }

    constexpr AggregationType agg_type() const { return agg_type_; }

protected:
    const StringView name_;
    const StringView metric_key_;
    const StringView span_key_;
    const StringView log_key_;
    const StringView desc_;
    const AggregationType agg_type_ = AggregationType::NoAggregate;
};

class DataTableSchema {
public:
    template <std::size_t N>
    constexpr DataTableSchema(StringView name, StringView desc, const DataElement (&elements)[N])
        : name_(name), desc_(desc), elements_(elements) {}

    DataTableSchema(StringView name, StringView desc, const std::vector<DataElement>& elements)
        : name_(name), desc_(desc), elements_(elements.data(), elements.size()) {}

    constexpr StringView name() const { return name_; }
    constexpr StringView desc() const { return desc_; }
    constexpr ArrayView<DataElement> elements() const { return elements_; }

    constexpr uint32_t ColIndex(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < elements_.size(); i++) {
            if (elements_[i].name() == key) {
                break;
            }
        }
        return i;
    }

    constexpr bool HasCol(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < elements_.size(); i++) {
            if (elements_[i].name() == key) {
                return true;
            }
        }
        return false;
    }

    constexpr StringView ColName(size_t i) const { return elements_[i].name(); }
    constexpr StringView ColMetricKey(size_t i) const { return elements_[i].metric_key(); }
    constexpr StringView ColSpanKey(size_t i) const { return elements_[i].span_key(); }
    constexpr StringView ColLogKey(size_t i) const { return elements_[i].log_key(); }

private:
    const StringView name_;
    const StringView desc_;
    const ArrayView<DataElement> elements_;
};

} // namespace ebpf
} // namespace logtail
