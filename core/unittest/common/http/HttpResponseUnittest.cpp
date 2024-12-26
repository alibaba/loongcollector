
#include "common/http/HttpResponse.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {
class HttpResponseUnittest : public ::testing::Test {
public:
    void TestNetworkStatus();
};

void HttpResponseUnittest::TestNetworkStatus() {
    HttpResponse resp;
    resp.SetNetworkStatus(CURLE_OK);
    APSARA_TEST_EQUAL(resp.GetNetworkStatus().mCode, NetworkCode::Ok);

    resp.SetNetworkStatus(CURLE_RECV_ERROR);
    APSARA_TEST_EQUAL(resp.GetNetworkStatus().mCode, NetworkCode::RecvDataFailed);

    resp.SetNetworkStatus(CURLE_FAILED_INIT);
    APSARA_TEST_EQUAL(resp.GetNetworkStatus().mCode, NetworkCode::Other);
}

UNIT_TEST_CASE(HttpResponseUnittest, TestNetworkStatus);
} // namespace logtail
UNIT_TEST_MAIN
