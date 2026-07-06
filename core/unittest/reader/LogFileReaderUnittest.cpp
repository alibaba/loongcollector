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

#include <cstdio>

#include <chrono>
#include <fstream>
#include <thread>

#include "common/FileSystemUtil.h"
#include "common/RuntimeUtil.h"
#include "file_server/FileServer.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "file_server/reader/JsonLogFileReader.h"
#include "file_server/reader/LogFileReader.h"
#include "monitor/metric_constants/MetricConstants.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(force_release_deleted_file_fd_timeout);

namespace logtail {

class LogFileReaderUnittest : public ::testing::Test {
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

    static void TearDownTestCase() {}

    void SetUp() override {
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        std::string filepath = logPathDir + PATH_SEPARATOR + utf8File;
        std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(filepath.c_str(), "rb"), &std::fclose);
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
    void TestSetExpectedFileSize();
    void TestReloadMetricsGaugeNoZeroDrop();
    void TestReloadMetricsGaugeTruncatedFile();

    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;

protected:
    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogFileReaderUnittest, TestReadGBK);
UNIT_TEST_CASE(LogFileReaderUnittest, TestReadUTF8);
UNIT_TEST_CASE(LogFileReaderUnittest, TestSetExpectedFileSize);
UNIT_TEST_CASE(LogFileReaderUnittest, TestReloadMetricsGaugeNoZeroDrop);
UNIT_TEST_CASE(LogFileReaderUnittest, TestReloadMetricsGaugeTruncatedFile);

std::string LogFileReaderUnittest::logPathDir;
std::string LogFileReaderUnittest::gbkFile;
std::string LogFileReaderUnittest::utf8File;

void LogFileReaderUnittest::TestReadGBK() {
    { // buffer size big enough and match pattern
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
    }
    { // buffer size big enough and match pattern, force read
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, reader.mLogFileOp.GetFileSize(), moreData, false);
        APSARA_TEST_FALSE_FATAL(moreData);
        char* expectedContentAll = expectedContent.get();
        size_t tmp = strlen(expectedContentAll);
        expectedContentAll[tmp + 1] = '\n';
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
        expectedContentAll[tmp + 1] = '\0';
    }
    { // buffer size not big enough and not match pattern
        Json::Value config;
        config["StartPattern"] = "no matching pattern";
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        multilineOpts.Init(config, ctx, "");
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        LogFileReader::BUFFER_SIZE = 14;
        size_t BUFFER_SIZE_UTF8 = 15; // "ilogtail 为可"
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
    { // buffer size not big enough and match pattern
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        // reader.mDiscardUnmatch = false;
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
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1); // exclude tailing \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
    }
    { // read twice, multiline
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        // reader.mDiscardUnmatch = false;
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read, first part should be read
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1);
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        auto lastFilePos = reader.mLastFilePos;
        // second read, end of second part cannot be determined, nothing read
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_EQUAL_FATAL(lastFilePos, reader.mLastFilePos);
    }
    { // read twice, single line
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        // reader.mDiscardUnmatch = false;
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read, first part should be read
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1); // -1 for \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        // second read, second part should be read
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        expectedPart = expectedContent.get();
        expectedPart = expectedPart.substr(expectedPart.rfind("iLogtail"));
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_EQUAL_FATAL(0UL, reader.mCache.size());
    }
    { // empty file
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, 0, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(NULL, logBuffer.rawBuffer.data());
    }
    { // force read + \n, which case read bytes is 0
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        std::string expectedPart(expectedContent.get());
        // first read, read first line without \n and not allowRollback
        int64_t firstReadSize = expectedPart.find("\n");
        expectedPart.resize(firstReadSize);
#if defined(__linux__)
        reader.ReadGBK(logBuffer, 127, moreData, false); // first line without \n
#else
        reader.ReadGBK(logBuffer, 128, moreData, false); // Windows has an extra \r character compared to Linux.
#endif
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_FALSE_FATAL(reader.mLastForceRead);
        reader.ReadGBK(logBuffer, 127, moreData, false); // force read, clear cache
        APSARA_TEST_TRUE_FATAL(reader.mLastForceRead);
        APSARA_TEST_EQUAL_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());

        // second read, start with \n but with other lines
        reader.ReadGBK(logBuffer, fileSize - 1, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        std::string expectedPart2(expectedContent.get() + firstReadSize + 1); // skip \n
        int64_t secondReadSize = expectedPart2.rfind("iLogtail") - 1;
        expectedPart2.resize(secondReadSize);
        APSARA_TEST_STREQ_FATAL(expectedPart2.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_FALSE_FATAL(reader.mLastForceRead);

        // third read, force read cache
        reader.ReadGBK(logBuffer, fileSize - 1, moreData, false);
        std::string expectedPart3(expectedContent.get() + firstReadSize + 1 + secondReadSize + 1);
        APSARA_TEST_STREQ_FATAL(expectedPart3.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_TRUE_FATAL(reader.mLastForceRead);

        // fourth read, only read \n
        LogBuffer logBuffer2;
        reader.ReadGBK(logBuffer2, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_EQUAL_FATAL(fileSize, reader.mLastFilePos);
        APSARA_TEST_STREQ_FATAL(NULL, logBuffer2.rawBuffer.data());
    }
}

void LogFileReaderUnittest::TestReadUTF8() {
    { // buffer size big enough and match pattern
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, reader.mLogFileOp.GetFileSize(), moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
    }
    { // buffer size big enough and match pattern
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, reader.mLogFileOp.GetFileSize(), moreData, false);
        APSARA_TEST_FALSE_FATAL(moreData);
        char* expectedContentAll = expectedContent.get();
        size_t tmp = strlen(expectedContentAll);
        expectedContentAll[tmp + 1] = '\n';
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
        expectedContentAll[tmp + 1] = '\0';
    }
    { // buffer size not big enough and not match pattern
        // should read buffer size
        Json::Value config;
        config["StartPattern"] = "no matching pattern";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        LogFileReader::BUFFER_SIZE = 15;
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
    { // buffer size not big enough and match pattern
        // should read to match pattern
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 13;
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1);
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
    }
    { // read twice, multiline
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 13;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1); // -1 for \n
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        auto lastFilePos = reader.mLastFilePos;
        // second read, end of second part cannot be determined, nothing read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_EQUAL_FATAL(lastFilePos, reader.mLastFilePos);
    }
    { // read twice, singleline
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 13;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        std::string expectedPart(expectedContent.get());
        expectedPart.resize(expectedPart.rfind("iLogtail") - 1);
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        // second read, second part should be read
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        expectedPart = expectedContent.get();
        expectedPart = expectedPart.substr(expectedPart.rfind("iLogtail"));
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_EQUAL_FATAL(0UL, reader.mCache.size());
    }
    { // empty
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, 0, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_STREQ_FATAL(NULL, logBuffer.rawBuffer.data());
    }
    { // force read + \n, which case read bytes is 0
        Json::Value config;
        config["StartPattern"] = "iLogtail.*";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogBuffer logBuffer;
        bool moreData = false;
        std::string expectedPart(expectedContent.get());
        // first read, read first line without \n and not allowRollback
        int64_t firstReadSize = expectedPart.find("\n");
        expectedPart.resize(firstReadSize);
        reader.mLastForceRead = true;
        reader.ReadUTF8(logBuffer, firstReadSize, moreData, false);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_FALSE_FATAL(reader.mLastForceRead);
        reader.ReadUTF8(logBuffer, firstReadSize, moreData, false); // force read, clear cache
        APSARA_TEST_TRUE_FATAL(reader.mLastForceRead);
        APSARA_TEST_EQUAL_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());

        // second read, start with \n but with other lines
        reader.ReadUTF8(logBuffer, fileSize - 1, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        std::string expectedPart2(expectedContent.get() + firstReadSize + 1); // skip \n
        int64_t secondReadSize = expectedPart2.rfind("iLogtail") - 1;
        expectedPart2.resize(secondReadSize);
        APSARA_TEST_STREQ_FATAL(expectedPart2.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_FALSE_FATAL(reader.mLastForceRead);

        // third read, force read cache
        reader.ReadUTF8(logBuffer, fileSize - 1, moreData, false);
        std::string expectedPart3(expectedContent.get() + firstReadSize + 1 + secondReadSize + 1);
        APSARA_TEST_STREQ_FATAL(expectedPart3.c_str(), logBuffer.rawBuffer.data());
        APSARA_TEST_TRUE_FATAL(reader.mLastForceRead);

        // fourth read, only read \n
        LogBuffer logBuffer2;
        reader.ReadUTF8(logBuffer2, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader.mCache.size(), 0UL);
        APSARA_TEST_EQUAL_FATAL(fileSize, reader.mLastFilePos);
        APSARA_TEST_STREQ_FATAL(NULL, logBuffer2.rawBuffer.data());
    }
}

void LogFileReaderUnittest::TestSetExpectedFileSize() {
    // Test 1: SetExpectedFileSize sets the value correctly
    {
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::UTF8;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        // Set expected file size
        int64_t expectedSize = 100;
        reader.SetExpectedFileSize(expectedSize);
        APSARA_TEST_EQUAL_FATAL(reader.mExpectedFileSize, expectedSize);
    }

    // Test 2: GetRawData respects mExpectedFileSize when fileSize > mExpectedFileSize
    {
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::UTF8;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        int64_t actualFileSize = reader.mLogFileOp.GetFileSize();
        APSARA_TEST_TRUE_FATAL(actualFileSize > 0);

        // Set expected file size smaller than actual file size
        int64_t expectedSize = 50;
        reader.SetExpectedFileSize(expectedSize);
        reader.mLastFilePos = 0;

        LogBuffer logBuffer;
        reader.GetRawData(logBuffer, actualFileSize, true);

        // Should read up to expectedSize, not actualFileSize
        APSARA_TEST_LE_FATAL(reader.mLastFilePos, expectedSize);
    }

    // Test 3: GetRawData returns false when mLastFilePos >= mExpectedFileSize
    {
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::UTF8;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        int64_t actualFileSize = reader.mLogFileOp.GetFileSize();
        APSARA_TEST_TRUE_FATAL(actualFileSize > 0);

        // Set expected file size
        int64_t expectedSize = 100;
        reader.SetExpectedFileSize(expectedSize);
        // Set last file pos to expected size (already read to expected size)
        reader.mLastFilePos = expectedSize;

        LogBuffer logBuffer;
        bool moreData = reader.GetRawData(logBuffer, actualFileSize, true);

        // Should return false when already read to expected size
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, expectedSize);
    }

    // Test 4: GetRawData uses min(fileSize, mExpectedFileSize) when both are set
    {
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::UTF8;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        int64_t actualFileSize = reader.mLogFileOp.GetFileSize();
        APSARA_TEST_TRUE_FATAL(actualFileSize > 0);

        // Set expected file size larger than actual file size
        int64_t expectedSize = actualFileSize + 1000;
        reader.SetExpectedFileSize(expectedSize);
        reader.mLastFilePos = 0;

        LogBuffer logBuffer;
        reader.GetRawData(logBuffer, actualFileSize, true);

        // Should use actualFileSize (the smaller one)
        APSARA_TEST_LE_FATAL(reader.mLastFilePos, actualFileSize);
    }

    // Test 5: When mExpectedFileSize is 0, it should not limit reading
    {
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::UTF8;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        reader.CheckFileSignatureAndOffset(true);

        int64_t actualFileSize = reader.mLogFileOp.GetFileSize();
        APSARA_TEST_TRUE_FATAL(actualFileSize > 0);

        // mExpectedFileSize defaults to 0, should not limit reading
        APSARA_TEST_EQUAL_FATAL(reader.mExpectedFileSize, 0);
        reader.mLastFilePos = 0;

        LogBuffer logBuffer;
        reader.GetRawData(logBuffer, actualFileSize, true);

        // Should read normally without size limit
        APSARA_TEST_LE_FATAL(reader.mLastFilePos, actualFileSize);
    }
}

