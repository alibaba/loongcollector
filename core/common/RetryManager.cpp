// Copyright 2025 loongcollector Authors
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

#include "common/RetryManager.h"

#include "logger/Logger.h"

using namespace std;
namespace logtail {

void RetryManager::InitializeStrategy() {
    switch (mConfig.strategyType) {
        case BackoffStrategyType::FIXED:
            mStrategy = std::make_unique<FixedBackoffStrategy>();
            break;
        case BackoffStrategyType::LINEAR:
            mStrategy = std::make_unique<LinearBackoffStrategy>();
            break;
        case BackoffStrategyType::EXPONENTIAL:
            mStrategy = std::make_unique<ExponentialBackoffStrategy>();
            break;
        case BackoffStrategyType::CUSTOM:
            mStrategy = std::make_unique<FixedBackoffStrategy>();
            break;
        default:
            mStrategy = std::make_unique<ExponentialBackoffStrategy>();
            break;
    }
}

bool RetryManager::ShouldRetry() const {
    std::lock_guard<std::mutex> lock(mMutex);
    int64_t currentTime = std::time(nullptr);
    return currentTime >= mNextRetryTime;
}

void RetryManager::RecordFailure() {
    std::lock_guard<std::mutex> lock(mMutex);
    int64_t currentTime = std::time(nullptr);
    mLastFailureTime = currentTime;
    mFailureCount++;

    int64_t retryInterval = CalculateRetryInterval();
    mNextRetryTime = currentTime + retryInterval;

    LOG_INFO(sLogger,
             ("RetryManager: operation failed", "")("failure count", mFailureCount)(
                 "strategy", static_cast<int>(mConfig.strategyType))("next retry in", retryInterval)("next retry time",
                                                                                                     mNextRetryTime));
}

void RetryManager::ResetFailureState() {
    std::lock_guard<std::mutex> lock(mMutex);
    mFailureCount = 0;
    mLastFailureTime = 0;
    mNextRetryTime = 0;

    LOG_INFO(sLogger, ("RetryManager: failure state reset", ""));
}

void RetryManager::UpdateConfig(const RetryConfig& config) {
    std::lock_guard<std::mutex> lock(mMutex);
    mConfig = config;
    InitializeStrategy();
}

void RetryManager::SetCustomStrategy(const CustomBackoffStrategy::CustomFunction& func) {
    std::lock_guard<std::mutex> lock(mMutex);
    mStrategy = std::make_unique<CustomBackoffStrategy>(func);
    mConfig.strategyType = BackoffStrategyType::CUSTOM;
}

int64_t RetryManager::CalculateRetryInterval() const {
    if (!mStrategy) {
        return mConfig.baseRetryInterval;
    }

    return mStrategy->CalculateInterval(mFailureCount, mConfig.baseRetryInterval, mConfig.maxRetryInterval);
}

} // namespace logtail
