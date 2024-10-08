#pragma once


#include <memory>

#include "common/http/HttpResponse.h"
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
    void DelayExecTime(uint64_t delaySeconds);

    virtual void Cancel();

protected:
    bool IsCancelled();

    std::chrono::steady_clock::time_point mFirstExecTime;
    std::chrono::steady_clock::time_point mLatestExecTime;
    int64_t mExecCount = 0;
    int64_t mInterval = 0;

    ReadWriteLock mLock;
    bool mValidState = true;
    std::shared_ptr<PromFuture<const HttpResponse&, uint64_t>> mFuture;
    std::shared_ptr<PromFuture<>> mIsContextValidFuture;
};
} // namespace logtail