// Regression test for #2632: config reload must not expose a transient 0 on the
// source_size_bytes / read_offset_bytes gauges. On reload a new reader is created for
// the same file, sharing the reentrant metrics record keyed by config name + file
// labels. If the old reader is destroyed before the new one is built, the shared
// record is released and recreated with gauges defaulting to 0. Without seeding, the
// gauge series shows a spurious drop to 0 until the new reader's first ReportMetrics.
void LogFileReaderUnittest::TestReloadMetricsGaugeNoZeroDrop() {
    const std::string configName = "reload_metrics_config_2632";
    MetricLabelsPtr defaultLabels = std::make_shared<MetricLabels>();
    defaultLabels->emplace_back(METRIC_LABEL_KEY_PROJECT, "test_project");
    defaultLabels->emplace_back(METRIC_LABEL_KEY_PIPELINE_NAME, configName);
    std::unordered_map<std::string, MetricType> metricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_SOURCE_SIZE_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
    };
    auto pluginMetricManager = std::make_shared<PluginMetricManager>(
        defaultLabels, metricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    FileServer::GetInstance()->AddPluginMetricManager(configName, pluginMetricManager);
    // RAII guard: remove the manager from the global FileServer singleton on scope exit,
    // including an early return triggered by a *_FATAL (gtest ASSERT_*) assertion, so a
    // mid-test failure cannot leak a stale manager into subsequent test cases.
    std::shared_ptr<void> managerGuard(nullptr, [&configName](void*) {
        FileServer::GetInstance()->RemovePluginMetricManager(configName);
    });

    CollectionPipelineContext reloadCtx;
    reloadCtx.SetConfigName(configName);
    MultilineOptions multilineOpts;
    FileReaderOptions reloadReaderOpts;
    reloadReaderOpts.mInputType = FileReaderOptions::InputType::InputFile;
    // Same dev/inode across readers so both map to the same reentrant metrics record.
    DevInode devInode(1234, 5678);
    // Offset restored from checkpoint; kept within the real file size so source_size
    // (from the actual on-disk file) stays >= read_offset, mirroring a normal reload.
    const int64_t consumedOffset = 128;

    // --- Old reader before reload: gauges hold a real, non-zero read offset. ---
    uint64_t offsetBeforeReload = 0;
    {
        LogFileReader oldReader(logPathDir,
                                utf8File,
                                devInode,
                                std::make_pair(&reloadReaderOpts, &reloadCtx),
                                std::make_pair(&multilineOpts, &reloadCtx),
                                std::make_pair(&fileTagOpts, &reloadCtx));
        oldReader.SetMetrics();
        APSARA_TEST_TRUE_FATAL(oldReader.mSourceReadOffsetBytes != nullptr);
        oldReader.mLastFilePos = consumedOffset;
        oldReader.ReportMetrics(0);
        offsetBeforeReload = oldReader.mSourceReadOffsetBytes->GetValue();
        APSARA_TEST_EQUAL_FATAL(offsetBeforeReload, (uint64_t)consumedOffset);
    }
    // oldReader destroyed -> ReleaseReentrantMetricsRecordRef erases the shared record.

    // --- New reader after reload: fresh record, gauge starts at 0 (the reload bug). ---
    LogFileReader newReader(logPathDir,
                            utf8File,
                            devInode,
                            std::make_pair(&reloadReaderOpts, &reloadCtx),
                            std::make_pair(&multilineOpts, &reloadCtx),
                            std::make_pair(&fileTagOpts, &reloadCtx));
    newReader.SetMetrics();
    APSARA_TEST_TRUE_FATAL(newReader.mSourceReadOffsetBytes != nullptr);
    // Reproduce the bug window: the freshly recreated record reads 0 before seeding.
    APSARA_TEST_EQUAL_FATAL(newReader.mSourceReadOffsetBytes->GetValue(), 0UL);

    // Simulate the checkpoint offset restore performed by InitReader on reload.
    newReader.mLastFilePos = consumedOffset;

    // Fix under test: seed gauges right after the reader is (re)created.
    newReader.InitMetricGauges();

    // No 0 gap: read offset is restored, and source size is the real (non-zero) file size.
    APSARA_TEST_EQUAL_FATAL(newReader.mSourceReadOffsetBytes->GetValue(), (uint64_t)consumedOffset);
    APSARA_TEST_GT_FATAL(newReader.mSourceSizeBytes->GetValue(), 0UL);
    APSARA_TEST_GE_FATAL(newReader.mSourceSizeBytes->GetValue(), newReader.mSourceReadOffsetBytes->GetValue());
    // managerGuard removes the PluginMetricManager on scope exit.
}

