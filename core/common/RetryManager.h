/*
 * Copyright 2025 loongcollector Authors
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

#include <ctime>

#include <functional>
#include <memory>
#include <mutex>

namespace logtail {

enum class BackoffStrategyType { FIXED, LINEAR, EXPONENTIAL, CUSTOM };

class BackoffStrategy {
public:
    virtual ~BackoffStrategy() = default;

    [[nodiscard]] virtual int64_t
    CalculateInterval(int32_t failureCount, int64_t baseInterval, int64_t maxInterval) const
        = 0;
};

class FixedBackoffStrategy : public BackoffStrategy {
public:
    [[nodiscard]] int64_t
    CalculateInterval(int32_t failureCount, int64_t baseInterval, int64_t maxInterval) const override {
        return baseInterval;
    }
};


class LinearBackoffStrategy : public BackoffStrategy {
public:
    [[nodiscard]] int64_t
    CalculateInterval(int32_t failureCount, int64_t baseInterval, int64_t maxInterval) const override {
        int64_t interval = baseInterval * failureCount;
        return interval > maxInterval ? maxInterval : interval;
    }
};

class ExponentialBackoffStrategy : public BackoffStrategy {
public:
    [[nodiscard]] int64_t
    CalculateInterval(int32_t failureCount, int64_t baseInterval, int64_t maxInterval) const override {
        int64_t interval = baseInterval;
        for (int32_t i = 1; i < failureCount; ++i) {
            interval *= 2;
        }
        return interval > maxInterval ? maxInterval : interval;
    }
};

class CustomBackoffStrategy : public BackoffStrategy {
public:
    using CustomFunction = std::function<int64_t(int32_t, int64_t, int64_t)>;

    explicit CustomBackoffStrategy(CustomFunction func) : mCustomFunc(std::move(func)) {}

    [[nodiscard]] int64_t
    CalculateInterval(int32_t failureCount, int64_t baseInterval, int64_t maxInterval) const override {
        return mCustomFunc ? mCustomFunc(failureCount, baseInterval, maxInterval) : baseInterval;
    }

private:
    CustomFunction mCustomFunc;
};

class RetryManager {
public:
    struct RetryConfig {
        int64_t baseRetryInterval;
        int64_t maxRetryInterval;
        BackoffStrategyType strategyType;

        RetryConfig() : baseRetryInterval(1), maxRetryInterval(1800), strategyType(BackoffStrategyType::EXPONENTIAL) {}
    };

    explicit RetryManager(const RetryConfig& config = RetryConfig{}) : mConfig(config) { InitializeStrategy(); }

    [[nodiscard]] bool ShouldRetry() const;

    void RecordFailure();

    void ResetFailureState();

    [[nodiscard]] int32_t GetFailureCount() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mFailureCount;
    }

    [[nodiscard]] int64_t GetNextRetryTime() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mNextRetryTime;
    }

    void UpdateConfig(const RetryConfig& config);

    void SetCustomStrategy(const CustomBackoffStrategy::CustomFunction& func);

private:
    RetryConfig mConfig;

    int64_t mLastFailureTime = 0;
    int32_t mFailureCount = 0;
    int64_t mNextRetryTime = 0;

    std::unique_ptr<BackoffStrategy> mStrategy;
    mutable std::mutex mMutex;

    void InitializeStrategy();

    [[nodiscard]] int64_t CalculateRetryInterval() const;
};

} // namespace logtail
