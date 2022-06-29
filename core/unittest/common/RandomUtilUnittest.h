/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "unittest/Unittest.h"
#include "common/RandomUtil.h"
#include <unordered_set>

namespace logtail {

class RandomUtilUnittest : public ::testing::Test {
public:
    void TestGenerateRandomHashKey();

    void TestCountBitSize();

    void TestGenerateHashKey();
};

APSARA_UNIT_TEST_CASE(RandomUtilUnittest, TestGenerateRandomHashKey, 0);
APSARA_UNIT_TEST_CASE(RandomUtilUnittest, TestCountBitSize, 0);
APSARA_UNIT_TEST_CASE(RandomUtilUnittest, TestGenerateHashKey, 0);

void RandomUtilUnittest::TestGenerateRandomHashKey() {
    std::unordered_set<std::string> keys;
    for (int i = 0; i < 1000; ++i) {
        auto key = GenerateRandomHashKey();
        EXPECT_EQ(key.size(), 32);
        EXPECT_TRUE(keys.find(key) == keys.end());
        keys.insert(key);
    }
    std::cout << *(keys.begin()) << std::endl;
}

namespace detail {
    size_t countBitSize(size_t maxVal);
}

void RandomUtilUnittest::TestCountBitSize() {
    auto testCountBitSize = [](size_t begin, size_t end, size_t result) {
        for (size_t val = begin; val <= end; ++val) {
            EXPECT_EQ(result, detail::countBitSize(val));
        }
    };

    testCountBitSize(0, 1, 1);
    testCountBitSize(2, 3, 2);
    testCountBitSize(4, 7, 3);
    testCountBitSize(8, 15, 4);
    testCountBitSize(16, 31, 5);
    testCountBitSize(32, 63, 6);
    testCountBitSize(64, 127, 7);
    testCountBitSize(128, 255, 8);
    testCountBitSize(256, 511, 9);
    testCountBitSize(512, 1023, 10);
}

void RandomUtilUnittest::TestGenerateHashKey() {
    const std::string kBaseHashKey = "FFB0";

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 8), "1FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 5, 8), "BFB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 7, 8), "FFB0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 11), "0FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 5, 11), "5FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 10, 11), "AFB0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 16), "0FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 5, 16), "5FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 10, 16), "AFB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 15, 16), "FFB0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 20), "07B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 5, 20), "2FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 10, 20), "57B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 18, 20), "97B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 50), "03B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 5, 50), "17B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 10, 50), "2BB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 25, 50), "67B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 32, 50), "83B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 48, 50), "C3B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 49, 50), "C7B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 100), "01B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 80, 100), "A1B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 99, 100), "C7B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 200), "00B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 52, 200), "34B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 109, 200), "6DB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 199, 200), "C7B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 300), "0030");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 49, 300), "18B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 129, 300), "40B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 259, 300), "81B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 299, 300), "95B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 400), "0030");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 63, 400), "1FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 127, 400), "3FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 259, 400), "81B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 300, 400), "9630");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 399, 400), "C7B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 500), "0030");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 63, 500), "1FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 127, 500), "3FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 259, 500), "81B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 300, 500), "9630");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 399, 500), "C7B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 499, 500), "F9B0");

    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 0, 512), "0030");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 63, 512), "1FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 127, 512), "3FB0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 259, 512), "81B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 300, 512), "9630");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 399, 512), "C7B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 499, 512), "F9B0");
    EXPECT_EQ(GenerateHashKey(kBaseHashKey, 511, 512), "FFB0");
}

} // namespace logtail
