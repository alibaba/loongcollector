/*
 * Copyright 2023 iLogtail Authors
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

#pragma once

#include <cstdint>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace logtail {

enum class MetricType {
    METRIC_TYPE_COUNTER,
    METRIC_TYPE_TIME_COUNTER,
    METRIC_TYPE_INT_GAUGE,
    METRIC_TYPE_DOUBLE_GAUGE,
};

class Counter {
protected:
    std::string mName;
    std::atomic_uint64_t mVal;

public:
    Counter(const std::string& name, uint64_t val = 0) : mName(name), mVal(val) {}
    uint64_t GetValue() const { return mVal.load(); }
    const std::string& GetName() const { return mName; }
    void Add(uint64_t val) { mVal.fetch_add(val); }
    Counter* Collect() { return new Counter(mName, mVal.exchange(0)); }
};

// input: nanosecond, output: milisecond
class TimeCounter : public Counter {
public:
    TimeCounter(const std::string& name, uint64_t val = 0) : Counter(name, val) {}
    uint64_t GetValue() const { return mVal.load() / 1000000; }
    void Add(std::chrono::nanoseconds val) { mVal.fetch_add(val.count()); }
    TimeCounter* Collect() { return new TimeCounter(mName, mVal.exchange(0)); }
};

template <typename T>
class Gauge {
public:
    Gauge(const std::string& name, T val = 0) : mName(name), mVal(val) {}
    ~Gauge() = default;

    T GetValue() const { return mVal.load(); }
    const std::string& GetName() const { return mName; }
    void Set(T val) { mVal.store(val); }
    Gauge* Collect() { return new Gauge<T>(mName, mVal.load()); }

protected:
    std::string mName;
    std::atomic<T> mVal;
};

class IntGauge : public Gauge<uint64_t> {
public:
    IntGauge(const std::string& name, uint64_t val = 0) : Gauge<uint64_t>(name, val) {}
    ~IntGauge() = default;

    IntGauge* Collect() { return new IntGauge(mName, mVal.load()); }
    void Add(uint64_t val) { mVal.fetch_add(val); }
    void Sub(uint64_t val) { mVal.fetch_sub(val); }
};

using CounterPtr = std::shared_ptr<Counter>;
using TimeCounterPtr = std::shared_ptr<TimeCounter>;
using IntGaugePtr = std::shared_ptr<IntGauge>;
using DoubleGaugePtr = std::shared_ptr<Gauge<double>>;

using MetricLabels = std::vector<std::pair<std::string, std::string>>;
using MetricLabelsPtr = std::shared_ptr<MetricLabels>;
using DynamicMetricLabels = std::vector<std::pair<std::string, std::function<std::string()>>>;
using DynamicMetricLabelsPtr = std::shared_ptr<DynamicMetricLabels>;

#define ADD_COUNTER(counterPtr, value) \
    if (counterPtr) { \
        (counterPtr)->Add(value); \
    }
#define SET_GAUGE(gaugePtr, value) \
    if (gaugePtr) { \
        (gaugePtr)->Set(value); \
    }
#define ADD_GAUGE(gaugePtr, value) \
    if (gaugePtr) { \
        (gaugePtr)->Add(value); \
    }
#define SUB_GAUGE(gaugePtr, value) \
    if (gaugePtr) { \
        (gaugePtr)->Sub(value); \
    }

} // namespace logtail
