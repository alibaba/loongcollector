#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "logger/Logger.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class ConvergerUnittest : public testing::Test {
public:
    ConvergerUnittest() {}

    // void TestBasicAgg();
    // void TestMove();

protected:
    void SetUp() override {}
    void TearDown() override {}

private:
};

// UNIT_TEST_CASE(ConvergerUnittest, TestBasicAgg);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
