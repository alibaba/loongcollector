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

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include "FileTagOptions.h"
#include "common/FileSystemUtil.h"
#include "common/memory/SourceBuffer.h"
#include "file_server/reader/LogFileReader.h"
#include "unittest/Unittest.h"

namespace logtail {

const std::string LOG_BEGIN_STRING = "Exception in thread \"main\" java.lang.NullPointerException";
const std::string LOG_BEGIN_REGEX = R"(Exception.*)";
const std::string LOG_CONTINUE_STRING = "    at com.example.myproject.Book.getTitle(Book.java:16)";
const std::string LOG_CONTINUE_REGEX = R"(\s+at\s.*)";
const std::string LOG_END_STRING = "    ...23 more";
const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more)";
const std::string LOG_UNMATCH = "unmatch log";

class RemoveLastIncompleteLogUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == logPathDir.back()) {
            logPathDir.resize(logPathDir.size() - 1);
        }
        logPathDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "LogFileReaderUnittest";
        gbkFile = "gbk.txt";
        utf8File = "utf8.txt"; // content of utf8.txt is equivalent to gbk.txt
    }

    static void TearDownTestCase() {
        remove(gbkFile.c_str());
        remove(utf8File.c_str());
    }

    void SetUp() override {
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        std::string filepath = logPathDir + PATH_SEPARATOR + utf8File;
        std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(filepath.c_str(), "r"), &std::fclose);
        if (!fp.get()) {
            return;
        }
        std::fseek(fp.get(), 0, SEEK_END);
        long filesize = std::ftell(fp.get());
        std::fseek(fp.get(), 0, SEEK_SET);
        expectedContent.reset(new char[filesize + 1]);
        fread(expectedContent.get(), filesize, 1, fp.get());
        expectedContent[filesize] = '\0';
        for (long i = filesize - 1; i >= 0; --i) {
            if (expectedContent[i] == '\n') {
                expectedContent[i] = 0;
                break;
            };
        }
    }

    void TestSingleline();
    void TestMultiline();

    std::unique_ptr<char[]> expectedContent;
    FileReaderOptions readerOpts;
    FileTagOptions tagOpts;
    CollectionPipelineContext ctx;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
};

UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestSingleline);
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestMultiline);

std::string RemoveLastIncompleteLogUnittest::logPathDir;
std::string RemoveLastIncompleteLogUnittest::gbkFile;
std::string RemoveLastIncompleteLogUnittest::utf8File;

