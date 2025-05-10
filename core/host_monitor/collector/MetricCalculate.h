#ifndef LOONGCOLLECTOR_METRIC_CALCULATE_H
#define LOONGCOLLECTOR_METRIC_CALCULATE_H
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#include <algorithm>
#include <memory>

#include "common/FieldEntry.h"

namespace logtail {

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
            enumerate([&](const FieldName<TMetric, TField>& field) {
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
            enumerate([&](const FieldName<TMetric, TField>& field) { field.value(dst) /= mCount; });
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
#endif // LOONGCOLLECTOR_METRIC_CALCULATE_H
