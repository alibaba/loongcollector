/**
 * Copyright 2025 Alibaba Cloud
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
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
    typedef FieldName<TMetric, TField> FieldMeta;
    void Reset() { mCount = 0; }

    void AddValue(const TMetric& v) {
        mCount++;
        if (1 == mCount) {
            enumerate([&](const FieldMeta& field) {
                const TField& metricValue = field.value(v);

                field.value(mMax) = metricValue;
                field.value(mMin) = metricValue;
                field.value(mTotal) = metricValue;
                field.value(mLast) = metricValue;
            });
        } else {
            enumerate([&](const FieldMeta& field) {
                const TField& metricValue = field.value(v);

                field.value(mMax) = std::max(field.value(mMax), metricValue);
                field.value(mMin) = std::min(field.value(mMin), metricValue);
                field.value(mTotal) += metricValue;
                field.value(mLast) = metricValue;
            });
        }
    }

    bool GetMaxValue(TMetric& dst) const { return GetValue(mMax, dst); }

    bool GetMinValue(TMetric& dst) const { return GetValue(mMin, dst); }

    bool GetAvgValue(TMetric& dst) const {
        bool exist = GetValue(mTotal, dst);
        if (exist && mCount > 1) {
            enumerate([&](const FieldMeta& field) { field.value(dst) /= mCount; });
        }
        return exist;
    }

    // 统计，计算最大、最小、均值
    bool Stat(TMetric& max, TMetric& min, TMetric& avg, TMetric* last = nullptr) {
        return GetMaxValue(max) && GetMinValue(min) && GetAvgValue(avg) && (last == nullptr || GetLastValue(*last));
    }

    bool GetLastValue(TMetric& dst) const { return GetValue(mLast, dst); }

    std::shared_ptr<TMetric> GetLastValue() const {
        auto ret = std::make_shared<TMetric>();
        if (!GetValue(mLast, *ret)) {
            ret.reset();
        }
        return ret;
    }

    size_t Count() const { return mCount; }

private:
    bool GetValue(const TMetric& src, TMetric& dst) const {
        bool exist = (mCount > 0);
        if (exist) {
            dst = src;
        }
        return exist;
    }

private:
    TMetric mMax;
    TMetric mMin;
    TMetric mTotal;
    TMetric mLast;
    size_t mCount = 0;
};

} // namespace logtail
