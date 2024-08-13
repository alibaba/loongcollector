#include "prometheus/schedulers/BaseScheduler.h"

namespace logtail {
void BaseScheduler::ExecDone() {
    mExecCount++;
}

std::chrono::steady_clock::time_point BaseScheduler::GetNextExecTime() {
    return mFirstExecTime + std::chrono::seconds(mExecCount * mInterval);
}

void BaseScheduler::SetFirstExecTime(std::chrono::steady_clock::time_point firstExecTime) {
    mFirstExecTime = firstExecTime;
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