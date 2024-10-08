#pragma once

#include <functional>

#include "common/Lock.h"

namespace logtail {

enum class PromFutureState { New, Processing, Done };

template <typename... Args>
class PromFuture {
public:
    using CallbackSignature = std::function<bool(Args...)>;
    // Process should support oneshot and streaming mode.
    bool Process(Args...);

    void AddDoneCallback(CallbackSignature&&);

    void Cancel();

protected:
    PromFutureState mState = {PromFutureState::New};
    ReadWriteLock mStateRWLock;

    std::vector<CallbackSignature> mDoneCallbacks;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ScrapeSchedulerUnittest;
#endif
};

} // namespace logtail