/*
 * Copyright 2025 iLogtail Authors
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

#include <algorithm>
#include <memory>

#include "common/FieldEntry.h"

namespace logtail {

// MetricCalculate 用于计算各个collector采集的指标的最大值、最小值和均值
// TMetric为各个collector保存指标数据的类，TField为各个collector保存指标数据的字段
// 计算完成后，各collector打上tag，生成对应的metricEvent

template <typename TMetric, typename TField = double>
class MetricCalculate {
public:
    using FieldMeta = FieldName<TMetric, TField>;
    void Reset() {
        mCount = 0;
        mMax.reset();
        mMin.reset();
        mTotal.reset();
        mLast.reset();
    }

    void AddValue(const TMetric& v) {
        mCount++;
        if (mCount == 1) {
            // First value - initialize all stats
            mMax.emplace(v);
            mMin.emplace(v);
            mTotal.emplace(v);
            mLast.emplace(v);
        } else {
            // Update existing stats
            TMetric::enumerate([&](const auto& field) {
                const TField& value = field.value(v);
                field.value(*mMax) = std::max(field.value(*mMax), value);
                field.value(*mMin) = std::min(field.value(*mMin), value);
                field.value(*mTotal) += value;
            });
            mLast.emplace(v);
        }
    }

    bool GetMaxValue(TMetric& dst) const {
        if (!mMax.has_value()) {
            return false;
        }

        dst = *mMax;
        return true;
    }

    bool GetMinValue(TMetric& dst) const {
        if (!mMin.has_value()) {
            return false;
        }

        dst = *mMin;
        return true;
    }

    bool GetAvgValue(TMetric& dst) const {
        if (!mTotal.has_value()) {
            return false;
        }

        dst = *mTotal;
        if (mCount > 1) {
            TMetric::enumerate([&](const auto& field) { field.value(dst) /= mCount; });
        }
        return true;
    }

    bool GetLastValue(TMetric& dst) const {
        if (!mLast.has_value()) {
            return false;
        }

        dst = *mLast;
        return true;
    }

    std::shared_ptr<TMetric> GetLastValue() const {
        if (!mLast.has_value()) {
            return nullptr;
        }

        return std::make_shared<TMetric>(*mLast);
    }

    bool Stat(TMetric& max, TMetric& min, TMetric& avg, TMetric* last = nullptr) {
        bool success = GetMaxValue(max) && GetMinValue(min) && GetAvgValue(avg);
        if (success && last) {
            GetLastValue(*last);
        }
        return success;
    }

    size_t Count() const { return mCount; }

private:
    size_t mCount = 0;
    std::optional<TMetric> mMax;
    std::optional<TMetric> mMin;
    std::optional<TMetric> mTotal;
    std::optional<TMetric> mLast;
};


} // namespace logtail