// Regression test for #2632 maintainer feedback: when the file is truncated across a
// reload (stat succeeds but reports a size smaller than the checkpoint-restored offset),
// InitMetricGauges must clamp source_size_bytes up to read_offset_bytes so the
// source_size >= read_offset invariant holds and the size gauge does not regress below
// the offset.
void LogFileReaderUnittest::TestReloadMetricsGaugeTruncatedFile() {
    const std::string configName = "reload_metrics_config_2632_truncated";
    MetricLabelsPtr defaultLabels = std::make_shared<MetricLabels>();
    defaultLabels->emplace_back(METRIC_LABEL_KEY_PROJECT, "test_project");
    defaultLabels->emplace_back(METRIC_LABEL_KEY_PIPELINE_NAME, configName);
    std::unordered_map<std::string, MetricType> metricKeys = {
        {METRIC_PLUGIN_OUT_EVENTS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_EVENT_GROUPS_TOTAL, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_OUT_SIZE_BYTES, MetricType::METRIC_TYPE_COUNTER},
        {METRIC_PLUGIN_SOURCE_SIZE_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
        {METRIC_PLUGIN_SOURCE_READ_OFFSET_BYTES, MetricType::METRIC_TYPE_INT_GAUGE},
    };
    auto pluginMetricManager = std::make_shared<PluginMetricManager>(
        defaultLabels, metricKeys, MetricCategory::METRIC_CATEGORY_PLUGIN_SOURCE);
    FileServer::GetInstance()->AddPluginMetricManager(configName, pluginMetricManager);
    std::shared_ptr<void> managerGuard(nullptr, [&configName](void*) {
        FileServer::GetInstance()->RemovePluginMetricManager(configName);
    });

    CollectionPipelineContext reloadCtx;
    reloadCtx.SetConfigName(configName);
    MultilineOptions multilineOpts;
    FileReaderOptions reloadReaderOpts;
    reloadReaderOpts.mInputType = FileReaderOptions::InputType::InputFile;
    DevInode devInode(4321, 8765);

    LogFileReader reader(logPathDir,
                         utf8File,
                         devInode,
                         std::make_pair(&reloadReaderOpts, &reloadCtx),
                         std::make_pair(&multilineOpts, &reloadCtx),
                         std::make_pair(&fileTagOpts, &reloadCtx));
    reader.SetMetrics();
    APSARA_TEST_TRUE_FATAL(reader.mSourceReadOffsetBytes != nullptr);
    APSARA_TEST_TRUE_FATAL(reader.mSourceSizeBytes != nullptr);

    // Restore an offset far beyond the real on-disk file size, simulating a file that
    // was truncated between the two readers while the checkpoint still holds the old,
    // larger offset. stat() succeeds here but returns a size smaller than the offset.
    const int64_t truncatedOffset = 1LL << 30; // 1 GiB, well above the small test file.
    reader.mLastFilePos = truncatedOffset;

    reader.InitMetricGauges();

    // Clamp under test: size gauge is raised to the offset, never below it.
    APSARA_TEST_EQUAL_FATAL(reader.mSourceReadOffsetBytes->GetValue(), (uint64_t)truncatedOffset);
    APSARA_TEST_EQUAL_FATAL(reader.mSourceSizeBytes->GetValue(), (uint64_t)truncatedOffset);
    APSARA_TEST_GE_FATAL(reader.mSourceSizeBytes->GetValue(), reader.mSourceReadOffsetBytes->GetValue());
    // managerGuard removes the PluginMetricManager on scope exit.
}

class LogMultiBytesUnittest : public ::testing::Test {
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

    static void TearDownTestCase() {}

    void SetUp() override {
        std::string filepath = logPathDir + PATH_SEPARATOR + utf8File;
        std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(filepath.c_str(), "rb"), &std::fclose);
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
    void TestAlignLastCharacterUTF8();
    void TestAlignLastCharacterGBK();
    void TestReadUTF8();
    void TestReadGBK();

    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
    FileDiscoveryOptions discoveryOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogMultiBytesUnittest, TestAlignLastCharacterUTF8);
UNIT_TEST_CASE(LogMultiBytesUnittest, TestAlignLastCharacterGBK);
UNIT_TEST_CASE(LogMultiBytesUnittest, TestReadUTF8);
UNIT_TEST_CASE(LogMultiBytesUnittest, TestReadGBK);

std::string LogMultiBytesUnittest::logPathDir;
std::string LogMultiBytesUnittest::gbkFile;
std::string LogMultiBytesUnittest::utf8File;

void LogMultiBytesUnittest::TestAlignLastCharacterUTF8() {
    { // case: no align
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader logFileReader("",
                                    "",
                                    DevInode(),
                                    std::make_pair(&readerOpts, &ctx),
                                    std::make_pair(&multilineOpts, &ctx),
                                    std::make_pair(&fileTagOpts, &ctx));
        std::string expectedLog = "为可观测场景而";
        std::string testLog = expectedLog + "生";
        size_t result = logFileReader.AlignLastCharacter(const_cast<char*>(testLog.data()), expectedLog.size());
        APSARA_TEST_EQUAL_FATAL(expectedLog.size(), result);
    }
    { // case: cut off
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        FileTagOptions fileTagOpts;
        LogFileReader logFileReader("",
                                    "",
                                    DevInode(),
                                    std::make_pair(&readerOpts, &ctx),
                                    std::make_pair(&multilineOpts, &ctx),
                                    std::make_pair(&fileTagOpts, &ctx));
        std::string expectedLog = "为可观测场景而";
        std::string testLog = expectedLog + "生";
        size_t result = logFileReader.AlignLastCharacter(const_cast<char*>(testLog.data()), expectedLog.size() + 1);
        APSARA_TEST_EQUAL_FATAL(expectedLog.size(), result);
    }
}

void LogMultiBytesUnittest::TestAlignLastCharacterGBK() {
    MultilineOptions multilineOpts;
    FileReaderOptions readerOpts;
    readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
    readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
    FileTagOptions fileTagOpts;
    LogFileReader logFileReader("",
                                "",
                                DevInode(),
                                std::make_pair(&readerOpts, &ctx),
                                std::make_pair(&multilineOpts, &ctx),
                                std::make_pair(&fileTagOpts, &ctx));
    { // case: no align
        std::string expectedLog
            = "\xce\xaa\xbf\xc9\xb9\xdb\xb2\xe2\xb3\xa1\xbe\xb0\xb6\xf8"; // equal to "为可观测场景而"
        std::string testLog = expectedLog + "\xc9";
        size_t result = logFileReader.AlignLastCharacter(const_cast<char*>(testLog.data()), expectedLog.size());
        APSARA_TEST_EQUAL_FATAL(expectedLog.size(), result);
    }
    { // case: GBK
        std::string expectedLog
            = "\xce\xaa\xbf\xc9\xb9\xdb\xb2\xe2\xb3\xa1\xbe\xb0\xb6\xf8"; // equal to "为可观测场景而"
        std::string testLog = expectedLog + "\xc9";
        size_t result = logFileReader.AlignLastCharacter(const_cast<char*>(testLog.data()), expectedLog.size() + 1);
        APSARA_TEST_EQUAL_FATAL(expectedLog.size(), result);
    }
}

void LogMultiBytesUnittest::TestReadUTF8() {
    MultilineOptions multilineOpts;
    FileReaderOptions readerOpts;
    readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
    FileTagOptions fileTagOpts;
    LogFileReader reader(logPathDir,
                         utf8File,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));
    LogFileReader::BUFFER_SIZE = 13; // equal to "iLogtail 为" plus one illegal byte
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    int64_t fileSize = reader.mLogFileOp.GetFileSize();
    reader.CheckFileSignatureAndOffset(true);
    LogBuffer logBuffer;
    bool moreData = false;
    reader.ReadUTF8(logBuffer, fileSize, moreData);
    std::string expectedPart(expectedContent.get());
    expectedPart = expectedPart.substr(0, LogFileReader::BUFFER_SIZE - 1);
    APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
}

