
#include "models/StringView.h"
#include "prometheus/Utils.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

bool IsDoubleEqual(double a, double b) {
    return fabs(a - b) < 0.000001;
}

class PromUtilsUnittest : public testing::Test {
public:
    void TestDurationToSecond();
    void TestSecondToDuration();
    void TestSizeToByte();
    void TestNetworkCodeToString();
};

void PromUtilsUnittest::TestDurationToSecond() {
    string rawData = "30s";
    APSARA_TEST_EQUAL(30ULL, DurationToSecond(rawData));
    rawData = "1m";
    APSARA_TEST_EQUAL(60ULL, DurationToSecond(rawData));
    rawData = "xxxs";
    APSARA_TEST_EQUAL(0ULL, DurationToSecond(rawData));
}

void PromUtilsUnittest::TestSecondToDuration() {
    APSARA_TEST_EQUAL("30s", SecondToDuration(30ULL));
    APSARA_TEST_EQUAL("1m", SecondToDuration(60ULL));
    APSARA_TEST_EQUAL("90s", SecondToDuration(90ULL));
}

void PromUtilsUnittest::TestSizeToByte() {
    APSARA_TEST_EQUAL(1025ULL, SizeToByte("1025B"));
    APSARA_TEST_EQUAL(1024ULL, SizeToByte("1K"));
    APSARA_TEST_EQUAL(1024ULL, SizeToByte("1KiB"));
    APSARA_TEST_EQUAL(1024ULL, SizeToByte("1KB"));

    APSARA_TEST_EQUAL(1024ULL * 1024ULL, SizeToByte("1M"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL, SizeToByte("1MiB"));
    APSARA_TEST_EQUAL(2 * 1024ULL * 1024ULL, SizeToByte("2MB"));

    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL, SizeToByte("1G"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL, SizeToByte("1GiB"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL, SizeToByte("1GB"));

    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1T"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1TiB"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1TB"));

    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1P"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1PiB"));
    APSARA_TEST_EQUAL(1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL, SizeToByte("1PB"));

    APSARA_TEST_EQUAL(0ULL, SizeToByte("1xxE"));
}

void PromUtilsUnittest::TestNetworkCodeToString() {
    APSARA_TEST_EQUAL("OK", prom::NetworkCodeToString(NetworkCode::Ok));
    APSARA_TEST_EQUAL("ERR_UNKNOWN", prom::NetworkCodeToString(NetworkCode::Other));
    APSARA_TEST_EQUAL("ERR_UNKNOWN", prom::NetworkCodeToString((NetworkCode)123));
}

UNIT_TEST_CASE(PromUtilsUnittest, TestDurationToSecond);
UNIT_TEST_CASE(PromUtilsUnittest, TestSecondToDuration);
UNIT_TEST_CASE(PromUtilsUnittest, TestSizeToByte);
UNIT_TEST_CASE(PromUtilsUnittest, TestNetworkCodeToString);

} // namespace logtail

UNIT_TEST_MAIN
