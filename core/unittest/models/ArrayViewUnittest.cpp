// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <array>
#include <string>
#include <vector>

#include "models/ArrayView.h"
#include "unittest/Unittest.h"

namespace logtail {

class ArrayViewUnittest : public ::testing::Test {
public:
    void TestDefaultConstructor();
    void TestArrayConstructor();
    void TestPointerConstructor();
    void TestStdArrayConstructor();
    void TestSize();
    void TestSubscriptOperator();
    void TestIterator();
    void TestConstCorrectness();

protected:
    void SetUp() override {}
    void TearDown() override {}
};

void ArrayViewUnittest::TestDefaultConstructor() {
    // 测试默认构造函数
    ArrayView<int> view;
    APSARA_TEST_EQUAL(view.size(), 0);
}

void ArrayViewUnittest::TestArrayConstructor() {
    // 测试从C风格数组构造
    const int arr[] = {1, 2, 3, 4, 5};
    ArrayView<int> view(arr);

    APSARA_TEST_EQUAL(view.size(), 5);
    for (size_t i = 0; i < 5; ++i) {
        APSARA_TEST_EQUAL(view[i], arr[i]);
    }
}

void ArrayViewUnittest::TestPointerConstructor() {
    // 测试从指针和大小构造
    const int arr[] = {1, 2, 3, 4, 5};
    ArrayView<int> view(arr, 3); // 只使用前3个元素

    APSARA_TEST_EQUAL(view.size(), 3);
    for (size_t i = 0; i < 3; ++i) {
        APSARA_TEST_EQUAL(view[i], arr[i]);
    }
}

void ArrayViewUnittest::TestStdArrayConstructor() {
    // 测试从std::array构造
    std::array<int, 4> arr = {1, 2, 3, 4};
    ArrayView<int> view(arr);

    APSARA_TEST_EQUAL(view.size(), 4);
    for (size_t i = 0; i < 4; ++i) {
        APSARA_TEST_EQUAL(view[i], arr[i]);
    }
}

void ArrayViewUnittest::TestSize() {
    // 测试size()方法
    const int arr[] = {1, 2, 3, 4, 5};
    ArrayView<int> view1; // 空视图
    ArrayView<int> view2(arr); // 完整数组视图
    ArrayView<int> view3(arr, 3); // 部分数组视图

    auto view1Size = view1.size();
    auto view2Size = view2.size();
    auto view3Size = view3.size();
    APSARA_TEST_EQUAL(view1Size, 0);
    APSARA_TEST_EQUAL(view2Size, 5);
    APSARA_TEST_EQUAL(view3Size, 3);
}

void ArrayViewUnittest::TestSubscriptOperator() {
    // 测试下标操作符
    const int arr[] = {1, 2, 3, 4, 5};
    ArrayView<int> view(arr, 5);

    auto value0 = view[0];
    auto value2 = view[2];
    auto value4 = view[4];
    APSARA_TEST_EQUAL(value0, 1);
    APSARA_TEST_EQUAL(value2, 3);
    APSARA_TEST_EQUAL(value4, 5);
}

void ArrayViewUnittest::TestIterator() {
    // 测试迭代器功能
    const int arr[] = {1, 2, 3, 4, 5};
    ArrayView<int> view(arr);

    // 测试范围for循环
    int sum = 0;
    for (const auto& value : view) {
        sum += value;
    }
    APSARA_TEST_EQUAL(sum, 15); // 1+2+3+4+5 = 15

    // 测试迭代器操作
    auto it = view.begin();
    APSARA_TEST_EQUAL(*it, 1);
    ++it;
    APSARA_TEST_EQUAL(*it, 2);

    // 测试迭代器比较
    APSARA_TEST_TRUE(view.begin() != view.end());
}

void ArrayViewUnittest::TestConstCorrectness() {
    // 测试const正确性
    struct TestStruct {
        int value;
    };

    static constexpr TestStruct arr[] = {{1}, {2}, {3}};

    constexpr ArrayView<TestStruct> view(arr);

    auto value0 = view[0].value;
    auto value1 = view[1].value;
    auto value2 = view[2].value;
    APSARA_TEST_EQUAL(value0, 1);
    APSARA_TEST_EQUAL(value1, 2);
    APSARA_TEST_EQUAL(value2, 3);
}

UNIT_TEST_CASE(ArrayViewUnittest, TestDefaultConstructor);
UNIT_TEST_CASE(ArrayViewUnittest, TestArrayConstructor);
UNIT_TEST_CASE(ArrayViewUnittest, TestPointerConstructor);
UNIT_TEST_CASE(ArrayViewUnittest, TestStdArrayConstructor);
UNIT_TEST_CASE(ArrayViewUnittest, TestSize);
UNIT_TEST_CASE(ArrayViewUnittest, TestSubscriptOperator);
UNIT_TEST_CASE(ArrayViewUnittest, TestIterator);
UNIT_TEST_CASE(ArrayViewUnittest, TestConstCorrectness);

} // namespace logtail

UNIT_TEST_MAIN