void LogMultiBytesUnittest::TestReadGBK() {
    MultilineOptions multilineOpts;
    FileReaderOptions readerOpts;
    readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
    readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
    FileTagOptions fileTagOpts;
    LogFileReader reader(logPathDir,
                         gbkFile,
                         DevInode(),
                         std::make_pair(&readerOpts, &ctx),
                         std::make_pair(&multilineOpts, &ctx),
                         std::make_pair(&fileTagOpts, &ctx));
    LogFileReader::BUFFER_SIZE = 12; // equal to "iLogtail 为" plus one illegal byte
    size_t BUFFER_SIZE_UTF8 = 12; // "ilogtail 为可"
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    int64_t fileSize = reader.mLogFileOp.GetFileSize();
    reader.CheckFileSignatureAndOffset(true);
    LogBuffer logBuffer;
    bool moreData = false;
    reader.ReadGBK(logBuffer, fileSize, moreData);
    APSARA_TEST_TRUE_FATAL(moreData);
    std::string expectedPart(expectedContent.get());
    expectedPart = expectedPart.substr(0, BUFFER_SIZE_UTF8);
    APSARA_TEST_STREQ_FATAL(expectedPart.c_str(), logBuffer.rawBuffer.data());
}

class LogFileReaderNoSplitUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == logPathDir.back()) {
            logPathDir.resize(logPathDir.size() - 1);
        }
        logPathDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "LogFileReaderUnittest";
        gbkFile = "gbk.txt";
        utf8File = "utf8.txt";
    }

    static void TearDownTestCase() {}

    void SetUp() override {
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        std::string filepath = logPathDir + PATH_SEPARATOR + utf8File;
        std::unique_ptr<FILE, decltype(&std::fclose)> fp(std::fopen(filepath.c_str(), "rb"), &std::fclose);
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

    void TestReadUTF8NoSplit();
    void TestReadGBKNoSplit();

    std::unique_ptr<char[]> expectedContent;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;

protected:
    FileDiscoveryOptions discoveryOpts;
    FileReaderOptions readerOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogFileReaderNoSplitUnittest, TestReadUTF8NoSplit);
