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

#include "common/StringTools.h"
#include "unittest/Unittest.h"

namespace logtail {
extern std::vector<std::string> GetTopicNames(const boost::regex& regex);

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
    APSARA_TEST_EQUAL("/aaa/aa",filePath);
    filePath = "/";
    RemoveFilePathTrailingSlash(filePath);
    APSARA_TEST_EQUAL("/",filePath);
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

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
