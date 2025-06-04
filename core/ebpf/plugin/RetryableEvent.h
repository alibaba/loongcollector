// Copyright 2025 iLogtail Authors
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

#include <cstdint>
namespace logtail::ebpf {

/**
 * @class RetryableEvent
 * @brief A base class for events that can be retried.
 *
 * This class provides a framework for handling events that may need to be
 * retried a certain number of times before being considered failed.
 */
class RetryableEvent {
public:
    /**
     * @brief Constructor for RetryableEvent.
     * @param retryLimit The initial number of retry attempts allowed.
     */
    explicit RetryableEvent(int retryLimit) : mRetryLeft(retryLimit) {}

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~RetryableEvent() = default;

    /**
     * @brief Handles the event message. The framework will call OnRetry() later if the message was not handled
     * successfully.
     * @return true if the message was handled successfully, false otherwise.
     */
    virtual bool HandleMessage() = 0;

    /**
     * @brief Retries the event message. The framework will retry up to the maximum number of retry attempts.
     * @return true if the message was retried successfully, false otherwise.
     */
    virtual bool OnRetry() = 0;

    /**
     * @brief Called when the event is dropped due to exhausting all retry attempts.
     */
    virtual void OnDrop() = 0;

    /**
     * @brief Checks if the event can be retried.
     * @return true if there are retry attempts left, false otherwise.
     */
    [[nodiscard]] bool CanRetry() const { return mRetryLeft > 0; }

    /**
     * @brief Decrements the number of retry attempts left.
     */
    void DecrementRetryCount() {
        if (mRetryLeft > 0) {
            --mRetryLeft;
        }
    }

    /**
     * @brief Gets the number of retry attempts left.
     * @return The number of retry attempts left.
     */
    [[nodiscard]] int GetRetryLeft() const { return mRetryLeft; }

    [[nodiscard]] bool IsTaskCompleted(int taskId) const { return mCompletedTasks & taskIdToBit(taskId); }

    void CompleteTask(int taskId) { mCompletedTasks |= taskIdToBit(taskId); }

    [[nodiscard]] bool AreAllPreviousTasksCompleted(int taskId) const {
        return (mCompletedTasks & (taskIdToBit(taskId) - 1)) == (taskIdToBit(taskId) - 1);
    }

    template <typename... Args>
    bool AreTasksComplete(Args... taskIds) const {
        return (IsTaskCompleted(taskIds) && ...);
    }

protected:
    static constexpr uint32_t taskIdToBit(int taskId) { return 1U << taskId; }

    int mRetryLeft = 0; ///< The number of retry attempts left.
    uint32_t mCompletedTasks = 0;
};

} // namespace logtail::ebpf