UNIT_TEST_CASE(LogFileReaderNoSplitUnittest, TestReadGBKNoSplit);

std::string LogFileReaderNoSplitUnittest::logPathDir;
std::string LogFileReaderNoSplitUnittest::gbkFile;
std::string LogFileReaderNoSplitUnittest::utf8File;

void LogFileReaderNoSplitUnittest::TestReadUTF8NoSplit() {
    { // WHOLE_FILE, buffer big enough, force read: returns all data with \n preserved (not split by line)
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);

        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, fileSize, moreData, false);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_TRUE_FATAL(logBuffer.rawBuffer.data() != NULL);
        std::string result(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
        APSARA_TEST_TRUE_FATAL(result.find('\n') != std::string::npos);
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
    }
    { // WHOLE_FILE, buffer too small: forced split at buffer boundary, moreData=true
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        LogFileReader::BUFFER_SIZE = 15;
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);

        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        size_t firstReadLen = logBuffer.rawBuffer.size();
        APSARA_TEST_GT_FATAL(firstReadLen, 0UL);
        APSARA_TEST_LE_FATAL(firstReadLen, (size_t)LogFileReader::BUFFER_SIZE);
    }
    { // WHOLE_FILE, buffer too small, read all: combined data equals expected
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader(logPathDir,
                             utf8File,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 13;

        std::string combined;
        bool moreData = true;
        while (moreData) {
            LogBuffer logBuffer;
            reader.ReadUTF8(logBuffer, fileSize, moreData);
            if (logBuffer.rawBuffer.data() != NULL) {
                if (!combined.empty()) {
                    combined += '\n';
                }
                combined.append(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
            }
        }
        if (combined.empty()) {
            LogBuffer logBuffer;
            reader.ReadUTF8(logBuffer, fileSize, moreData, false);
            if (logBuffer.rawBuffer.data() != NULL) {
                combined.append(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
            }
        }
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), combined.c_str());
    }
}

void LogFileReaderNoSplitUnittest::TestReadGBKNoSplit() {
    { // WHOLE_FILE, GBK, buffer big enough, force read: returns all data with \n preserved
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);

        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, fileSize, moreData, false);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_TRUE_FATAL(logBuffer.rawBuffer.data() != NULL);
        std::string result(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
        APSARA_TEST_TRUE_FATAL(result.find('\n') != std::string::npos);
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), logBuffer.rawBuffer.data());
    }
    { // WHOLE_FILE, GBK, buffer too small: forced split, logTooLongSplitFlag triggered
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        LogFileReader::BUFFER_SIZE = 14;
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);

        LogBuffer logBuffer;
        bool moreData = false;
        reader.ReadGBK(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        APSARA_TEST_GT_FATAL(logBuffer.rawBuffer.size(), 0UL);
    }
    { // WHOLE_FILE, GBK, buffer too small, read all: combined data equals expected
        Json::Value config;
        config["Multiline.Mode"] = "whole_file";
        MultilineOptions multilineOpts;
        multilineOpts.Init(config, ctx, "");
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        readerOpts.mFileEncoding = FileReaderOptions::Encoding::GBK;
        LogFileReader reader(logPathDir,
                             gbkFile,
                             DevInode(),
                             std::make_pair(&readerOpts, &ctx),
                             std::make_pair(&multilineOpts, &ctx),
                             std::make_pair(&fileTagOpts, &ctx));
        reader.UpdateReaderManual();
        reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader.mLogFileOp.GetFileSize();
        reader.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 11;

        std::string combined;
        bool moreData = true;
        while (moreData) {
            LogBuffer logBuffer;
            reader.ReadGBK(logBuffer, fileSize, moreData);
            if (logBuffer.rawBuffer.data() != NULL) {
                if (!combined.empty()) {
                    combined += '\n';
                }
                combined.append(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
            }
        }
        if (combined.empty()) {
            LogBuffer logBuffer;
            reader.ReadGBK(logBuffer, fileSize, moreData, false);
            if (logBuffer.rawBuffer.data() != NULL) {
                combined.append(logBuffer.rawBuffer.data(), logBuffer.rawBuffer.size());
            }
        }
        APSARA_TEST_STREQ_FATAL(expectedContent.get(), combined.c_str());
    }
}

class LogFileReaderCheckpointUnittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        logPathDir = GetProcessExecutionDir();
        if (PATH_SEPARATOR[0] == logPathDir.back()) {
            logPathDir.resize(logPathDir.size() - 1);
        }
        logPathDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "LogFileReaderUnittest";
        utf8File = "utf8.txt"; // content of utf8.txt is equivalent to gbk.txt
    }

    void SetUp() override { FileServer::GetInstance()->AddFileDiscoveryConfig("", &discoveryOpts, &ctx); }

    void TearDown() override {
        CheckPointManager::Instance()->RemoveAllCheckPoint();
        LogFileReader::BUFFER_SIZE = 1024 * 512;
        FileServer::GetInstance()->RemoveFileDiscoveryConfig("");
    }

    void TestDumpMetaToMem();

    static std::string logPathDir;
    static std::string utf8File;
    FileDiscoveryOptions discoveryOpts;
    FileTagOptions fileTagOpts;
    CollectionPipelineContext ctx;
};

UNIT_TEST_CASE(LogFileReaderCheckpointUnittest, TestDumpMetaToMem);

std::string LogFileReaderCheckpointUnittest::logPathDir;
std::string LogFileReaderCheckpointUnittest::utf8File;

