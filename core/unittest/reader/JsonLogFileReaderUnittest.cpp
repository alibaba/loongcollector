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
#include <stdio.h>
#include <fstream>
#include "reader/JsonLogFileReader.h"
#include "common/RuntimeUtil.h"
#include "common/FileSystemUtil.h"
#include "util.h"

DECLARE_FLAG_INT32(force_release_deleted_file_fd_timeout);

namespace logtail {

class JsonLogFileReaderUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == logPathDir.back()) {
            logPathDir.resize(logPathDir.size() - 1);
        }
        logPathDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "JsonLogFileReaderUnittest";
        gbkFile = "gbk.txt";
        utf8File = "utf8.txt"; // content of utf8.txt is equivalent to gbk.txt
    }

    static void TearDownTestCase() {
        remove(gbkFile.c_str());
        remove(utf8File.c_str());
    }

    void SetUp() override {
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
    void TearDown() override { LogFileReader::BUFFER_SIZE = 1024 * 512; }
    void TestReadGBK();
    void TestReadUTF8();

    std::string projectName = "projectName";
    std::string category = "logstoreName";
    std::string timeFormat = "";
    std::string topicFormat = "";
    std::string groupTopic = "";
    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
};

UNIT_TEST_CASE(JsonLogFileReaderUnittest, TestReadGBK);
UNIT_TEST_CASE(JsonLogFileReaderUnittest, TestReadUTF8);

std::string JsonLogFileReaderUnittest::logPathDir;
std::string JsonLogFileReaderUnittest::gbkFile;
std::string JsonLogFileReaderUnittest::utf8File;

void JsonLogFileReaderUnittest::TestReadGBK() {
    { // buffer size big enough and is json
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 gbkFile,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_GBK,
                                 false,
                                 false);
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        std::string recovered = logBuffer.rawBuffer.to_string();
        std::replace(recovered.begin(), recovered.end(), '\0', '\n');
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), recovered.c_str());
    }
    { // buffer size not big enough to hold any json
      // should read buffer size
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 gbkFile,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_GBK,
                                 false,
                                 false);
        LogFileReader::BUFFER_SIZE = 23;
        size_t BUFFER_SIZE_UTF8 = 25; // "{"first":"iLogtail 为可"
        reader.SetLogMultilinePolicy("no matching pattern", ".*", ".*");
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(std::string(expectedContent.get(), BUFFER_SIZE_UTF8).c_str(),
                                logBuffer.rawBuffer.data());
    }
    { // buffer size not big enough to hold all json
        // should read until last json
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 gbkFile,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_GBK,
                                 false,
                                 false);
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind(R"({"second")") - 1); // exclude tailing \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
    }
}

void JsonLogFileReaderUnittest::TestReadUTF8() {
    { // buffer size big enough and is json
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 utf8File,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_UTF8,
                                 false,
                                 false);
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        std::string recovered = logBuffer.rawBuffer.to_string();
        std::replace(recovered.begin(), recovered.end(), '\0', '\n');
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), recovered.c_str());
    }
    { // buffer size not big enough to hold any json
      // should read buffer size
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 utf8File,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_UTF8,
                                 false,
                                 false);
        LogFileReader::BUFFER_SIZE = 25;
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(std::string(expectedContent.get(), LogFileReader::BUFFER_SIZE).c_str(),
                                logBuffer.rawBuffer.data());
    }
    { // buffer size not big enough to hold all json
        // should read until last json
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 utf8File,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_UTF8,
                                 false,
                                 false);
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind(R"({"second")") - 1); // exclude tailing \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
    }
    { // read twice
        JsonLogFileReader reader(projectName,
                                 category,
                                 logPathDir,
                                 utf8File,
                                 INT32_FLAG(default_tail_limit_kb),
                                 timeFormat,
                                 topicFormat,
                                 groupTopic,
                                 FileEncoding::ENCODING_UTF8,
                                 false,
                                 false);
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind(R"({"second")") - 1); // exclude tailing \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        // second read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        expectedPart = expectedContent.get();
        expectedPart = expectedPart.substr(expectedPart.rfind(R"({"second")"));
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_EQUAL_FATAL(0UL, reader.mCache.size());
    }
}

