#pragma once


#include <memory>

#include "common/http/HttpResponse.h"
#include "common/timer/Timer.h"
#include "models/EventPool.h"
#include "prometheus/async/PromFuture.h"

namespace logtail {
class BaseScheduler {
public:
    BaseScheduler() = default;
    virtual ~BaseScheduler() = default;

    virtual void ScheduleNext() = 0;

    void ExecDone();

    std::chrono::steady_clock::time_point GetNextExecTime();

    void SetFirstExecTime(std::chrono::steady_clock::time_point firstExecTime,std::chrono::system_clock::time_point firstScrapeTime);
    void DelayExecTime(uint64_t delaySeconds);
    virtual void Cancel();

    void SetComponent(std::shared_ptr<Timer> timer, EventPool* eventPool);

protected:
    bool IsCancelled();

    // for scrape monitor
    std::chrono::system_clock::time_point mFirstScrapeTime;
    std::chrono::system_clock::time_point mLatestScrapeTime;

    // for scheduler
    std::chrono::steady_clock::time_point mFirstExecTime;
    std::chrono::steady_clock::time_point mLatestExecTime;
    int64_t mExecCount = 0;
    int64_t mInterval = 0;

    ReadWriteLock mLock;
    bool mValidState = true;
    std::shared_ptr<PromFuture<HttpResponse&, uint64_t>> mFuture;
    std::shared_ptr<PromFuture<>> mIsContextValidFuture;

    std::shared_ptr<Timer> mTimer;
    EventPool* mEventPool = nullptr;
};
} // namespace logtail