#pragma once

#include <cstdint>
#include <string>

#include "common/http/HttpRequest.h"
#include "prometheus/async/PromFuture.h"

namespace logtail {

class PromHttpRequest : public AsynHttpRequest {
public:
    PromHttpRequest(const std::string& method,
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
                    std::shared_ptr<PromFuture<>> isContextValidFuture = nullptr,
                    bool followRedirects = false,
                    std::optional<CurlTLS> tls = std::nullopt);
    PromHttpRequest(const PromHttpRequest&) = default;
    ~PromHttpRequest() override = default;

    void OnSendDone(HttpResponse& response) override;
    [[nodiscard]] bool IsContextValid() const override;

private:
    void SetNextExecTime(std::chrono::steady_clock::time_point execTime);

    std::shared_ptr<PromFuture<HttpResponse&, uint64_t>> mFuture;
    std::shared_ptr<PromFuture<>> mIsContextValidFuture;
};

} // namespace logtail