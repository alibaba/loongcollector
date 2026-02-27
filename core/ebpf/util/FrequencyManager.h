//
// Created by qianlu on 2024/7/2.
//

#pragma once

#include <chrono>

namespace logtail::ebpf {
/**
 * Manages the frequency of periodical action.
 */
class FrequencyManager {
    using time_point = std::chrono::steady_clock::time_point;

public:
    /**
     * Returns true if the current cycle has expired.
     */
    [[nodiscard]] bool Expired(const time_point now) const { return now >= mNext; }

    /**
     * Ends the current cycle, and starts the next one.
     */
    void Reset(const time_point now) {
        mNext = now + mPeriod;
        ++mCount;
    }

    void SetPeriod(std::chrono::milliseconds period) { mPeriod = period; }
    [[nodiscard]] const auto& Period() const { return mPeriod; }
    [[nodiscard]] const auto& Next() const { return mNext; }
    [[nodiscard]] uint32_t Count() const { return mCount; }

private:
    // The cycle's period.
    std::chrono::milliseconds mPeriod = {};

    // When the current cycle should end.
    std::chrono::steady_clock::time_point mNext;

    // The count of expired cycle so far.
    uint32_t mCount = 0;
};
} // namespace logtail::ebpf
