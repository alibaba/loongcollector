#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "logger/Logger.h"
#include "unittest/Unittest.h"

#include "ebpf/util/AggregateTree.h"

DECLARE_FLAG_BOOL(logtail_mode);

class HT {
   public:
    std::unordered_map<int, std::string> tt;
    // val是数量
    int val = 0;

    explicit HT(int val) : val(val) {};
};

namespace logtail {
namespace ebpf {
class AggregatorUnittest : public testing::Test {
public:
    AggregatorUnittest() {}

    void TestBasicAgg();
    void TestMove();

protected:
    void SetUp() override {
        agg = std::make_unique<SIZETAggTree<HT, std::vector<std::string>>>(
            10, 
            [](std::unique_ptr<HT> &base, const std::vector<std::string> &other) {
                APSARA_TEST_TRUE(base != nullptr);
                size_t i = 0;
                for (auto &key : other) {
                    base->tt[i++] = key;
                }
                base->val++;
            }, [](const std::vector<std::string> & in) {
                // LOG_INFO(sLogger, ("enter generate ... ", ""));
                return std::make_unique<HT>(0);
            });
    }
    void TearDown() override {}

    int GetSum() { return GetSum(agg); }

    static int GetSum(const std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> &agg) {
        int result = 0;
        agg->ForEach([&result](const auto ht) { result += ht->val; });
        return result;
    }

    int GetDataNodeCount() { return GetDataNodeCount(agg); }

    inline size_t GetHashByDepth(const std::vector<std::string> & data, int depth) {
        size_t seed = 0UL;
        for(int i=0; i<depth; i++) {
            seed ^= std::hash<std::string>{}(data[i])+ 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
    bool Aggregate(const std::vector<std::string> & data, int depth) {
        return agg->Aggregate(data, std::array<size_t, 1>{GetHashByDepth(data, depth)});
    }

    static int GetDataNodeCount(const std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> &agg) {
        int node_count = 0;
        agg->ForEach([&node_count](const auto ht) { node_count++; });
        return node_count;
    }

private:

    std::unique_ptr<SIZETAggTree<HT, std::vector<std::string>>> agg;
};

void AggregatorUnittest::TestBasicAgg() {
    Aggregate({"a", "b", "c", "d"}, 4);
    Aggregate({"a", "b", "c", "d", "e"}, 4);
    Aggregate({"a", "b", "d", "r"}, 4);
    Aggregate({"a", "b", "c", "e"}, 4);
    Aggregate({"a", "b", "c"}, 3);
    APSARA_TEST_EQUAL(GetDataNodeCount(), 4);
    APSARA_TEST_EQUAL(agg->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(), 5);
    agg->Clear();
    APSARA_TEST_EQUAL(GetDataNodeCount(), 0);
    APSARA_TEST_EQUAL(agg->NodeCount(), 0);
    APSARA_TEST_EQUAL(GetSum(), 0);
}

void AggregatorUnittest::TestMove() {
    Aggregate({"a", "b", "c", "d"}, 4);
    Aggregate({"a", "b", "c", "d", "e"}, 4);
    Aggregate({"a", "b", "d", "r"}, 4);
    Aggregate({"a", "b", "c", "e"}, 4);
    Aggregate({"a", "b", "c"}, 3);
    APSARA_TEST_EQUAL(GetDataNodeCount(), 4);
    APSARA_TEST_EQUAL(agg->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(), 5);
    auto moved_map = std::make_unique<SIZETAggTree<HT, std::vector<std::string>>>(std::move(*agg));
    APSARA_TEST_EQUAL(GetDataNodeCount(), 0);
    APSARA_TEST_EQUAL(agg->NodeCount(), 0);
    APSARA_TEST_EQUAL(GetSum(), 0);

    APSARA_TEST_EQUAL(GetDataNodeCount(moved_map), 4);
    APSARA_TEST_EQUAL(moved_map->NodeCount(), 4);
    APSARA_TEST_EQUAL(GetSum(moved_map), 5);
}

UNIT_TEST_CASE(AggregatorUnittest, TestBasicAgg);
UNIT_TEST_CASE(AggregatorUnittest, TestMove);

} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