class LastMatchedLineUnittest : public ::testing::Test {
public:
    LastMatchedLineUnittest()
        : mLogFileReader("project",
                         "logstore",
                         "dir",
                         "file",
                         INT32_FLAG(default_tail_limit_kb),
                         "",
                         "",
                         "",
                         FileEncoding::ENCODING_UTF8,
                         false,
                         false) {}

    void TestLastMatchedLineSingleLine();
    void TestLastMatchedLineSingleLineIncomplete();
    void TestLastMatchedLineSingleLineIncompleteNoRollback();
    void TestLastMatchedLineMultiline();
    void TestLastMatchedLineMultilineIncomplete();
    void TestLastMatchedLineMultilineIncompleteNoRollback();
    void TestLastMatchedLineNotValidJson();
    void TestLastMatchedLineNotValidJsonNoRollback();

    JsonLogFileReader mLogFileReader;
};

UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineSingleLine);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineSingleLineIncomplete);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineSingleLineIncompleteNoRollback);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineMultiline);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineMultilineIncomplete);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineMultilineIncompleteNoRollback);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineNotValidJson);
UNIT_TEST_CASE(LastMatchedLineUnittest, TestLastMatchedLineNotValidJsonNoRollback);

void LastMatchedLineUnittest::TestLastMatchedLineSingleLine() {
    { // case single line
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third value"})";
        std::string expectMatch = line1 + '\0' + line2 + '\0' + line3 + '\0';
        std::string testLog = line1 + '\n' + line2 + '\n' + line3 + '\n';
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineSingleLineIncomplete() {
    { // case single line, buffer size not big enough, json truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third)";
        std::string expectMatch = line1 + '\0' + line2 + '\0';
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case single line, buffer size not big enough, newline truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third value"}))";
        std::string expectMatch = line1 + '\0' + line2 + '\0';
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineSingleLineIncompleteNoRollback() {
    { // case single line, buffer size not big enough, json truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third)";
        std::string expectMatch = line1 + '\0' + line2 + '\0' + line3;
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case single line, buffer size not big enough, newline truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third value"}))";
        std::string expectMatch = line1 + '\0' + line2 + '\0' + line3;
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineMultiline() {
    { // case multi line
        std::vector<int32_t> index;
        std::string firstLog = R"({
    "key": "first value"
})";
        std::string secondLog = R"({
    "key": {
        "nested_key": "second value"
    }
})";
        std::string expectMatch = firstLog + '\0' + secondLog + '\0';
        std::string testLog = firstLog + '\n' + secondLog + '\n';
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineMultilineIncomplete() {
    { // case multi line, buffer size not enough, json truncated
        std::vector<int32_t> index;
        std::string firstLog = R"({
    "key": "first value"
})";
        std::string secondLog = R"({
    "key": {
        "nested_key": "second value"
    })";
        std::string expectMatch = firstLog + '\0';
        std::string testLog = firstLog + '\n' + secondLog + '\n';
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(4, rollbackLineFeedCount);
    }
    { // case multi line, buffer size not enough, newline truncated
        std::vector<int32_t> index;
        std::string firstLog = R"({
    "key": "first value"
})";
        std::string secondLog = R"({
    "key": {
        "nested_key": "second value"
    }
})";
        std::string expectMatch = firstLog + '\0';
        std::string testLog = firstLog + '\n' + secondLog;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(5, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineMultilineIncompleteNoRollback() {
    { // case multi line, buffer size not enough, json truncated
        std::vector<int32_t> index;
        std::string firstLog = R"({
    "key": "first value"
})";
        std::string secondLog = R"({
    "key": {
        "nested_key": "second value"
    })";
        std::string splittedSecondLog = secondLog;
        std::replace(splittedSecondLog.begin(), splittedSecondLog.end(), '\n', '\0');
        std::string expectMatch = firstLog + '\0' + splittedSecondLog + '\0';
        std::string testLog = firstLog + '\n' + secondLog + '\n';
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case multi line, buffer size not enough, newline truncated
        std::vector<int32_t> index;
        std::string firstLog = R"({
    "key": "first value"
})";
        std::string secondLog = R"({
    "key": {
        "nested_key": "second value"
    }
})";
        std::string expectMatch = firstLog + '\0' + secondLog;
        std::string testLog = firstLog + '\n' + secondLog;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineNotValidJson() {
    { // case not json, skip all
        std::string testLog = "not a json at all.\nnot a json at all.\nnot a json at all.\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(testLog.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case partial json at end, rollback
        std::string notjson = "not a json at all.\nnot a json at all.\n";
        std::string expectMatch = notjson;
        std::replace(expectMatch.begin(), expectMatch.end(), '\n', '\0');
        std::string testLog = "not a json at all.\nnot a json at all.\n{partial json\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
    { // case json at begin, invalid at end, rollback
        std::string firstLog = R"({
    "key": {
        "nested_key": "second value"
    }
})";
        std::string notjson = "not a json at all.\nnot a json at all.\n";
        std::string testLog = firstLog + '\n' + notjson;
        std::replace(notjson.begin(), notjson.end(), '\n', '\0');
        std::string expectMatch = firstLog + '\0' + notjson;
        ;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize
            = mLogFileReader.LastMatchedLine(const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void LastMatchedLineUnittest::TestLastMatchedLineNotValidJsonNoRollback() {
    { // case not json
        std::string testLog = "not a json at all.\nnot a json at all.\nnot a json at all.\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(testLog.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case partial json at end
        std::string testLog = "not a json at all.\nnot a json at all.\n{partial json\n";
        std::string expectMatch = testLog;
        std::replace(expectMatch.begin(), expectMatch.end(), '\n', '\0');
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case json at begin, invalid at end
        std::string firstLog = R"({
    "key": {
        "nested_key": "second value"
    }
})";
        std::string notjson = "not a json at all.\nnot a json at all.\n";
        std::string testLog = firstLog + '\n' + notjson;
        std::replace(notjson.begin(), notjson.end(), '\n', '\0');
        std::string expectMatch = firstLog + '\0' + notjson;
        ;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader.LastMatchedLine(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

class LogSplitUnittest : public ::testing::Test {
public:
    void TestLogSplitSingleLine();
    void TestLogSplitMultiLine();
};

UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitSingleLine);
UNIT_TEST_CASE(LogSplitUnittest, TestLogSplitMultiLine);

void LogSplitUnittest::TestLogSplitSingleLine() {
    JsonLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false);
    std::string line1 = R"({"key": "first value"})";
    std::string line2 = R"({"key": "first value"})";
    std::string line3 = R"({"key": "first value"})";
    std::string testLog = line1 + '\0' + line2 + '\0' + line3;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(3UL, index.size());
    APSARA_TEST_EQUAL_FATAL(line1, index[0].to_string());
    APSARA_TEST_EQUAL_FATAL(line2, index[1].to_string());
    APSARA_TEST_EQUAL_FATAL(line3, index[2].to_string());
}

void LogSplitUnittest::TestLogSplitMultiLine() {
    JsonLogFileReader logFileReader(
        "project", "logstore", "dir", "file", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false);
    std::string line1 = R"({
    "key": "first value"
})";
    std::string line2 = R"(
    "key": "first value"
})";
    std::string line3 = R"({
    "key": "first value"
})";
    std::string testLog = line1 + '\0' + line2 + '\0' + line3;
    int32_t lineFeed = 0;
    std::vector<StringView> index;
    std::vector<StringView> discard;
    bool splitSuccess = logFileReader.LogSplit(testLog.data(), testLog.size(), lineFeed, index, discard);
    APSARA_TEST_TRUE_FATAL(splitSuccess);
    APSARA_TEST_EQUAL_FATAL(3UL, index.size());
    APSARA_TEST_EQUAL_FATAL(line1, index[0].to_string());
    APSARA_TEST_EQUAL_FATAL(line2, index[1].to_string());
    APSARA_TEST_EQUAL_FATAL(line3, index[2].to_string());
}

