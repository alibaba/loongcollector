#include "prometheus/schedulers/BaseScheduler.h"

#include "common/timer/Timer.h"
#include "models/EventPool.h"

using namespace std;

namespace logtail {
void BaseScheduler::ExecDone() {
    mExecCount++;
    mLatestExecTime = mFirstExecTime + chrono::seconds(mExecCount * mInterval);
    mLatestScrapeTime = mFirstScrapeTime + chrono::seconds(mExecCount * mInterval);
}

chrono::steady_clock::time_point BaseScheduler::GetNextExecTime() {
    return mLatestExecTime;
}

void BaseScheduler::SetFirstExecTime(chrono::steady_clock::time_point firstExecTime,
                                     chrono::system_clock::time_point firstScrapeTime) {
    mFirstExecTime = firstExecTime;
    mLatestExecTime = mFirstExecTime;
    mFirstScrapeTime = firstScrapeTime;
    mLatestScrapeTime = mFirstScrapeTime;
}

void BaseScheduler::DelayExecTime(uint64_t delaySeconds) {
    mLatestExecTime = mLatestExecTime + chrono::seconds(delaySeconds);
    mLatestScrapeTime = mLatestScrapeTime + chrono::seconds(delaySeconds);
}

void BaseScheduler::Cancel() {
    WriteLock lock(mLock);
    mValidState = false;
}

bool BaseScheduler::IsCancelled() {
    ReadLock lock(mLock);
    return !mValidState;
}

void BaseScheduler::SetComponent(shared_ptr<Timer> timer, EventPool* eventPool) {
    mTimer = std::move(timer);
    mEventPool = eventPool;
}
} // namespace logtail