#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "logger/Logger.h"
#include "unittest/Unittest.h"
#include "ebpf/type/table/AppTable.h"

DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class TableUnittest : public testing::Test {
public:
    TableUnittest() {}

    // void TestBasicAgg();

protected:
    void SetUp() override {
        
    }
    void TearDown() override {}

private:
};

// UNIT_TEST_CASE(TableUnittest, TestBasicAgg);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