void RemoveLastIncompleteLogUnittest::TestSingleline() {
    MultilineOptions multilineOpts;
    LogFileReader logFileReader(logPathDir,
                                utf8File,
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    { // case single line
        std::string line1 = "first.";
        std::string line2 = "second.";
        std::string line3 = "third.";
        std::string expectMatch = line1 + '\n' + line2 + '\n' + line3 + '\n';
        std::string testLog = expectMatch;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case single line, buffer size not big enough (no new line at the end of line)
        std::string line1 = "first.";
        std::string line2 = "second.";
        std::string line3 = "third_part.";
        std::string expectMatch = line1 + '\n' + line2 + '\n';
        std::string testLog = expectMatch + line3;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case single line, cannot be split, buffer size not big enough (no new line at the end of line)
      // it will be force read in ReadUTF8/ReadGBK
        std::string testLog = "first.";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(0, matchSize);
        // return the whole buffer, so no rollback
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case empty string
        std::string expectMatch = "";
        int32_t rollbackLineFeedCount = 0;
        std::string testLog2 = expectMatch + "";
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog2.data()), testLog2.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestMultiline() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader(logPathDir,
                                utf8File,
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    { // case multi line
        std::vector<int32_t> index;
        std::string firstLog = LOG_BEGIN_STRING + "first.\nmultiline1\nmultiline2";
        std::string secondLog = LOG_BEGIN_STRING + "second.\nmultiline1\nmultiline2";
        std::string expectMatch = firstLog + '\n';
        std::string testLog = expectMatch + secondLog + '\n';
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(3, rollbackLineFeedCount);
    }
    { // case multi line, buffer size not big enough (no new line at the end of line)
        std::vector<int32_t> index;
        std::string firstLog = LOG_BEGIN_STRING + "first.\nmultiline1\nmultiline2";
        std::string secondLog = LOG_BEGIN_STRING + "second.\nmultiline1\nmultiline2";
        std::string expectMatch = firstLog + '\n';
        std::string testLog = expectMatch + secondLog;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(3, rollbackLineFeedCount);
    }
    { // case multi line not match
        std::string testLog2 = "log begin does not match.\nlog begin does not match.\nlog begin does not match.\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog2.data()), testLog2.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(int32_t(testLog2.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case multi line not match, buffer size not big enough (no new line at the end of line)
        std::string expectMatch = "log begin does not match.\nlog begin does not match.\n";
        std::string testLog2 = expectMatch + "log begin does not";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog2.data()), testLog2.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case empty string
        std::string expectMatch = "";
        int32_t rollbackLineFeedCount = 0;
        std::string testLog2 = expectMatch + "";
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog2.data()), testLog2.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

class RemoveLastIncompleteLogMultilineUnittest : public ::testing::Test {
public:
    void TestRemoveLastIncompleteLogWithBeginContinue();
    void TestRemoveLastIncompleteLogWithBeginEnd();
    void TestRemoveLastIncompleteLogWithBegin();
    void TestRemoveLastIncompleteLogWithContinueEnd();
    void TestRemoveLastIncompleteLogWithEnd();
    void SetUp() override { readerOpts.mInputType = FileReaderOptions::InputType::InputFile; }

private:
    FileReaderOptions readerOpts;
    FileTagOptions tagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(RemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBeginContinue);
UNIT_TEST_CASE(RemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBeginEnd);
UNIT_TEST_CASE(RemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBegin);
UNIT_TEST_CASE(RemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithContinueEnd);
UNIT_TEST_CASE(RemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithEnd);

void RemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBeginContinue() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    // logFileReader.mDiscardUnmatch = true;
    { // case: end with begin continue
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + '\n';
        std::string testLog = expectMatch + LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(2, rollbackLineFeedCount);
    }
    { // case: end with begin
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + '\n';
        std::string testLog = expectMatch + LOG_BEGIN_STRING + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: end with unmatch
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n";
        std::string testLog = expectMatch + LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(2, rollbackLineFeedCount);
    }
    { // case: all unmatch
        std::string expectMatch = "\n\n";
        std::string testLog = expectMatch + LOG_UNMATCH;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBeginEnd() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    // logFileReader.mDiscardUnmatch = true;
    { // case: end with begin end
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING + '\n';
        std::string testLog = std::string(expectMatch.data());
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case: end with begin
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING + '\n';
        std::string testLog = expectMatch + LOG_BEGIN_STRING + LOG_BEGIN_STRING + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: end with unmatch
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING + "\n";
        std::string testLog = expectMatch + LOG_UNMATCH + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: all unmatch
        std::string expectMatch = "\n\n";
        std::string testLog = expectMatch + LOG_UNMATCH;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBegin() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    // logFileReader.mDiscardUnmatch = true;
    { // case: end with begin
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_UNMATCH + '\n';
        std::string testLog = expectMatch + LOG_BEGIN_STRING;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: end with unmatch
        std::string expectMatch = LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n";
        std::string testLog = expectMatch + LOG_BEGIN_STRING + "\n" + LOG_UNMATCH + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(2, rollbackLineFeedCount);
    }
    { // case: all unmatch
        std::string expectMatch = "\n\n";
        std::string testLog = expectMatch + LOG_UNMATCH;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithContinueEnd() {
    Json::Value config;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    // logFileReader.mDiscardUnmatch = true;
    { // case: end with continue end
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING + '\n';
        std::string testLog = std::string(expectMatch.data());
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case: end with continue
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING + '\n';
        std::string testLog = expectMatch + LOG_CONTINUE_STRING + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: end with unmatch
        std::string expectMatch = LOG_CONTINUE_STRING + "\n" + LOG_CONTINUE_STRING + "\n" + LOG_END_STRING + "\n";
        std::string testLog = expectMatch + LOG_UNMATCH + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: only \n
        std::string expectMatch = "\n\n";
        std::string testLog = expectMatch + LOG_UNMATCH;
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithEnd() {
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&tagOpts, &ctx));
    // logFileReader.mDiscardUnmatch = true;
    { // case: end with end
        {
            std::string expectMatch = LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING + '\n';
            std::string testLog = std::string(expectMatch.data());
            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
            APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING;
            std::string testLog = std::string(expectMatch.data());
            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            APSARA_TEST_EQUAL(0, matchSize);
            APSARA_TEST_EQUAL(std::string(testLog.data(), matchSize), "");
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    } // namespace logtail
    { // case: end with unmatch
        std::string expectMatch = LOG_UNMATCH + "\n" + LOG_UNMATCH + "\n" + LOG_END_STRING + '\n';
        std::string testLog = expectMatch + LOG_UNMATCH + "\n";
        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case: all unmatch
        {
            std::string expectMatch = "\n\n";
            std::string testLog = expectMatch + LOG_UNMATCH;
            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            APSARA_TEST_EQUAL_FATAL(static_cast<int32_t>(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
            APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n" + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch;
            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            APSARA_TEST_EQUAL(static_cast<int32_t>(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(std::string(testLog.data(), matchSize), expectMatch);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
    }
}

class GetLastLineUnittest : public ::testing::Test {
public:
    void TestGetLastLine();
    void TestGetLastLineEmpty();

private:
    FileReaderOptions readerOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(GetLastLineUnittest, TestGetLastLine);
UNIT_TEST_CASE(GetLastLineUnittest, TestGetLastLineEmpty);

void GetLastLineUnittest::TestGetLastLine() {
    std::string testLog = "first line\nsecond line\nthird line";
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(nullptr, &ctx),
                                std::make_pair(nullptr, &ctx));
    auto lastLine = logFileReader.GetLastLine(const_cast<char*>(testLog.data()), testLog.size());
    std::string expectLog = "third line";
    APSARA_TEST_EQUAL_FATAL(expectLog, std::string(lastLine.data.data(), lastLine.data.size()));
}

void GetLastLineUnittest::TestGetLastLineEmpty() {
    std::string testLog = "";
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(nullptr, &ctx),
                                std::make_pair(nullptr, &ctx));
    auto lastLine = logFileReader.GetLastLine(const_cast<char*>(testLog.data()), testLog.size());
    APSARA_TEST_EQUAL_FATAL(0, int(lastLine.data.size()));
    APSARA_TEST_EQUAL_FATAL("", std::string(lastLine.data.data(), lastLine.data.size()));
    APSARA_TEST_EQUAL_FATAL(testLog.data(), lastLine.data);
}

class ContainerdTextRemoveLastIncompleteLogMultilineUnittest : public ::testing::Test {
public:
    void TestRemoveLastIncompleteLogWithBeginEnd();
    void TestRemoveLastIncompleteLogWithBegin();
    void TestRemoveLastIncompleteLogWithEnd();
    void SetUp() override { readerOpts.mInputType = FileReaderOptions::InputType::InputContainerStdio; }

private:
    FileReaderOptions readerOpts;
    CollectionPipelineContext ctx;
    const std::string LOG_PART = "2021-08-25T07:00:00.000000000Z stdout P ";
    const std::string LOG_FULL = "2021-08-25T07:00:00.000000000Z stdout F ";
    const std::string LOG_FULL_NOT_FOUND = "2021-08-25T07:00:00.000000000Z stdout ";
    const std::string LOG_ERROR = "2021-08-25T07:00:00.000000000Z stdout";

    const std::string LOG_BEGIN_STRING = "Exception in thread \"main\" java.lang.NullPointerException";
    const std::string LOG_BEGIN_REGEX = R"(Exception.*)";

    const std::string LOG_END_STRING = "    ...23 more";
    const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more.*)";

    const std::string LOG_UNMATCH = "unmatch log";
};

UNIT_TEST_CASE(ContainerdTextRemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBeginEnd);
UNIT_TEST_CASE(ContainerdTextRemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBegin);
UNIT_TEST_CASE(ContainerdTextRemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithEnd);

void ContainerdTextRemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBeginEnd() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(nullptr, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    { // case: end with begin end
        std::string expectMatch
            = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
        std::string testLog = std::string(expectMatch.data());

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
    }
    { // case: end with begin
        std::string expectMatch
            = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
        std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n";

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
    }
    { // case: end with unmatch
        std::string expectMatch
            = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + "\n";
        std::string testLog = expectMatch + LOG_FULL + LOG_UNMATCH + "\n";

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
    }
    { // case: all unmatch
        std::string expectMatch = "\n\n";
        std::string testLog = expectMatch + LOG_FULL + LOG_UNMATCH;

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
    }
}

void ContainerdTextRemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBegin() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(nullptr, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    { // case: end with begin
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_PART + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_PART + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
    }
    { // case: end with unmatch
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
    }
    { // case: all unmatch
        {
            std::string expectMatch = "\n\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n";
            std::string testLog = expectMatch + LOG_FULL + LOG_UNMATCH;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch + LOG_PART + LOG_BEGIN_STRING + "\n" + LOG_PART + LOG_BEGIN_STRING + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch + LOG_PART + LOG_BEGIN_STRING + "\n" + LOG_PART + LOG_BEGIN_STRING;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
    }
    { // case: end with part log
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_PART + LOG_BEGIN_STRING + "\n"
                + LOG_PART + LOG_BEGIN_STRING + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch + LOG_FULL + LOG_BEGIN_STRING + "\n" + LOG_PART + LOG_BEGIN_STRING + "\n"
                + LOG_PART + LOG_BEGIN_STRING;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    }
}

void ContainerdTextRemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithEnd() {
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(nullptr, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    { // case: end with end
        {
            std::string expectMatch = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_PART
                + LOG_END_STRING + '\n' + LOG_FULL + LOG_UNMATCH;
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(0, matchSize);
            APSARA_TEST_EQUAL("", matchLog);
            APSARA_TEST_EQUAL(4, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_PART
                + LOG_END_STRING + '\n' + LOG_FULL + LOG_UNMATCH + '\n';
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING;
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(0, matchSize);
            APSARA_TEST_EQUAL("", matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    }
    { // case: end with unmatch
        std::string expectMatch
            = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
        std::string testLog = expectMatch + LOG_FULL + LOG_UNMATCH + "\n";

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
    }
    { // case: all unmatch
        {
            std::string expectMatch = "\n\n";
            std::string testLog = expectMatch + LOG_FULL + LOG_UNMATCH;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n" + LOG_FULL + LOG_UNMATCH + "\n";
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
    }
    { // case: end with part log
        {
            std::string expectMatch
                = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
            std::string testLog
                = expectMatch + LOG_PART + LOG_UNMATCH + "\n" + LOG_PART + LOG_UNMATCH + "\n" + LOG_PART + LOG_UNMATCH;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
            std::string testLog = expectMatch + LOG_PART + LOG_UNMATCH + "\n" + LOG_PART + LOG_UNMATCH + "\n" + LOG_PART
                + LOG_UNMATCH + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_UNMATCH + "\n" + LOG_FULL + LOG_END_STRING + '\n';
            std::string testLog = expectMatch + LOG_PART + LOG_UNMATCH + "\n" + LOG_PART + LOG_UNMATCH + "\n"
                + "2021-08-25T07:00:00.000000000Z";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    }
}

class DockerJsonRemoveLastIncompleteLogMultilineUnittest : public ::testing::Test {
public:
    void TestRemoveLastIncompleteLogWithBeginEnd();
    void TestRemoveLastIncompleteLogWithBegin();
    void TestRemoveLastIncompleteLogWithEnd();
    void SetUp() override { readerOpts.mInputType = FileReaderOptions::InputType::InputContainerStdio; }

private:
    FileReaderOptions readerOpts;
    CollectionPipelineContext ctx;

    const std::string LOG_BEGIN_STRING = "Exception in thread \"main\" java.lang.NullPointerException";
    const std::string LOG_BEGIN_REGEX = R"(Exception.*)";

    const std::string LOG_END_STRING = "    ...23 more";
    const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more.*)";

    const std::string LOG_UNMATCH = "unmatch log";

    std::string BuildLog(const std::string& log, bool isNormalLog = true) {
        if (isNormalLog) {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            writer.StartObject();
            writer.Key("log");
            writer.String((log + "\\n").c_str());
            writer.Key("stream");
            writer.String("stdout");
            writer.Key("time");
            writer.String("2024-02-19T03:49:37.793533014Z");
            writer.EndObject();
            return buffer.GetString();
        } else {
            return R"({"log":")" + log + R"(\n","stream":"stdout","time":"2024-02-19T03:49:37.79)";
        }
    }
};

UNIT_TEST_CASE(DockerJsonRemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithBegin);
UNIT_TEST_CASE(DockerJsonRemoveLastIncompleteLogMultilineUnittest, TestRemoveLastIncompleteLogWithEnd);

void DockerJsonRemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithBegin() {
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(nullptr, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<DockerJsonFileParser>(0);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    { // case: end with begin
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
    }
    { // case: end with unmatch
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n";
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n";
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(2, rollbackLineFeedCount);
        }
    }
    { // case: all unmatch
        {
            std::string expectMatch = "\n\n" + BuildLog(LOG_UNMATCH) + "\n";
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n";
            std::string testLog = expectMatch + BuildLog(LOG_UNMATCH);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
    }
    { // case: end with part log
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_BEGIN_STRING, false)
                + "\n" + BuildLog(LOG_BEGIN_STRING, false) + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_BEGIN_STRING) + "\n" + BuildLog(LOG_BEGIN_STRING, false)
                + "\n" + BuildLog(LOG_BEGIN_STRING, false);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    }
}

void DockerJsonRemoveLastIncompleteLogMultilineUnittest::TestRemoveLastIncompleteLogWithEnd() {
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    MultilineOptions multilineOpts;
    multilineOpts.Init(config, ctx, "");
    LogFileReader logFileReader("dir",
                                "file",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(nullptr, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<DockerJsonFileParser>(0);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    { // case: end with end
        {
            std::string expectMatch
                = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING);
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(0, matchSize);
            APSARA_TEST_EQUAL("", matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING) + '\n';
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
    }
    { // case: end with unmatch
        std::string expectMatch
            = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING) + '\n';
        std::string testLog = expectMatch + BuildLog(LOG_UNMATCH) + "\n";

        int32_t rollbackLineFeedCount = 0;
        int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        const auto& matchLog = std::string(testLog.data(), matchSize);

        APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
        APSARA_TEST_EQUAL(expectMatch, matchLog);
        APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
    }
    { // case: all unmatch
        {
            std::string expectMatch = "\n\n";
            std::string testLog = expectMatch + BuildLog(LOG_UNMATCH);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(1, rollbackLineFeedCount);
        }
        {
            std::string expectMatch = "\n\n" + BuildLog(LOG_UNMATCH) + "\n";
            std::string testLog = expectMatch;

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(0, rollbackLineFeedCount);
        }
    }
    { // case: end with part log
        {
            std::string expectMatch
                = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH, false) + "\n"
                + BuildLog(LOG_UNMATCH, false);

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH, false) + "\n"
                + BuildLog(LOG_UNMATCH, false) + "\n";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
        {
            std::string expectMatch
                = BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_END_STRING) + '\n';
            std::string testLog = expectMatch + BuildLog(LOG_UNMATCH) + "\n" + BuildLog(LOG_UNMATCH, false) + "\n"
                + "2021-08-25T07:00:00.000000000Z";

            int32_t rollbackLineFeedCount = 0;
            int32_t matchSize = logFileReader.RemoveLastIncompleteLog(
                const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
            const auto& matchLog = std::string(testLog.data(), matchSize);

            APSARA_TEST_EQUAL(int32_t(expectMatch.size()), matchSize);
            APSARA_TEST_EQUAL(expectMatch, matchLog);
            APSARA_TEST_EQUAL(3, rollbackLineFeedCount);
        }
    }
}
} // namespace logtail

UNIT_TEST_MAIN
