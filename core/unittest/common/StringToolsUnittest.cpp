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

#include <cstdint>

#include "common/StringTools.h"
#include "common/StringView.h"
#include "unittest/Unittest.h"

namespace logtail {
extern std::vector<std::string> GetTopicNames(const boost::regex& regex);
}

using namespace logtail;

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

TEST_F(StringToolsUnittest, TestStartWith) {
    EXPECT_TRUE(StartWith("/asdfasdf/asdfasdf/asdfasdf", "/"));
    EXPECT_TRUE(StartWith("/asdfasdf/asdfasdf/asdfasdf", "/asdfasdf"));
    EXPECT_FALSE(StartWith("/asdfasdf/asdfasdf/asdfasdf", "/213123"));
    EXPECT_FALSE(StartWith("/", "/asdfasdf"));
    EXPECT_FALSE(StartWith("", "/asdfasdf"));
}

TEST_F(StringToolsUnittest, TestEndWith) {
    EXPECT_TRUE(EndWith("a.json", ".json"));
    EXPECT_FALSE(EndWith("a.json.bak", ".json"));
    EXPECT_FALSE(EndWith("a.json", "xxx.json"));
    EXPECT_FALSE(EndWith("a.json ", ".json"));
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

TEST_F(StringToolsUnittest, TestGetTopicNames) {
    {
        boost::regex regex(R"(/stdlog/(.*?)/.*)", boost::regex::save_subexpression_location);
        std::vector<std::string> names = GetTopicNames(regex);
        APSARA_TEST_EQUAL_FATAL(1UL, names.size());
    }
    {
        boost::regex regex(R"(/stdlog/(?<container_name>.*?)/.*)", boost::regex::save_subexpression_location);
        std::vector<std::string> names = GetTopicNames(regex);
        APSARA_TEST_EQUAL_FATAL(1UL, names.size());
        APSARA_TEST_EQUAL_FATAL(std::string("container_name"), names[0]);
    }
    {
        boost::regex regex(R"(/stdlog/(?<container_name>.*?)/(?<log_name>.*?))",
                           boost::regex::save_subexpression_location);
        std::vector<std::string> names = GetTopicNames(regex);
        APSARA_TEST_EQUAL_FATAL(2UL, names.size());
        APSARA_TEST_EQUAL_FATAL(std::string("container_name"), names[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("log_name"), names[1]);
    }
}

TEST_F(StringToolsUnittest, TestRemoveFilePathTrailingSlash) {
    std::string filePath = "/aaa/aa/";
    RemoveFilePathTrailingSlash(filePath);
    APSARA_TEST_EQUAL("/aaa/aa", filePath);
    filePath = "/";
    RemoveFilePathTrailingSlash(filePath);
    APSARA_TEST_EQUAL("/", filePath);
}

TEST_F(StringToolsUnittest, TestBoostRegexSearch) {
    {
        // ^(\[\d+-\d+-\d+\].*)|(\[\d+\].*)
        std::string buffer = "[2024-04-01] xxxxxx";
        boost::regex reg(R"(^(\[\d+-\d+-\d+\].*)|(\[\d+\].*))"); // Regular expression to match "test"
        std::string exception;
        bool result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);

        buffer = "aaa[2024-04-01] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);

        buffer = "[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);
        buffer = "123[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);
    }

    {
        // ^(\[\d+-\d+-\d+\].*)|\[\d+\]
        std::string buffer = "[2024-04-01] xxxxxx";
        boost::regex reg(R"(^(\[\d+-\d+-\d+\].*)|\[\d+\])"); // Regular expression to match "test"
        std::string exception;
        bool result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);

        buffer = "aaa[2024-04-01] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);

        buffer = "[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);
        buffer = "123[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);
    }

    {
        // ^\[\d+-\d+-\d+\].*|\[\d+\]
        std::string buffer = "[2024-04-01] xxxxxx";
        boost::regex reg(R"(^\[\d+-\d+-\d+\].*|\[\d+\])"); // Regular expression to match "test"
        std::string exception;
        bool result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);

        buffer = "aaa[2024-04-01] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);

        buffer = "[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);
        buffer = "123[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);
    }


    {
        // ^\[\d+-\d+-\d+\]|\[\d+\]
        std::string buffer = "[2024-04-01] xxxxxx";
        boost::regex reg(R"(^\[\d+-\d+-\d+\]|\[\d+\])"); // Regular expression to match "test"
        std::string exception;
        bool result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);

        buffer = "aaa[2024-04-01] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);

        buffer = "[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_TRUE(result);
        buffer = "123[138998928392] xxxxxx";
        result = BoostRegexSearch(buffer.data(), reg, exception);
        EXPECT_FALSE(result);
    }
}

