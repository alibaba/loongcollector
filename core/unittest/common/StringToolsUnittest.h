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
#include "common/StringTools.h"

namespace logtail {

class StringToolsUnittest : public ::testing::Test {};

TEST_F(StringToolsUnittest, TestToStringVector) {
    std::vector<std::string> v1;
    EXPECT_EQ("", ToString(v1));

    std::vector<std::string> v2{"a"};
    EXPECT_EQ("a", ToString(v2));

    std::vector<std::string> v3{"a", "b"};
    EXPECT_EQ("a,b", ToString(v3));

    std::vector<std::string> v4{"a", "b", "c"};
    EXPECT_EQ("a,b,c", ToString(v4));
}

TEST_F(StringToolsUnittest, TestEndWith) {
    EXPECT_TRUE(EndWith("a.json", ".json"));
    EXPECT_FALSE(EndWith("a.json.bak", ".json"));
    EXPECT_FALSE(EndWith("a.json", "xxx.json"));
    EXPECT_FALSE(EndWith("a.json ", ".json"));
}

TEST_F(StringToolsUnittest, TestStringToBoolean) {
    EXPECT_TRUE(StringTo<bool>("true"));
    EXPECT_FALSE(StringTo<bool>("false"));
    EXPECT_FALSE(StringTo<bool>("any"));
    EXPECT_FALSE(StringTo<bool>(""));
}

TEST_F(StringToolsUnittest, TestReplaceString) {
    std::string raw;

    raw = "{endpoint}....{str}";
    ReplaceString(raw, "{endpoint}", "endpoint");
    ReplaceString(raw, "{str}", "str");
    EXPECT_EQ(raw, "endpoint....str");

    raw = "{endpoint}....{str}....{endpoint}";
    ReplaceString(raw, "{endpoint}", "endpoint");
    ReplaceString(raw, "{str}", "str");
    EXPECT_EQ(raw, "endpoint....str....endpoint");

    raw = "...{endpoint}....{str}....{endpoint}...";
    ReplaceString(raw, "{endpoint}", "endpoint");
    ReplaceString(raw, "{str}", "str");
    EXPECT_EQ(raw, "...endpoint....str....endpoint...");
}

#if defined(_MSC_VER)
TEST_F(StringToolsUnittest, Test_fnmatch) {
    // Windows does not support FNM_PATHNAME, * is equal to **.
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\**", "C:\\test\\a\\1", 0));
    EXPECT_EQ(1, fnmatch("C:\\test\\a\\*\\b", "C:\\test\\a\\b", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\*\\b", "C:\\test\\a\\1\\b", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\*\\b", "C:\\test\\a\\1\\2\\b", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\**\\b", "C:\\test\\a\\1\\b", 0));
    EXPECT_EQ(1, fnmatch("C:\\test\\a\\**\\b", "C:\\test\\a\\b", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\**\\b", "C:\\test\\a\\1\\2\\b", 0));
    EXPECT_EQ(1, fnmatch("C:\\test\\a\\**", "C:\\test\\a", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\**", "C:\\test\\a\\1", 0));
    EXPECT_EQ(0, fnmatch("C:\\test\\a\\**", "C:\\test\\a\\1\\2\\b", 0));
}
#endif

} // namespace logtail
