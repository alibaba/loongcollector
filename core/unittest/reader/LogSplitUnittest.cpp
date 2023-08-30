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

#include "unittest/Unittest.h"
#include "reader/CommonRegLogFileReader.h"
#include "reader/SourceBuffer.h"
#include "common/FileSystemUtil.h"

namespace logtail
{

const std::string LOG_BEGIN_STRING = "Exception in thread \"main\" java.lang.NullPointerException";
const std::string LOG_BEGIN_REGEX = R"(Exception.*)";
const std::string LOG_CONTINUE_STRING = "    at com.example.myproject.Book.getTitle(Book.java:16)";
const std::string LOG_CONTINUE_REGEX = R"(\s+at\s.*)";
const std::string LOG_END_STRING = "    ...23 more";
const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more)";
const std::string LOG_UNMATCH = "unmatch log";

static void CompareTwoStringArray(std::vector<std::string> s1, std::vector<StringView> s2) {
    APSARA_TEST_EQUAL_FATAL(s1.size(), s2.size());
    for (size_t i = 0; i < s1.size(); i++) {
        APSARA_TEST_EQUAL_FATAL(s1[i], s2[i].to_string());
    }
}

class LogSplitUnittest : public ::testing::Test {
public:
    void TestLogSplitSingleLine();
    void TestLogSplitSingleLinePartNotMatch();
    void TestLogSplitSingleLinePartNotMatchNoDiscard();
    void TestLogSplitMultiLine();
    void TestLogSplitMultiLinePartNotMatch();
    void TestLogSplitMultiLinePartNotMatchNoDiscard();
    void TestLogSplitMultiLineAllNotmatch();
    void TestLogSplitMultiLineAllNotmatchNoDiscard();
};

UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitSingleLine);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitSingleLinePartNotMatch);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitSingleLinePartNotMatchNoDiscard);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLine);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLinePartNotMatch);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLinePartNotMatchNoDiscard);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLineAllNotmatch);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLineAllNotmatchNoDiscard);

void LogSplitUnittest::TestLogSplitSingleLine() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    std::string line1 = LOG_BEGIN_STRING + "first.";
    std::string line23 = "multiline1\nmultiline2";
    std::string line4 = LOG_BEGIN_STRING + "second.";
    std::string line56 = "multiline1\nmultiline2";
    std::string testLog = line1 + '\n' + line23 + '\n' + line4 + '\n' + line56;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(6UL, index.size());
    APSARA_TEST_EQUAL_FATAL(line1, index[0].to_string());
    APSARA_TEST_EQUAL_FATAL(line4, index[3].to_string());
}

void LogSplitUnittest::TestLogSplitSingleLinePartNotMatch() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = true;
    std::string line1 = "first.";
    std::string testLog = line1;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_FALSE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(0UL, index.size());
    APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
}

void LogSplitUnittest::TestLogSplitSingleLinePartNotMatchNoDiscard() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false);
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = false;
    std::string line1 = "first.";
    std::string testLog = line1;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_FALSE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(1UL, index.size());
    APSARA_TEST_EQUAL_FATAL(line1, index[0].to_string());
}

void LogSplitUnittest::TestLogSplitMultiLine() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = true;
    std::string firstLog = LOG_BEGIN_STRING + "first.\nmultiline1\nmultiline2";
    std::string secondLog = LOG_BEGIN_STRING + "second.\nmultiline1\nmultiline2";
    std::string testLog = firstLog + '\n' + secondLog;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(2UL, index.size());
    APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
    APSARA_TEST_EQUAL_FATAL(firstLog, index[0].to_string());
    APSARA_TEST_EQUAL_FATAL(secondLog, index[1].to_string());
}

void LogSplitUnittest::TestLogSplitMultiLinePartNotMatch() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = true;
    std::string firstLog = "first.\nmultiline1\nmultiline2";
    std::string secondLog = LOG_BEGIN_STRING + "second.\nmultiline1\nmultiline2";
    std::string testLog = firstLog + '\n' + secondLog;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(1UL, index.size());
    APSARA_TEST_EQUAL_FATAL(secondLog, index[0].to_string());
    APSARA_TEST_EQUAL_FATAL(3UL, discard.size());
    CompareTwoStringArray({"first.", "multiline1", "multiline2"}, discard);
}