class JsonParseLogLineUnittest : public ::testing::Test {
public:
    void TestCanBeParsed();
    void TestCanNotBeParsedUnDiscard();
    void TestCanNotBeParsedDiscard();

    static void SetUpTestCase() { BOOL_FLAG(ilogtail_discard_old_data) = false; }
};

UNIT_TEST_CASE(JsonParseLogLineUnittest, TestCanBeParsed);
UNIT_TEST_CASE(JsonParseLogLineUnittest, TestCanNotBeParsedUnDiscard);
UNIT_TEST_CASE(JsonParseLogLineUnittest, TestCanNotBeParsedDiscard);

void JsonParseLogLineUnittest::TestCanBeParsed() {
    JsonLogFileReader logFileReader("project",
                                    "logstore",
                                    "dir",
                                    "file",
                                    INT32_FLAG(default_tail_limit_kb),
                                    "",
                                    "",
                                    "",
                                    ENCODING_UTF8,
                                    false,
                                    false);
    sls_logs::LogGroup logGroup;
    ParseLogError error;
    LogtailTime lastLogLineTime = {0, 0};
    std::string lastLogTimeStr = "";
    uint32_t logGroupSize = 0;
    std::string testLog = "{\n"
                          "\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\n"
                          "\"time\": \"07/Jul/2022:10:30:28\"\n}";
    bool successful
        = logFileReader.ParseLogLine(testLog, logGroup, error, lastLogLineTime, lastLogTimeStr, logGroupSize);
    APSARA_TEST_TRUE_FATAL(successful);
    APSARA_TEST_EQUAL_FATAL(logGroupSize, 86U);
}

