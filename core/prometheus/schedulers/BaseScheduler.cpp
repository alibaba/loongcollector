#include "prometheus/schedulers/BaseScheduler.h"

#include "common/timer/Timer.h"
#include "models/EventPool.h"

using namespace std;

namespace logtail {
void BaseScheduler::ExecDone() {
    mExecCount++;
    mLatestExecTime = mFirstExecTime + std::chrono::seconds(mExecCount * mInterval);
}

std::chrono::steady_clock::time_point BaseScheduler::GetNextExecTime() {
    return mLatestExecTime;
}

void BaseScheduler::SetFirstExecTime(std::chrono::steady_clock::time_point firstExecTime) {
    mFirstExecTime = firstExecTime;
    mLatestExecTime = mFirstExecTime;
}

void BaseScheduler::DelayExecTime(uint64_t delaySeconds) {
    mLatestExecTime = mLatestExecTime + std::chrono::seconds(delaySeconds);
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