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

#include <stdio.h>

#include <fstream>

#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "file_server/FileServer.h"
#include "file_server/reader/JsonLogFileReader.h"
#include "unittest/Unittest.h"

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
        FileServer::GetInstance()->AddFileDiscoveryConfig("", &discoveryOpts, &ctx);
    }
    void TearDown() override {
        LogFileReader::BUFFER_SIZE = 1024 * 512;
        FileServer::GetInstance()->RemoveFileDiscoveryConfig("");
    }
    void TestReadGBK();
    void TestReadUTF8();

    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
    FileDiscoveryOptions discoveryOpts;
    PipelineContext ctx;
};

UNIT_TEST_CASE(JsonLogFileReaderUnittest, TestReadGBK)
UNIT_TEST_CASE(JsonLogFileReaderUnittest, TestReadUTF8)

std::string JsonLogFileReaderUnittest::logPathDir;
std::string JsonLogFileReaderUnittest::gbkFile;
std::string JsonLogFileReaderUnittest::utf8File;

void JsonLogFileReaderUnittest::TestReadGBK() {
    { // buffer size big enough and is json
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        JsonLogFileReader reader(
            logPathDir, gbkFile, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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
        Json::Value config;
        config["StartPattern"] = "no matching pattern";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        JsonLogFileReader reader(
            logPathDir, gbkFile, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        LogFileReader::BUFFER_SIZE = 23;
        size_t BUFFER_SIZE_UTF8 = 25; // "{"first":"iLogtail 为可"
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
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        JsonLogFileReader reader(
            logPathDir, gbkFile, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        JsonLogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        JsonLogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        JsonLogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        JsonLogFileReader reader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
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

class RemoveLastIncompleteLogUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mLogFileReader.reset(new JsonLogFileReader(
            "dir", "file", DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx)));
    }

    void TestRemoveLastIncompleteLogSingleLine();
    void TestRemoveLastIncompleteLogSingleLineIncomplete();
    void TestRemoveLastIncompleteLogSingleLineIncompleteNoRollback();
    void TestRemoveLastIncompleteLogMultiline();
    void TestRemoveLastIncompleteLogMultilineIncomplete();
    void TestRemoveLastIncompleteLogMultilineIncompleteNoRollback();
    void TestRemoveLastIncompleteLogNotValidJson();
    void TestRemoveLastIncompleteLogNotValidJsonNoRollback();

    std::unique_ptr<JsonLogFileReader> mLogFileReader;
    MultilineOptions multilineOpts;
    FileReaderOptions readerOpts;
    PipelineContext ctx;
};

UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogSingleLine)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogSingleLineIncomplete)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogSingleLineIncompleteNoRollback)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogMultiline)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogMultilineIncomplete)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogMultilineIncompleteNoRollback)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogNotValidJson)
UNIT_TEST_CASE(RemoveLastIncompleteLogUnittest, TestRemoveLastIncompleteLogNotValidJsonNoRollback)

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogSingleLine() {
    { // case single line
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third value"})";
        std::string expectMatch = line1 + '\0' + line2 + '\0' + line3 + '\0';
        std::string testLog = line1 + '\n' + line2 + '\n' + line3 + '\n';
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogSingleLineIncomplete() {
    { // case single line, buffer size not big enough, json truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third)";
        std::string expectMatch = line1 + '\0' + line2 + '\0';
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(1, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogSingleLineIncompleteNoRollback() {
    { // case single line, buffer size not big enough, json truncated
        std::string line1 = R"({"key": "first value"})";
        std::string line2 = R"({"key": "second value"})";
        std::string line3 = R"({"key": "third)";
        std::string expectMatch = line1 + '\0' + line2 + '\0' + line3;
        std::string testLog = line1 + '\n' + line2 + '\n' + line3;
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogMultiline() {
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogMultilineIncomplete() {
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(5, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogMultilineIncompleteNoRollback() {
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogNotValidJson() {
    { // case not json, skip all
        std::string testLog = "not a json at all.\nnot a json at all.\nnot a json at all.\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(testLog.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case partial json at end, rollback
        std::string notjson = "not a json at all.\nnot a json at all.\n";
        std::string expectMatch = notjson;
        std::replace(expectMatch.begin(), expectMatch.end(), '\n', '\0');
        std::string testLog = "not a json at all.\nnot a json at all.\n{partial json\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

void RemoveLastIncompleteLogUnittest::TestRemoveLastIncompleteLogNotValidJsonNoRollback() {
    { // case not json
        std::string testLog = "not a json at all.\nnot a json at all.\nnot a json at all.\n";
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(testLog.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
    { // case partial json at end
        std::string testLog = "not a json at all.\nnot a json at all.\n{partial json\n";
        std::string expectMatch = testLog;
        std::replace(expectMatch.begin(), expectMatch.end(), '\n', '\0');
        int32_t rollbackLineFeedCount = 0;
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
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
        size_t matchSize = mLogFileReader->RemoveLastIncompleteLog(
            const_cast<char*>(testLog.data()), testLog.size(), rollbackLineFeedCount, false);
        APSARA_TEST_EQUAL_FATAL(expectMatch.size(), matchSize);
        APSARA_TEST_EQUAL_FATAL(std::string(testLog.data(), matchSize), expectMatch);
        APSARA_TEST_EQUAL_FATAL(0, rollbackLineFeedCount);
    }
}

} // namespace logtail

UNIT_TEST_MAIN
