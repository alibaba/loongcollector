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

    void TestBasic();
    void TestAppTable();
    void TestConnStatsTable();
    void TestFileSecurityTable();
    void TestNetworkSecurityTable();
    void TestProcessSecurityTable();

protected:
    void SetUp() override {
        
    }
    void TearDown() override {}

private:
};

void TableUnittest::TestBasic() {
    // col idx

    // col name
}
void TableUnittest::TestAppTable() {
    kAppMetricsTable;
    kAppTraceTable;
}
void TableUnittest::TestConnStatsTable() {

}
void TableUnittest::TestFileSecurityTable() {
    

}
void TableUnittest::TestNetworkSecurityTable() {

}
void TableUnittest::TestProcessSecurityTable() {

}

UNIT_TEST_CASE(TableUnittest, TestBasic);
UNIT_TEST_CASE(TableUnittest, TestAppTable);
UNIT_TEST_CASE(TableUnittest, TestConnStatsTable);
UNIT_TEST_CASE(TableUnittest, TestFileSecurityTable);
UNIT_TEST_CASE(TableUnittest, TestNetworkSecurityTable);
UNIT_TEST_CASE(TableUnittest, TestProcessSecurityTable);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
