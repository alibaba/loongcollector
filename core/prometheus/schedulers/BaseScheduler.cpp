#include "prometheus/schedulers/BaseScheduler.h"

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
} // namespace logtail