TEST_F(StringToolsUnittest, TestNormalizeTopicRegFormat) {
    { // Perl flavor
        std::string topicFormat(R"(/stdlog/(?<container_name>.*?)/(?<log_name>.*?))");
        APSARA_TEST_TRUE_FATAL(NormalizeTopicRegFormat(topicFormat));
        APSARA_TEST_EQUAL_FATAL(topicFormat, std::string(R"(/stdlog/(?<container_name>.*?)/(?<log_name>.*?))"));
    }
    { // PCRE flavor
        std::string topicFormat(R"(/stdlog/(?P<container_name>.*?)/(?P<log_name>.*?))");
        APSARA_TEST_TRUE_FATAL(NormalizeTopicRegFormat(topicFormat));
        APSARA_TEST_EQUAL_FATAL(topicFormat, std::string(R"(/stdlog/(?<container_name>.*?)/(?<log_name>.*?))"));
    }
}

TEST_F(StringToolsUnittest, TestExtractTopics) {
    { // default topic name
        std::vector<std::string> keys, values;
        APSARA_TEST_TRUE_FATAL(ExtractTopics(R"(/stdlog/main/0.log)", R"(/stdlog/(.*?)/.*)", keys, values));
        APSARA_TEST_EQUAL_FATAL(1UL, keys.size());
        APSARA_TEST_EQUAL_FATAL(1UL, values.size());
        APSARA_TEST_EQUAL_FATAL(std::string("__topic_1__"), keys[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("main"), values[0]);
    }
    { // one topic name
        std::vector<std::string> keys, values;
        APSARA_TEST_TRUE_FATAL(ExtractTopics(R"(/logtail_host/u01/u02/dts/run/fqza707b1543bdm/logs/index.log)",
                                             R"(/logtail_host/u01/u02/dts/run/(?<jobid>.*)/logs/index\.log)",
                                             keys,
                                             values));
        APSARA_TEST_EQUAL_FATAL(1UL, keys.size());
        APSARA_TEST_EQUAL_FATAL(1UL, values.size());
        APSARA_TEST_EQUAL_FATAL(std::string("jobid"), keys[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("fqza707b1543bdm"), values[0]);
    }
    { // one topic name with underscore
        std::vector<std::string> keys, values;
        APSARA_TEST_TRUE_FATAL(
            ExtractTopics(R"(/stdlog/main/0.log)", R"(/stdlog/(?<container_name>.*?)/.*)", keys, values));
        APSARA_TEST_EQUAL_FATAL(1UL, keys.size());
        APSARA_TEST_EQUAL_FATAL(1UL, values.size());
        APSARA_TEST_EQUAL_FATAL(std::string("container_name"), keys[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("main"), values[0]);
    }
    /* The code snippet is a unit test case for the `ExtractTopics` function. It tests the extraction of
    topic names and values from a given input string using a regular expression pattern. */
    { // two topic name
        std::vector<std::string> keys, values;
        APSARA_TEST_TRUE_FATAL(
            ExtractTopics(R"(/stdlog/main/0.log)", R"(/stdlog/(?<container_name>.*?)/(?<log_name>.*?))", keys, values));
        APSARA_TEST_EQUAL_FATAL(2UL, keys.size());
        APSARA_TEST_EQUAL_FATAL(2UL, values.size());
        APSARA_TEST_EQUAL_FATAL(std::string("container_name"), keys[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("main"), values[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("log_name"), keys[1]);
        APSARA_TEST_EQUAL_FATAL(std::string("0.log"), values[1]);
    }
    {
        std::vector<std::string> keys, values;
        APSARA_TEST_TRUE_FATAL(
            ExtractTopics(R"(/stdlog/main/0.log)", R"(/stdlog/(.*?)/(?<log_name>.*?))", keys, values));
        APSARA_TEST_EQUAL_FATAL(2UL, keys.size());
        APSARA_TEST_EQUAL_FATAL(2UL, values.size());
        APSARA_TEST_EQUAL_FATAL(std::string("__topic_1__"), keys[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("main"), values[0]);
        APSARA_TEST_EQUAL_FATAL(std::string("log_name"), keys[1]);
        APSARA_TEST_EQUAL_FATAL(std::string("0.log"), values[1]);
    }
}

TEST_F(StringToolsUnittest, TestLtrim) {
    StringView v1 = "";
    APSARA_TEST_EQUAL(StringView(""), Ltrim(v1));

    StringView v2 = "2 2";
    APSARA_TEST_EQUAL(StringView("2 2"), Ltrim(v2));

    StringView v3 = " 33";
    APSARA_TEST_EQUAL(StringView("33"), Ltrim(v3));

    StringView v4 = "44 ";
    APSARA_TEST_EQUAL(StringView("44 "), Ltrim(v4));

    StringView v5 = " 55 ";
    APSARA_TEST_EQUAL(StringView("55 "), Ltrim(v5));
}

TEST_F(StringToolsUnittest, TestRtrim) {
    StringView v1 = "";
    APSARA_TEST_EQUAL(StringView(""), Rtrim(v1));

    StringView v2 = "2 2";
    APSARA_TEST_EQUAL(StringView("2 2"), Rtrim(v2));

    StringView v3 = " 33";
    APSARA_TEST_EQUAL(StringView(" 33"), Rtrim(v3));

    StringView v4 = "44 ";
    APSARA_TEST_EQUAL(StringView("44"), Rtrim(v4));

    StringView v5 = " 55 ";
    APSARA_TEST_EQUAL(StringView(" 55"), Rtrim(v5));
}

TEST_F(StringToolsUnittest, TestTrim) {
    StringView v1 = "";
    APSARA_TEST_EQUAL(StringView(""), Trim(v1));

    StringView v2 = "2 2";
    APSARA_TEST_EQUAL(StringView("2 2"), Trim(v2));

    StringView v3 = " 33";
    APSARA_TEST_EQUAL(StringView("33"), Trim(v3));

    StringView v4 = "44 ";
    APSARA_TEST_EQUAL(StringView("44"), Trim(v4));

    StringView v5 = " 55 ";
    APSARA_TEST_EQUAL(StringView("55"), Trim(v5));

    StringView v6("\0ss\0", 4);
    APSARA_TEST_EQUAL(StringView("ss"), Trim(v6, kNullSv));
}

TEST_F(StringToolsUnittest, TestStringViewSplitterEmpty) {
    StringView sv("");
    int i = 0;
    for (auto field : StringViewSplitter(sv, StringView("\0", 1))) {
        APSARA_TEST_EQUAL_FATAL(StringView(""), field);
        ++i;
    }
    APSARA_TEST_EQUAL(1, i);
}

TEST_F(StringToolsUnittest, TestStringViewSplitterSingle) {
    StringView sv("111");
    int i = 0;
    for (auto field : StringViewSplitter(sv, StringView("\0", 1))) {
        if (i == 0) {
            APSARA_TEST_EQUAL_FATAL(StringView("111"), field);
        }
        ++i;
    }
    APSARA_TEST_EQUAL(1, i);
}

TEST_F(StringToolsUnittest, TestStringViewSplitterMulti) {
    static const char data[] = "111\000222 333\000444";
    StringView sv(data, sizeof(data) - 1);
    int i = 0;
    for (auto field : StringViewSplitter(sv, StringView("\0", 1))) {
        if (i == 0) {
            APSARA_TEST_EQUAL_FATAL(StringView("111"), field);
        } else if (i == 1) {
            APSARA_TEST_EQUAL_FATAL(StringView("222 333"), field);
        } else {
            APSARA_TEST_EQUAL_FATAL(StringView("444"), field);
        }
        ++i;
    }
    APSARA_TEST_EQUAL(3, i);
}

TEST_F(StringToolsUnittest, TestStringViewSplitterMultiEmpty) {
    static const char data[] = "111\000";
    StringView sv(data, sizeof(data) - 1);
    int i = 0;
    for (auto field : StringViewSplitter(sv, StringView("\0", 1))) {
        if (i == 0) {
            APSARA_TEST_EQUAL_FATAL(StringView("111"), field);
        } else if (i == 1) {
            APSARA_TEST_EQUAL_FATAL(StringView(""), field);
        }
        ++i;
    }
    APSARA_TEST_EQUAL(2, i);
}

TEST_F(StringToolsUnittest, TestStringViewSplitterMultiEmptyEmpty) {
    static const char data[] = "\000";
    StringView sv(data, sizeof(data) - 1);
    int i = 0;
    for (auto field : StringViewSplitter(sv, StringView("\0", 1))) {
        APSARA_TEST_EQUAL_FATAL(StringView(""), field);
        ++i;
    }
    APSARA_TEST_EQUAL(2, i);
}

TEST_F(StringToolsUnittest, TestStringTo) {
    int i = 0;
    APSARA_TEST_FALSE(StringTo(nullptr, nullptr, i));
    APSARA_TEST_FALSE(StringTo((const char*)1, nullptr, i));
    APSARA_TEST_FALSE(StringTo(nullptr, (const char*)1, i));
    APSARA_TEST_TRUE(StringTo(std::string("666"), i));
    APSARA_TEST_EQUAL(666, i);
    long j = 0;
    APSARA_TEST_TRUE(StringTo(std::string("-666666"), j));
    APSARA_TEST_EQUAL(-666666, j);
    uint32_t l = 0;
    APSARA_TEST_TRUE(StringTo(std::string_view("777"), l));
    APSARA_TEST_EQUAL(777U, l);
    uint64_t k = 0;
    APSARA_TEST_FALSE(StringTo(std::string_view("-888"), k));

    bool b = false;
    APSARA_TEST_TRUE(StringTo("true", b));
    APSARA_TEST_EQUAL(true, b);
    APSARA_TEST_TRUE(StringTo("false", b));
    APSARA_TEST_EQUAL(false, b);
    APSARA_TEST_TRUE(StringTo("any", b));
    APSARA_TEST_EQUAL(false, b);
    APSARA_TEST_FALSE(StringTo(nullptr, b));

    float f = 0.0F;
    APSARA_TEST_FALSE(StringTo(nullptr, nullptr, f));
    APSARA_TEST_FALSE(StringTo(std::to_string(std::numeric_limits<double>::max()), f));
    APSARA_TEST_TRUE(StringTo("111.111", f));
    APSARA_TEST_EQUAL(111.111F, f);

    double d = 0.0;
    APSARA_TEST_TRUE(StringTo("1111.1111", d));
    APSARA_TEST_EQUAL(1111.1111, d);

    std::string s;
    APSARA_TEST_FALSE(StringTo(nullptr, nullptr, s));
}

UNIT_TEST_MAIN
