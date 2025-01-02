#include <json/json.h>

#include <algorithm>
#include <iostream>
#include <random>

#include "logger/Logger.h"
#include "unittest/Unittest.h"
#include "ebpf/util/sampler/Sampler.h"
#include "ebpf/util/TraceId.h"


DECLARE_FLAG_BOOL(logtail_mode);

namespace logtail {
namespace ebpf {
class SamplerUnittest : public testing::Test {
public:
    SamplerUnittest() {}

    void TestRandFromSpanID();
    void TestSampleAll();


protected:
    void SetUp() override {
    }
    void TearDown() override {}

private:
    std::unique_ptr<HashRatioSampler> sampler_;
};


std::array<uint8_t, 16> GenerateSpanID() {
    std::random_device rd;
    std::mt19937_64 generator(rd());
    std::uniform_int_distribution<uint64_t> distribution(0, std::numeric_limits<uint64_t>::max());

    auto result = std::array<uint8_t, 16>();
    auto buf_size = result.size();

    for (size_t i = 0; i < buf_size; i += sizeof(uint64_t)) {
        uint64_t value = distribution(generator);

        if (i + sizeof(uint64_t) <= buf_size) {
            memcpy(&result[i], &value, sizeof(uint64_t));
        } else {
            memcpy(&result[i], &value, buf_size - i);
        }
    }
    return result;
}

void SamplerUnittest::TestRandFromSpanID() {
    sampler_ = std::make_unique<HashRatioSampler>(0.01);
    int totalCount = 0;
    int sampledCount = 0;
    for (int i = 0; i < 1000000; i++) {
        auto id = GenerateSpanID();
        bool result = sampler_->ShouldSample(id);
        totalCount++;
        if (result) {
            sampledCount++;
        }
    }
    double realPortion = double(sampledCount) / double(totalCount);
    ASSERT_GE(realPortion, 0.009);
    ASSERT_LE(realPortion, 0.011);
    APSARA_TEST_GE(realPortion, 0.009);
    APSARA_TEST_LE(realPortion, 0.011);

}
void SamplerUnittest::TestSampleAll() {
    sampler_ = std::make_unique<HashRatioSampler>(1);
    for (int i = 0; i < 1000000; i++) {
        auto id = GenerateSpanID();
        APSARA_TEST_TRUE(sampler_->ShouldSample(id));
    }
}

UNIT_TEST_CASE(SamplerUnittest, TestRandFromSpanID);
UNIT_TEST_CASE(SamplerUnittest, TestSampleAll);


} // namespace ebpf
} // namespace logtail

UNIT_TEST_MAIN