void JsonParseLogLineUnittest::TestCanNotBeParsedUnDiscard() {
    JsonLogFileReader logFileReader("project",
                                    "logstore",
                                    "dir",
                                    "file",
                                    INT32_FLAG(default_tail_limit_kb),
                                    "",
                                    "",
                                    "",
                                    ENCODING_UTF8,
                                    false,
                                    false);
    sls_logs::LogGroup logGroup;
    ParseLogError error;
    LogtailTime lastLogLineTime = {0, 0};
    std::string lastLogTimeStr = "";
    uint32_t logGroupSize = 0;
    logFileReader.mDiscardUnmatch = false;
    std::string testLog = "{\n"
                          "\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\n"
                          "\"time\": \n}";
    bool successful
        = logFileReader.ParseLogLine(testLog, logGroup, error, lastLogLineTime, lastLogTimeStr, logGroupSize);
    APSARA_TEST_FALSE_FATAL(successful);
    APSARA_TEST_EQUAL_FATAL(logGroupSize, 88U);
}

void JsonParseLogLineUnittest::TestCanNotBeParsedDiscard() {
    JsonLogFileReader logFileReader("project",
                                    "logstore",
                                    "dir",
                                    "file",
                                    INT32_FLAG(default_tail_limit_kb),
                                    "",
                                    "",
                                    "",
                                    ENCODING_UTF8,
                                    false,
                                    false);
    sls_logs::LogGroup logGroup;
    ParseLogError error;
    LogtailTime lastLogLineTime = {0, 0};
    std::string lastLogTimeStr = "";
    uint32_t logGroupSize = 0;
    logFileReader.mDiscardUnmatch = true;
    std::string testLog = "{\n"
                          "\"url\": \"POST /PutData?Category=YunOsAccountOpLog HTTP/1.1\",\n"
                          "\"time\": \n}";
    bool successful
        = logFileReader.ParseLogLine(testLog, logGroup, error, lastLogLineTime, lastLogTimeStr, logGroupSize);
    APSARA_TEST_FALSE_FATAL(successful);
    APSARA_TEST_EQUAL_FATAL(logGroupSize, 0U);
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}