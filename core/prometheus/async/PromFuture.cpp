#include "prometheus/async/PromFuture.h"

#include "common/Lock.h"
#include "common/http/HttpResponse.h"

namespace logtail {

template <typename... Args>
bool PromFuture<Args...>::Process(Args... args) {
    WriteLock lock(mStateRWLock);
    if (mState == PromFutureState::New) {
        for (auto& callback : mDoneCallbacks) {
            if (!callback(std::forward<Args>(args)...)) {
                mState = PromFutureState::Done;
                return false;
            }
        }
        mState = PromFutureState::Done;
    }

    return true;
}

template <typename... Args>
void PromFuture<Args...>::AddDoneCallback(CallbackSignature&& callback) {
    mDoneCallbacks.emplace_back(std::move(callback));
}

template <typename... Args>
void PromFuture<Args...>::Cancel() {
    WriteLock lock(mStateRWLock);
    mState = PromFutureState::Done;
}

template class PromFuture<const HttpResponse&, uint64_t>;
template class PromFuture<>;

} // namespace logtail