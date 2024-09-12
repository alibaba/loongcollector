#pragma once

#include "common/Lock.h"
#include "common/http/HttpResponse.h"

namespace logtail {

enum class PromFutureState { New, Processing, Done };

class PromFuture {
public:
    // Process should support oneshot and streaming mode.
    void Process(const HttpResponse&, uint64_t timestampMilliSec);
    bool PreCheck();

    void AddDoneCallback(std::function<void(const HttpResponse&, uint64_t timestampMilliSec)>&& callback);
    void AddPreCheckCallback(std::function<bool()>&& callback);

    void Cancel();

protected:
    PromFutureState mState = {PromFutureState::New};
    ReadWriteLock mStateRWLock;

    std::vector<std::function<void(const HttpResponse&, uint64_t timestampMilliSec)>> mDoneCallbacks;
    std::vector<std::function<bool()>> mPreCheckCallbacks;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ScrapeSchedulerUnittest;
#endif
};

} // namespace logtail