void LogSplitUnittest::TestLogSplitMultiLinePartNotMatchNoDiscard() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false);
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = false;
    std::string firstLog = "first.\nmultiline1\nmultiline2";
    std::string secondLog = LOG_BEGIN_STRING + "second.\nmultiline1\nmultiline2";
    std::string testLog = firstLog + '\n' + secondLog;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(4UL, index.size());
    CompareTwoStringArray({"first.", "multiline1", "multiline2", secondLog}, index);
}

void LogSplitUnittest::TestLogSplitMultiLineAllNotmatch() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = true;
    std::string firstLog = "first.\nmultiline1\nmultiline2";
    std::string secondLog = "second.\nmultiline1\nmultiline2";
    std::string testLog = firstLog + '\n' + secondLog;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_FALSE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(0UL, index.size());
    APSARA_TEST_EQUAL_FATAL(6UL, discard.size());
    CompareTwoStringArray({"first.", "multiline1", "multiline2", "second.", "multiline1", "multiline2"}, discard);
}

void LogSplitUnittest::TestLogSplitMultiLineAllNotmatchNoDiscard() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false);
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = false;
    std::string firstLog = "first.\nmultiline1\nmultiline2";
    std::string secondLog = "second.\nmultiline1\nmultiline2";
    std::string testLog = firstLog + '\n' + secondLog;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_FALSE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(6UL, index.size());
    CompareTwoStringArray({"first.", "multiline1", "multiline2", "second.", "multiline1", "multiline2"}, index);
}

class LogSplitDiscardUnmatchUnittest : public ::testing::Test {
public:
    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();
};

UNIT_TEST_CASE(LogSplitDiscardUnmatchUnittest, TestLogSplitWithBeginContinue);
UNIT_TEST_CASE(LogSplitDiscardUnmatchUnittest, TestLogSplitWithBeginEnd);
UNIT_TEST_CASE(LogSplitDiscardUnmatchUnittest, TestLogSplitWithBegin);
UNIT_TEST_CASE(LogSplitDiscardUnmatchUnittest, TestLogSplitWithContinueEnd);
UNIT_TEST_CASE(LogSplitDiscardUnmatchUnittest, TestLogSplitWithEnd);

void LogSplitDiscardUnmatchUnittest::TestLogSplitWithBeginContinue() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, LOG_CONTINUE_REGEX, "");
    logFileReader.mDiscardUnmatch = true;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: complete log (end with unmatch log)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[1].to_string());
    }
    { // case: complete log (only begin)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_BEGIN_STRING, LOG_BEGIN_STRING}, index);
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
}

void LogSplitDiscardUnmatchUnittest::TestLogSplitWithBeginEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = true;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[1].to_string());
    }
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[1].to_string());
    }
    { // case: incomplete log (begin)
        std::string expectMatch = LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(3UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_BEGIN_STRING, LOG_UNMATCH}, discard);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
}

void LogSplitDiscardUnmatchUnittest::TestLogSplitWithBegin() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = true;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_BEGIN_STRING + '\n' + LOG_UNMATCH;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
}

void LogSplitDiscardUnmatchUnittest::TestLogSplitWithContinueEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy("", LOG_CONTINUE_REGEX, LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = true;
    { // case: complete log
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(1UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[1].to_string());
    }
    { // case: complete log (only end)
        std::string expectMatch = LOG_END_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_END_STRING, LOG_END_STRING}, index);
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
    { // case: incomplete log (continue)
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(4UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_CONTINUE_STRING, LOG_CONTINUE_STRING, LOG_UNMATCH}, discard);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
}

void LogSplitDiscardUnmatchUnittest::TestLogSplitWithEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy("", "", LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = true;
    { // case: complete log
        std::string expectMatch1 = LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string expectMatch2 = LOG_END_STRING;
        std::string testLog = expectMatch1 + '\n' + expectMatch2;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch1, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch2, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch1 = LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string expectMatch2 = LOG_END_STRING;
        std::string testLog = expectMatch1 + '\n' + expectMatch2 + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(1UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch1, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch2, index[1].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, discard[0].to_string());
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(0UL, index.size());
        APSARA_TEST_EQUAL_FATAL(2UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, discard);
    }
}