void LogFileReaderCheckpointUnittest::TestDumpMetaToMem() {
    { // read twice with checkpoint, singleline
        MultilineOptions multilineOpts;
        FileReaderOptions readerOpts;
        readerOpts.mInputType = FileReaderOptions::InputType::InputFile;
        LogFileReader reader1(logPathDir,
                              utf8File,
                              DevInode(),
                              std::make_pair(&readerOpts, &ctx),
                              std::make_pair(&multilineOpts, &ctx),
                              std::make_pair(&fileTagOpts, &ctx));
        reader1.UpdateReaderManual();
        reader1.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
        int64_t fileSize = reader1.mLogFileOp.GetFileSize();
        reader1.CheckFileSignatureAndOffset(true);
        LogFileReader::BUFFER_SIZE = fileSize - 13;
        LogBuffer logBuffer;
        bool moreData = false;
        // first read
        reader1.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_TRUE_FATAL(moreData);
        APSARA_TEST_GE_FATAL(reader1.mCache.size(), 0UL);
        reader1.DumpMetaToMem(false);
        // second read
        LogFileReader reader2(logPathDir,
                              utf8File,
                              DevInode(),
                              std::make_pair(&readerOpts, &ctx),
                              std::make_pair(&multilineOpts, &ctx),
                              std::make_pair(&fileTagOpts, &ctx));
        reader2.UpdateReaderManual();
        reader2.InitReader(false, LogFileReader::BACKWARD_TO_BEGINNING);
        reader2.CheckFileSignatureAndOffset(true);
        APSARA_TEST_EQUAL_FATAL(reader1.mLastFilePos, reader2.mLastFilePos);
        APSARA_TEST_EQUAL_FATAL(reader1.mCache, reader2.mCache); // cache should recoverd from checkpoint
        reader2.ReadUTF8(logBuffer, fileSize, moreData);
        APSARA_TEST_FALSE_FATAL(moreData);
        APSARA_TEST_EQUAL_FATAL(0UL, reader2.mCache.size());
        reader1.DumpMetaToMem(false);
    }
}

class LogFileReaderHoleUnittest : public ::testing::Test {
public:
    void TestReadLogHoleInTheMiddle();
    void TestReadLogHoleOnTheLeft();
    void TestReadLogJsonHoleOnTheRight();

protected:
    static void SetUpTestCase() {
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        gLogName = "test.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1)) {
            gRootDir.resize(gRootDir.size() - 1);
        }
        gRootDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "LogFileReaderHoleUnittest";
        gLogPath = gRootDir + PATH_SEPARATOR + gLogName;
        bfs::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}
    void SetUp() override {
        bfs::create_directories(gRootDir);
        mReaderOpts.mInputType = FileReaderOptions::InputType::InputFile;
        mMultilineOpts.mMode = MultilineOptions::Mode::CUSTOM;
    }
    void TearDown() override { bfs::remove_all(gRootDir); }

    static std::string gRootDir;
    static std::string gLogName;
    static std::string gLogPath;

private:
    const std::string mConfigName = "##1.0##project-0$config-0";
    FileDiscoveryOptions mDiscoveryOpts;
    FileReaderOptions mReaderOpts;
    MultilineOptions mMultilineOpts;
    FileTagOptions mTagOpts;
    CollectionPipelineContext mCtx;
    FileDiscoveryConfig mConfig;
    bool writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), std::fstream::out | std::fstream::trunc | std::ios_base::binary);
        if (!writer) {
            return false;
        }
        writer << logContent;
        writer.close();
        return true;
    }
};

std::string LogFileReaderHoleUnittest::gRootDir;
std::string LogFileReaderHoleUnittest::gLogName;
std::string LogFileReaderHoleUnittest::gLogPath;

void LogFileReaderHoleUnittest::TestReadLogHoleInTheMiddle() {
    std::string content = "a sample " + std::string(1024, '\0') + " log";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content + "\n"));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!reader.ReadLog(logbuf, &event1)); // false means no more data
    APSARA_TEST_TRUE_FATAL(reader.mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(logbuf.rawBuffer, content);
}

void LogFileReaderHoleUnittest::TestReadLogHoleOnTheLeft() {
    std::string content = std::string(1024, '\0') + "a sample log";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content + "\n"));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(!reader.ReadLog(logbuf, &event1)); // false means no more data
    APSARA_TEST_TRUE_FATAL(reader.mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(logbuf.rawBuffer, "a sample log");
}

void LogFileReaderHoleUnittest::TestReadLogJsonHoleOnTheRight() {
    std::string content = "a sample log" + std::string(LogFileReader::BUFFER_SIZE, '\0');
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content + "\n"));
    mMultilineOpts.mMode = MultilineOptions::Mode::JSON;

    JsonLogFileReader reader(gRootDir,
                             gLogName,
                             DevInode(),
                             std::make_pair(&mReaderOpts, &mCtx),
                             std::make_pair(&mMultilineOpts, &mCtx),
                             std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    Event event1(gRootDir, "", EVENT_MODIFY, 0);
    LogBuffer logbuf;
    APSARA_TEST_TRUE_FATAL(reader.ReadLog(logbuf, &event1)); // true means has more data
    APSARA_TEST_TRUE_FATAL(reader.mLogFileOp.IsOpen());
    APSARA_TEST_EQUAL_FATAL(logbuf.rawBuffer, "a sample log");
}

UNIT_TEST_CASE(LogFileReaderHoleUnittest, TestReadLogHoleInTheMiddle);
UNIT_TEST_CASE(LogFileReaderHoleUnittest, TestReadLogHoleOnTheLeft);
UNIT_TEST_CASE(LogFileReaderHoleUnittest, TestReadLogJsonHoleOnTheRight);

class WholeFileOverwriteUnittest : public ::testing::Test {
public:
    void TestOverwriteSameSize();
    void TestOverwriteLargerSize();
    void TestOverwriteSmallerSize();

protected:
    static void SetUpTestCase() {
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        gLogName = "test.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1)) {
            gRootDir.resize(gRootDir.size() - 1);
        }
        gRootDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "WholeFileOverwriteUnittest";
        gLogPath = gRootDir + PATH_SEPARATOR + gLogName;
        bfs::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}
    void SetUp() override {
        bfs::create_directories(gRootDir);
        mReaderOpts.mInputType = FileReaderOptions::InputType::InputFile;
        mMultilineOpts.mMode = MultilineOptions::Mode::WHOLE_FILE;
        mMultilineOpts.mFileWriteMode = MultilineOptions::FileWriteMode::OVERWRITE;
    }
    void TearDown() override { bfs::remove_all(gRootDir); }

    static std::string gRootDir;
    static std::string gLogName;
    static std::string gLogPath;

