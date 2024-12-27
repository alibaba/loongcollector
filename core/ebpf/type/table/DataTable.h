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

#include <memory>
#include <functional>
#include <string_view>
#include <chrono>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "models/ArrayView.h"

namespace logtail {

/* AggregationType defination
NoAggregate means that this field doesn't engage in aggregate. level means the level of aggregation map this field will exist in. 
Note that currently we use Level0 to represent the fixed fields of exporter struct ApplicationBatchMeasure. they are expected to be aggregated first. 
*/
enum class AggregationType {
  NoAggregate,
  Level0,
  Level1,
  AggregationTypeBoundry,  // notice: DO NOT use AggregationTypeBoundry when assigning AggregationType. 
};

constexpr int MinAggregationLevel = static_cast<int>(AggregationType::NoAggregate) + 1;
constexpr int MaxAggregationLevel = static_cast<int>(AggregationType::AggregationTypeBoundry) - 1;


class DataElement {
 public:
  constexpr DataElement() = delete;
  constexpr DataElement(std::string_view name, std::string_view metric_key, 
    std::string_view span_key, std::string_view log_key, 
    std::string_view desc, 
    AggregationType agg_type = AggregationType::NoAggregate)
      : name_(name), metric_key_(metric_key), span_key_(span_key), log_key_(log_key), desc_(desc), agg_type_(agg_type) {
  }

  constexpr std::string_view name() const { return name_; }
  constexpr std::string_view metric_key() const { return metric_key_; }
  constexpr std::string_view span_key() const { return span_key_; }
  constexpr std::string_view log_key() const { return log_key_; }
  constexpr std::string_view desc() const { return desc_; }

  constexpr AggregationType agg_type() const { return agg_type_; }

 protected:
  const std::string_view name_;
  const std::string_view metric_key_;
  const std::string_view span_key_;
  const std::string_view log_key_;
  const std::string_view desc_;
  const AggregationType agg_type_ = AggregationType::NoAggregate;
};

class DataTableSchema {
 public:
  // TODO(oazizi): This constructor should only be called at compile-time. Need to enforce this.
  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, std::string_view desc,
                            const DataElement (&elements)[N])
      : name_(name), desc_(desc), elements_(elements) {
  }

  template <std::size_t N>
  constexpr DataTableSchema(std::string_view name, std::string_view desc,
                            const DataElement (&elements)[N])
      : name_(name),
        desc_(desc),
        elements_(elements) {
  }

  DataTableSchema(std::string_view name, std::string_view desc,
                  const std::vector<DataElement>& elements)
      : name_(name), desc_(desc), elements_(elements.data(), elements.size()) {
  }

  constexpr std::string_view name() const { return name_; }
  constexpr std::string_view desc() const { return desc_; }
  constexpr ArrayView<DataElement> elements() const { return elements_; }

  constexpr uint32_t ColIndex(std::string_view key) const {
    uint32_t i = 0;
    for (i = 0; i < elements_.size(); i++) {
      if (elements_[i].name() == key) {
        break;
      }
    }
    return i;
  }

  constexpr bool HasCol(std::string_view key) const {
    uint32_t i = 0;
    for (i = 0; i < elements_.size(); i++) {
      if (elements_[i].name() == key) {
        return true;
      }
    }
    return false;
  }

  constexpr std::string_view ColName(size_t i) const {
    return elements_[i].name();
  }

  constexpr std::string_view ColMetricKey(size_t i) const {
    return elements_[i].metric_key();
  }
  constexpr std::string_view ColSpanKey(size_t i) const {
    return elements_[i].span_key();
  }
  constexpr std::string_view ColLogKey(size_t i) const {
    return elements_[i].log_key();
  }

 private:
  constexpr void CheckSchema() {
  }

  const std::string_view name_;
  const std::string_view desc_;
  const ArrayView<DataElement> elements_;
};

}