class LogSplitNoDiscardUnmatchUnittest : public ::testing::Test {
public:
    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();
};

UNIT_TEST_CASE(LogSplitNoDiscardUnmatchUnittest, TestLogSplitWithBeginContinue);
UNIT_TEST_CASE(LogSplitNoDiscardUnmatchUnittest, TestLogSplitWithBeginEnd);
UNIT_TEST_CASE(LogSplitNoDiscardUnmatchUnittest, TestLogSplitWithBegin);
UNIT_TEST_CASE(LogSplitNoDiscardUnmatchUnittest, TestLogSplitWithContinueEnd);
UNIT_TEST_CASE(LogSplitNoDiscardUnmatchUnittest, TestLogSplitWithEnd);

void LogSplitNoDiscardUnmatchUnittest::TestLogSplitWithBeginContinue() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, LOG_CONTINUE_REGEX, "");
    logFileReader.mDiscardUnmatch = false;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(3UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[2].to_string());
    }
    { // case: complete log (only begin)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(4UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_BEGIN_STRING, LOG_BEGIN_STRING, LOG_UNMATCH}, index);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, index);
    }
}

void LogSplitNoDiscardUnmatchUnittest::TestLogSplitWithBeginEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = false;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(3UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[2].to_string());
    }
    { // case: incomplete log (begin)
        std::string expectMatch = LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(3UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_BEGIN_STRING, LOG_UNMATCH}, index);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, index);
    }
}

void LogSplitNoDiscardUnmatchUnittest::TestLogSplitWithBegin() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy(LOG_BEGIN_REGEX, "", "");
    logFileReader.mDiscardUnmatch = false;
    { // case: complete log
        std::string expectMatch = LOG_BEGIN_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_BEGIN_STRING + '\n' + LOG_UNMATCH;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, index);
    }
}

void LogSplitNoDiscardUnmatchUnittest::TestLogSplitWithContinueEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy("", LOG_CONTINUE_REGEX, LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = false;
    { // case: complete log
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(3UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch, index[1].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[2].to_string());
    }
    { // case: complete log (only end)
        std::string expectMatch = LOG_END_STRING + "\n" + LOG_END_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(4UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_END_STRING, LOG_END_STRING, LOG_UNMATCH}, index);
    }
    { // case: incomplete log (continue)
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING;
        std::string testLog = LOG_UNMATCH + "\n" + expectMatch + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(4UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_CONTINUE_STRING, LOG_CONTINUE_STRING, LOG_UNMATCH}, index);
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, index);
    }
}

void LogSplitNoDiscardUnmatchUnittest::TestLogSplitWithEnd() {
    CommonRegLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogMultilinePolicy("", "", LOG_END_REGEX);
    logFileReader.mDiscardUnmatch = false;
    { // case: complete log
        std::string expectMatch1 = LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string expectMatch2 = LOG_END_STRING;
        std::string testLog = expectMatch1 + '\n' + expectMatch2;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch1, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch2, index[1].to_string());
    }
    { // case: complete log (end with unmatch)
        std::string expectMatch1 = LOG_UNMATCH + "\n" + LOG_END_STRING;
        std::string expectMatch2 = LOG_END_STRING;
        std::string testLog = expectMatch1 + '\n' + expectMatch2 + '\n' + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_TRUE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(3UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        APSARA_TEST_EQUAL_FATAL(expectMatch1, index[0].to_string());
        APSARA_TEST_EQUAL_FATAL(expectMatch2, index[1].to_string());
        APSARA_TEST_EQUAL_FATAL(LOG_UNMATCH, index[2].to_string());
    }
    { // case: no match log
        std::string expectMatch = "";
        std::string testLog = LOG_UNMATCH + "\n" + LOG_UNMATCH;
        int32_t lineFeed = 0;
        std::vector<StringView> index;
        std::vector<StringView> discard;
        bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
        APSARA_TEST_FALSE_FATAL(splitSuccess);
        APSARA_TEST_EQUAL_FATAL(2UL, index.size());
        APSARA_TEST_EQUAL_FATAL(0UL, discard.size());
        CompareTwoStringArray({LOG_UNMATCH, LOG_UNMATCH}, index);
    }
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}