private:
    FileDiscoveryOptions mDiscoveryOpts;
    FileReaderOptions mReaderOpts;
    MultilineOptions mMultilineOpts;
    FileTagOptions mTagOpts;
    CollectionPipelineContext mCtx;

    bool writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), std::fstream::out | std::fstream::trunc | std::ios_base::binary);
        if (!writer) {
            return false;
        }
        writer << logContent;
        writer.close();
        return true;
    }
};

std::string WholeFileOverwriteUnittest::gRootDir;
std::string WholeFileOverwriteUnittest::gLogName;
std::string WholeFileOverwriteUnittest::gLogPath;

void WholeFileOverwriteUnittest::TestOverwriteSameSize() {
    std::string content1 = R"({"key":"value1","data":"aaa"})";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content1));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    // First read: data goes to cache (WHOLE_FILE mode caches until flush timeout)
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    // Flush timeout: force read cached data
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content1);
    }
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, (int64_t)content1.size());

    // Overwrite with same-size content (same signature prefix)
    std::this_thread::sleep_for(std::chrono::seconds(1)); // ensure mtime changes
    std::string content2 = R"({"key":"value2","data":"bbb"})";
    APSARA_TEST_EQUAL_FATAL(content1.size(), content2.size());
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content2));

    // CheckFileSignatureAndOffset should detect mtime change and reset position
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, 0);
    APSARA_TEST_TRUE_FATAL(reader.mCache.empty());

    // Read again: should get complete new content
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content2);
    }
}

void WholeFileOverwriteUnittest::TestOverwriteLargerSize() {
    std::string content1 = R"({"key":"v1"})";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content1));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content1);
    }

    // Overwrite with larger content
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string content2 = R"({"key":"value2","extra":"more_data_here"})";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content2));

    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, 0);

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content2);
    }
}

void WholeFileOverwriteUnittest::TestOverwriteSmallerSize() {
    std::string content1 = R"({"key":"value1","extra":"some_data"})";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content1));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content1);
    }

    // Overwrite with smaller content
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string content2 = R"({"key":"v2"})";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content2));

    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, 0);

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), content2);
    }
}

UNIT_TEST_CASE(WholeFileOverwriteUnittest, TestOverwriteSameSize);
UNIT_TEST_CASE(WholeFileOverwriteUnittest, TestOverwriteLargerSize);
UNIT_TEST_CASE(WholeFileOverwriteUnittest, TestOverwriteSmallerSize);

class WholeFileOverwriteLargeUnittest : public ::testing::Test {
public:
    void TestLargeFileChunkedDrain();
    void TestOverwriteDuringAccumulation();
    void TestChunkLineAlignment();
    void TestChunkCharAlignment();
    void TestAppendModeNoReset();

protected:
    static void SetUpTestCase() {
        srand(time(NULL));
        gRootDir = GetProcessExecutionDir();
        gLogName = "test_large.log";
        if (PATH_SEPARATOR[0] == gRootDir.at(gRootDir.size() - 1)) {
            gRootDir.resize(gRootDir.size() - 1);
        }
        gRootDir += PATH_SEPARATOR + "testDataSet" + PATH_SEPARATOR + "WholeFileOverwriteLargeUnittest";
        gLogPath = gRootDir + PATH_SEPARATOR + gLogName;
        bfs::remove_all(gRootDir);
    }

    static void TearDownTestCase() {}
    void SetUp() override {
        bfs::create_directories(gRootDir);
        mReaderOpts.mInputType = FileReaderOptions::InputType::InputFile;
        mMultilineOpts.mMode = MultilineOptions::Mode::WHOLE_FILE;
        mMultilineOpts.mFileWriteMode = MultilineOptions::FileWriteMode::OVERWRITE;
    }
    void TearDown() override { bfs::remove_all(gRootDir); }

    static std::string gRootDir;
    static std::string gLogName;
    static std::string gLogPath;

private:
    FileDiscoveryOptions mDiscoveryOpts;
    FileReaderOptions mReaderOpts;
    MultilineOptions mMultilineOpts;
    FileTagOptions mTagOpts;
    CollectionPipelineContext mCtx;

    bool writeLog(const std::string& logPath, const std::string& logContent) {
        std::ofstream writer(logPath.c_str(), std::fstream::out | std::fstream::trunc | std::ios_base::binary);
        if (!writer) {
            return false;
        }
        writer << logContent;
        writer.close();
        return true;
    }
};

std::string WholeFileOverwriteLargeUnittest::gRootDir;
std::string WholeFileOverwriteLargeUnittest::gLogName;
std::string WholeFileOverwriteLargeUnittest::gLogPath;

void WholeFileOverwriteLargeUnittest::TestLargeFileChunkedDrain() {
    // Create content larger than BUFFER_SIZE (512KB)
    std::string content(600 * 1024, 'A'); // 600KB
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    // First read: entire file goes to cache (one-shot read bypasses BUFFER_SIZE)
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
        // Data should be in cache, not emitted (WHOLE_FILE caches until flush timeout)
        APSARA_TEST_TRUE_FATAL(logbuf.rawBuffer.empty());
    }

    // Flush timeout: should trigger chunked drain
    std::string reconstructed;
    int totalChunks = 0;
    bool moreData = true;
    while (moreData) {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        moreData = reader.ReadLog(logbuf, pEvent.get());
        if (!logbuf.rawBuffer.empty()) {
            APSARA_TEST_TRUE_FATAL(logbuf.wholeFileSeq >= 0);
            APSARA_TEST_EQUAL_FATAL(logbuf.wholeFileSeq, totalChunks);
            reconstructed.append(logbuf.rawBuffer.data(), logbuf.rawBuffer.size());
            totalChunks++;
        }
    }
    APSARA_TEST_TRUE_FATAL(totalChunks > 1); // must have been split
    APSARA_TEST_EQUAL_FATAL(reconstructed, content);
    // All chunks should report the same total
    APSARA_TEST_EQUAL_FATAL(
        totalChunks, static_cast<int>((content.size() + LogFileReader::BUFFER_SIZE - 1) / LogFileReader::BUFFER_SIZE));
}

