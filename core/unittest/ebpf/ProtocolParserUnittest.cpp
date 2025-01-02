#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "logger/Logger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class ProtocolParserUnittest : public testing::Test {
public:
    ProtocolParserUnittest() {}

    // void TestBasicAgg();

protected:
    void SetUp() override {
        
    }
    void TearDown() override {}

private:
};

// UNIT_TEST_CASE(ProtocolParserUnittest, TestBasicAgg);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
