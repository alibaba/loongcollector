#include "prometheus/async/PromHttpRequest.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include "common/http/HttpRequest.h"

namespace logtail {

PromHttpRequest::PromHttpRequest(const std::string& method,
                                 bool httpsFlag,
                                 const std::string& host,
                                 int32_t port,
                                 const std::string& url,
                                 const std::string& query,
                                 const std::map<std::string, std::string>& header,
                                 const std::string& body,
                                 HttpResponse&& response,
                                 uint32_t timeout,
                                 uint32_t maxTryCnt,
                                 std::shared_ptr<PromFuture<HttpResponse&, uint64_t>> future,
                                 std::shared_ptr<PromFuture<>> isContextValidFuture,
                                 bool followRedirects,
                                 std::optional<CurlTLS> tls)
    : AsynHttpRequest(method,
                      httpsFlag,
                      host,
                      port,
                      url,
                      query,
                      header,
                      body,
                      std::move(response),
                      timeout,
                      maxTryCnt,
                      followRedirects,
                      std::move(tls)),
      mFuture(std::move(future)),
      mIsContextValidFuture(std::move(isContextValidFuture)) {
}

void PromHttpRequest::OnSendDone(HttpResponse& response) {
    if (mFuture != nullptr) {
        mFuture->Process(
            response, std::chrono::duration_cast<std::chrono::milliseconds>(mLastSendTime.time_since_epoch()).count());
    }
}

[[nodiscard]] bool PromHttpRequest::IsContextValid() const {
    if (mIsContextValidFuture != nullptr) {
        return mIsContextValidFuture->Process();
    }
    return true;
}

} // namespace logtail