void WholeFileOverwriteLargeUnittest::TestOverwriteDuringAccumulation() {
    // Write initial content and read it fully
    std::string content1(600 * 1024, 'B');
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content1));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    // Read into cache
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    // Drain first content
    {
        bool moreData = true;
        while (moreData) {
            auto pEvent = reader.CreateFlushTimeoutEvent();
            LogBuffer logbuf;
            moreData = reader.ReadLog(logbuf, pEvent.get());
        }
    }

    // Overwrite with new content
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string content2(700 * 1024, 'C');
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content2));

    // CheckFileSignatureAndOffset should detect mtime change and reset
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, 0);
    APSARA_TEST_TRUE_FATAL(reader.mCache.empty());

    // Read new content
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }

    // Drain and verify we get new content, not old
    std::string reconstructed;
    {
        bool moreData = true;
        while (moreData) {
            auto pEvent = reader.CreateFlushTimeoutEvent();
            LogBuffer logbuf;
            moreData = reader.ReadLog(logbuf, pEvent.get());
            if (!logbuf.rawBuffer.empty()) {
                reconstructed.append(logbuf.rawBuffer.data(), logbuf.rawBuffer.size());
            }
        }
    }
    APSARA_TEST_EQUAL_FATAL(reconstructed, content2);
}

void WholeFileOverwriteLargeUnittest::TestChunkLineAlignment() {
    // Multi-line content larger than BUFFER_SIZE: every non-final chunk must end on a line boundary
    // so each chunk shown downstream (no reassembly) contains only whole lines.
    std::string content;
    content.reserve(600 * 1024);
    int lineNo = 0;
    while (content.size() < 600UL * 1024) {
        std::string line = "this is log line number " + ToString(lineNo++) + " with some padding text here\n";
        content += line;
    }
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }

    std::vector<std::string> chunks;
    bool moreData = true;
    while (moreData) {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        moreData = reader.ReadLog(logbuf, pEvent.get());
        if (!logbuf.rawBuffer.empty()) {
            chunks.emplace_back(logbuf.rawBuffer.data(), logbuf.rawBuffer.size());
        }
    }
    APSARA_TEST_TRUE_FATAL(chunks.size() > 1); // must have been split
    std::string reconstructed;
    for (size_t i = 0; i < chunks.size(); ++i) {
        reconstructed += chunks[i];
        if (i + 1 < chunks.size()) {
            // every non-final chunk ends on a complete line
            APSARA_TEST_EQUAL_FATAL(chunks[i].back(), '\n');
            APSARA_TEST_TRUE_FATAL(chunks[i].size() <= LogFileReader::BUFFER_SIZE);
        }
    }
    APSARA_TEST_EQUAL_FATAL(reconstructed, content);
}

void WholeFileOverwriteLargeUnittest::TestChunkCharAlignment() {
    // A single huge line of multibyte UTF-8 characters (no newline) must never be split inside a
    // character: every chunk length stays a multiple of the 3-byte character width.
    const std::string ch = "\xE4\xB8\xAD"; // "中", 3 bytes
    std::string content;
    content.reserve(600 * 1024);
    while (content.size() < 600UL * 1024) {
        content += ch;
    }
    APSARA_TEST_TRUE_FATAL(content.size() % 3 == 0);
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&mReaderOpts, &mCtx),
                         std::make_pair(&mMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }

    std::string reconstructed;
    bool moreData = true;
    int totalChunks = 0;
    while (moreData) {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        moreData = reader.ReadLog(logbuf, pEvent.get());
        if (!logbuf.rawBuffer.empty()) {
            // chunk boundary never splits a 3-byte character
            APSARA_TEST_EQUAL_FATAL(logbuf.rawBuffer.size() % 3, 0UL);
            reconstructed.append(logbuf.rawBuffer.data(), logbuf.rawBuffer.size());
            totalChunks++;
        }
    }
    APSARA_TEST_TRUE_FATAL(totalChunks > 1);
    APSARA_TEST_EQUAL_FATAL(reconstructed, content);
}

void WholeFileOverwriteLargeUnittest::TestAppendModeNoReset() {
    // Test that append mode does NOT reset position on mtime change
    FileReaderOptions appendOpts;
    appendOpts.mInputType = FileReaderOptions::InputType::InputFile;
    MultilineOptions appendMultilineOpts;
    appendMultilineOpts.mMode = MultilineOptions::Mode::WHOLE_FILE;
    appendMultilineOpts.mFileWriteMode = MultilineOptions::FileWriteMode::APPEND;

    std::string content1 = "initial content\n";
    APSARA_TEST_TRUE_FATAL(writeLog(gLogPath, content1));

    LogFileReader reader(gRootDir,
                         gLogName,
                         DevInode(),
                         std::make_pair(&appendOpts, &mCtx),
                         std::make_pair(&appendMultilineOpts, &mCtx),
                         std::make_pair(&mTagOpts, &mCtx));
    reader.UpdateReaderManual();
    reader.InitReader(true, LogFileReader::BACKWARD_TO_BEGINNING);
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));

    // Read initial content
    {
        Event event(gRootDir, "", EVENT_MODIFY, 0);
        LogBuffer logbuf;
        reader.ReadLog(logbuf, &event);
    }
    {
        auto pEvent = reader.CreateFlushTimeoutEvent();
        LogBuffer logbuf;
        reader.ReadLog(logbuf, pEvent.get());
        APSARA_TEST_EQUAL_FATAL(std::string(logbuf.rawBuffer.data(), logbuf.rawBuffer.size()), "initial content");
    }
    int64_t posAfterFirstRead = reader.mLastFilePos;
    APSARA_TEST_TRUE_FATAL(posAfterFirstRead > 0);

    // Append more content (mtime changes)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    {
        std::ofstream writer(gLogPath.c_str(), std::fstream::out | std::fstream::app | std::ios_base::binary);
        writer << "appended content\n";
        writer.close();
    }

    // CheckFileSignatureAndOffset should NOT reset position for append mode
    APSARA_TEST_TRUE_FATAL(reader.CheckFileSignatureAndOffset(true));
    APSARA_TEST_EQUAL_FATAL(reader.mLastFilePos, posAfterFirstRead);
}

UNIT_TEST_CASE(WholeFileOverwriteLargeUnittest, TestLargeFileChunkedDrain);
UNIT_TEST_CASE(WholeFileOverwriteLargeUnittest, TestOverwriteDuringAccumulation);
UNIT_TEST_CASE(WholeFileOverwriteLargeUnittest, TestChunkLineAlignment);
UNIT_TEST_CASE(WholeFileOverwriteLargeUnittest, TestChunkCharAlignment);
UNIT_TEST_CASE(WholeFileOverwriteLargeUnittest, TestAppendModeNoReset);

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
