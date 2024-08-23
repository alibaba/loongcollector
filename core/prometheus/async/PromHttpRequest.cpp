#include "prometheus/async/PromHttpRequest.h"

#include <cstdint>
#include <string>

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
                                 uint32_t timeout,
                                 uint32_t maxTryCnt,
                                 std::shared_ptr<PromFuture> future)
    : AsynHttpRequest(method, httpsFlag, host, port, url, query, header, body, timeout, maxTryCnt),
      mFuture(std::move(future)) {
}

void PromHttpRequest::OnSendDone(const HttpResponse& response) {
    mFuture->Process(response, mLastSendTime * 1000);
}

[[nodiscard]] bool PromHttpRequest::IsContextValid() const {
    return true;
}

} // namespace logtail