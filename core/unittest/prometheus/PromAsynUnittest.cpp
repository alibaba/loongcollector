
#include "common/http/HttpResponse.h"
#include "prometheus/async/PromFuture.h"
#include "prometheus/async/PromHttpRequest.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class PromAsynUnittest : public testing::Test {
public:
    void TestExecTime();
};

void PromAsynUnittest::TestExecTime() {
    auto future = std::make_shared<PromFuture<HttpResponse&, uint64_t>>();
    auto now = std::chrono::system_clock::now();
    bool exec = false;
    future->AddDoneCallback([&exec, now](HttpResponse&, uint64_t timestampMilliSec) {
        APSARA_TEST_EQUAL(timestampMilliSec,
                          std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count());

        APSARA_TEST_TRUE(exec);
        return true;
    });
    auto request = std::make_shared<PromHttpRequest>(
        "http", false, "127.0.0.1", 8080, "/", "", map<string, string>(), "", HttpResponse(), 10, 3, future);
    auto asynRequest = std::dynamic_pointer_cast<AsynHttpRequest>(request);
    asynRequest->mLastSendTime = now;
    auto response = HttpResponse{};
    exec = true;
    asynRequest->OnSendDone(response);
}

UNIT_TEST_CASE(PromAsynUnittest, TestExecTime);

} // namespace logtail

UNIT_TEST_MAIN