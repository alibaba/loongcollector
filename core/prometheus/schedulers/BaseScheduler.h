#pragma once


#include <memory>

#include "prometheus/async/PromFuture.h"

namespace logtail {
class BaseScheduler {
public:
    BaseScheduler() = default;
    virtual ~BaseScheduler() = default;

    virtual void ScheduleNext() = 0;

    void ExecDone();

    std::chrono::steady_clock::time_point GetNextExecTime();

    void SetFirstExecTime(std::chrono::steady_clock::time_point firstExecTime);

    virtual void Cancel();

protected:
    bool IsCancelled();

    std::chrono::steady_clock::time_point mFirstExecTime;
    int64_t mExecCount = 0;
    int64_t mInterval = 0;

    ReadWriteLock mLock;
    bool mValidState = true;
    std::shared_ptr<PromFuture> mFuture;
};
} // namespace logtail