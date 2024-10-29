#include "prometheus/async/PromHttpRequest.h"

#include <chrono>
#include <cstdint>
#include <string>

#include "common/http/HttpRequest.h"
#include "prometheus/Constants.h"

namespace logtail {

// size_t PromWriteCallback(char* buffer, size_t size, size_t nmemb, void* data) {
//     unsigned long sizes = size * nmemb;

//     if (buffer == nullptr) {
//         return 0;
//     }

//     PromResponseBody* body = static_cast<PromResponseBody*>(data);

//     size_t begin = 0;
//     while (begin < sizes) {
//         for (size_t end = begin; end < sizes; ++end) {
//             if (buffer[end] == '\n') {
//                 if (begin == 0) {
//                     body->mCache.append(buffer, end);
//                     if (!body->mCache.empty()) {
//                         auto e = body->mEventGroup.AddLogEvent();
//                         auto sb = body->mEventGroup.GetSourceBuffer()->CopyString(body->mCache);
//                         body->mCache.clear();
//                         e->SetContentNoCopy(prometheus::PROMETHEUS, StringView(sb.data, sb.size));
//                     }
//                 } else if (begin != end) {
//                     auto e = body->mEventGroup.AddLogEvent();
//                     auto sb = body->mEventGroup.GetSourceBuffer()->CopyString(buffer + begin, end - begin);
//                     e->SetContentNoCopy(prometheus::PROMETHEUS, StringView(sb.data, sb.size));
//                 }
//                 begin += end - begin + 1;
//                 continue;
//             }
//         }
//         break;
//     }
//     if (begin < sizes) {
//         body->mCache.append(buffer + begin, sizes - begin);
//     }
//     body->mRawSize += sizes;
//     return sizes;
// }

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
                                 std::shared_ptr<PromFuture<HttpResponse&, uint64_t>> future,
                                 std::shared_ptr<PromFuture<>> isContextValidFuture)
    : AsynHttpRequest(method,
                      httpsFlag,
                      host,
                      port,
                      url,
                      query,
                      header,
                      body,
                      //   HttpResponse(
                      //       new PromResponseBody(), [](void* ptr) { delete static_cast<PromResponseBody*>(ptr); },
                      //       PromWriteCallback),
                      HttpResponse(),
                      timeout,
                      maxTryCnt),
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