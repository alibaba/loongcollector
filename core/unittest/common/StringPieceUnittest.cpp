// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include "common/FileSystemUtil.h"
#include "app_config/AppConfig.h"
#include "StringPiece.h"

namespace logtail {

class StringPieceUnittest : public ::testing::Test {
public:
    void Test() {
        // test char*
        {
            StringPiece input = "12345";
            APSARA_TEST_EQUAL(input.size(), 5);
            APSARA_TEST_EQUAL(input.length(), 5);
            APSARA_TEST_EQUAL(input.max_size(), 5);
            APSARA_TEST_EQUAL(input.capacity(), 5);
            APSARA_TEST_EQUAL(input.at(0), '1');
            APSARA_TEST_EQUAL(input[0], '1');
            APSARA_TEST_EQUAL(input.empty(), false);
            APSARA_TEST_EQUAL(input.as_string(), "12345");
            APSARA_TEST_EQUAL(input.compare("012"), 1);
            APSARA_TEST_EQUAL(*input.begin(), input.at(0));
            APSARA_TEST_EQUAL(*(input.end() - 1), input.at(4));
            APSARA_TEST_EQUAL(*input.rbegin(), input.at(4));
            APSARA_TEST_EQUAL(*(--input.rend()), input.at(0));
            APSARA_TEST_EQUAL(input.find('1'), 0);
            APSARA_TEST_EQUAL(input.rfind('1'), 0);
            APSARA_TEST_EQUAL(input.substr(1, 2).as_string(), std::string("23"));
            APSARA_TEST_TRUE(input == StringPiece("12345"));


            input.remove_prefix(1);
            APSARA_TEST_TRUE(input == StringPiece("2345"));
            input.remove_suffix(1);
            APSARA_TEST_TRUE(input == StringPiece("234"));
            input.set("1122334455");
            APSARA_TEST_EQUAL(input.size(), 10);
            APSARA_TEST_EQUAL(input.find_last_not_of('5'), 7);
            APSARA_TEST_EQUAL(input.find_last_of('5'), 9);
            APSARA_TEST_EQUAL(input.find_first_not_of('1'), 2);
            APSARA_TEST_EQUAL(input.find_first_of('1'), 0);
            APSARA_TEST_EQUAL(input.find('1'), 0);
            APSARA_TEST_EQUAL(input.rfind('1'), 1);


            StringPiece swapStr = "543210";
            input.swap(swapStr);
            APSARA_TEST_EQUAL(input.size(), 6);

            input.clear();
            APSARA_TEST_EQUAL(input.size(), 0);
            APSARA_TEST_EQUAL(input.empty(), true);
        }
    }
};

UNIT_TEST_CASE(StringPieceUnittest, Test);


} // namespace logtail

UNIT_TEST_MAIN