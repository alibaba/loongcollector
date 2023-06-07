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
#if defined(__linux__)
#include <unistd.h>
#include <iconv.h>
#elif defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#endif
#include <cstdlib>
#include <cstring>
#include <fstream>
#include "reader/LogFileReader.h"
#include "common/util.h"
#include "common/Flags.h"
#include "parser/DelimiterModeFsmParser.h"
#include "logger/Logger.h"
#include "log_pb/sls_logs.pb.h"
#if defined(__linux__)
#include "unittest/UnittestHelper.h"
#endif

using namespace std;
using namespace sls_logs;

DECLARE_FLAG_INT64(first_read_endure_bytes);

namespace logtail {

class LogFileReaderUnittest : public ::testing::Test {

    static const string LOG_BEGIN_TIME;
    static const string LOG_BEGIN_REGEX;
    static const string NEXT_LINE;
    static const size_t BUFFER_SIZE;
    static const size_t SIGNATURE_CHAR_COUNT;
    static string gRootDir;

public:
    void TestLogSplit();
    std::string GenerateRawData(
        int32_t lines, vector<int32_t>& index, int32_t size = 100, bool singleLine = false, bool gbk = false);
};

APSARA_UNIT_TEST_CASE(LogFileReaderUnittest, TestLogSplit, 1);


const string LogFileReaderUnittest::LOG_BEGIN_TIME = "2012-12-12上午 : ";
const string LogFileReaderUnittest::LOG_BEGIN_REGEX = "\\d{4}-\\d{2}-\\d{2}上午.*";
const string LogFileReaderUnittest::NEXT_LINE = "\n";
const size_t LogFileReaderUnittest::BUFFER_SIZE = 1024 * 512;
const size_t LogFileReaderUnittest::SIGNATURE_CHAR_COUNT = 1024;
string LogFileReaderUnittest::gRootDir = "";

string
LogFileReaderUnittest::GenerateRawData(int32_t lines, vector<int32_t>& index, int32_t size, bool singleLine, bool gbk) {
    static string GBK_CHARCTERS[26] = {
#if defined(__linux__)
        "行",
        "请",
        "求",
        "环",
        "节",
        "里",
        "效",
        "率",
        "最",
        "高",
        "的",
        "部",
        "分",
        "这",
        "就",
        "让",
        "每",
        "个",
        "请",
        "求",
        "处",
        "理",
        "速",
        "度",
        "了",
        "多"
#elif defined(_MSC_VER)
        "\xe8\xa1\x8c",
        "\xe8\xaf\xb7",
        "\xe6\xb1\x82",
        "\xe7\x8e\xaf",
        "\xe8\x8a\x82",
        "\xe9\x87\x8c",
        "\xe6\x95\x88",
        "\xe7\x8e\x87",
        "\xe6\x9c\x80",
        "\xe9\xab\x98",
        "\xe7\x9a\x84",
        "\xe9\x83\xa8",
        "\xe5\x88\x86",
        "\xe8\xbf\x99",
        "\xe5\xb0\xb1",
        "\xe8\xae\xa9",
        "\xe6\xaf\x8f",
        "\xe4\xb8\xaa",
        "\xe8\xaf\xb7",
        "\xe6\xb1\x82",
        "\xe5\xa4\x84",
        "\xe7\x90\x86",
        "\xe9\x80\x9f",
        "\xe5\xba\xa6",
        "\xe4\xba\x86",
        "\xe5\xa4\x9a"
#endif
    };
    int32_t prefixSize = (int32_t)LOG_BEGIN_TIME.size();
    if (size < prefixSize + 10)
        size = prefixSize + 10;
    string rawLog;
    for (int32_t i = 0; i < lines; ++i) {
        index.push_back(rawLog.size());
        rawLog += LOG_BEGIN_TIME;
        for (int32_t j = 0; j < size - prefixSize - 1; ++j) {
            int32_t mod = rand() % 26;
            int32_t modValue = rand() % 10;
            if (!singleLine) {
                if (gbk) {
                    if (modValue == 0)
                        rawLog.append("\n");
                    else if (modValue / 2 == 0)
                        rawLog.append(GBK_CHARCTERS[mod]);
                    else
                        rawLog.append(1, char('A' + mod));
                } else {
                    if (modValue == 0)
                        rawLog.append("\n");
                    else
                        rawLog.append(1, char('A' + mod));
                }
            } else {
                if (gbk) {
                    if (modValue / 2 == 0)
                        rawLog.append(GBK_CHARCTERS[mod]);
                    else
                        rawLog.append(1, char('A' + mod));
                } else
                    rawLog.append(1, char('A' + mod));
            }
        }
        rawLog.append(NEXT_LINE);
    }
    return rawLog;
}

void LogFileReaderUnittest::TestLogSplit() {
    LOG_INFO(sLogger, ("TestLogSplit() begin", time(NULL)));
    // this is call after GetRawData(), so buffer must end with '\0'
    // case 0: multiple line
    CommonRegLogFileReader logFileReader("100_proj", "", "", "", INT32_FLAG(default_tail_limit_kb), "", "", "");
    logFileReader.SetLogBeginRegex(LOG_BEGIN_REGEX);
    vector<int32_t> index;
    string multipleLog = GenerateRawData(3, index, 10);
    int32_t multipleLogSize = multipleLog.size();
    char* buffer = new char[multipleLogSize + 1];
    strcpy(buffer, multipleLog.c_str());
    buffer[multipleLogSize - 1] = '\0';
    int32_t lineFeed;
    vector<int32_t> splitIndex = logFileReader.LogSplit(buffer, multipleLogSize, lineFeed);
    APSARA_TEST_EQUAL(index.size(), splitIndex.size());
    for (size_t i = 0; i < index.size() && i < splitIndex.size(); ++i) {
        APSARA_TEST_EQUAL(index[i], splitIndex[i]);
    }
    APSARA_TEST_TRUE(lineFeed >= (int32_t)splitIndex.size());
    delete[] buffer;

    // case 1: single line
    {
        string singleLog = LOG_BEGIN_TIME + "single log";
        int32_t singleLogSize = singleLog.size();
        buffer = new char[singleLogSize + 1];
        strcpy(buffer, singleLog.c_str());
        buffer[singleLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, singleLogSize, lineFeed);
        APSARA_TEST_EQUAL(1, splitIndex.size());
        APSARA_TEST_EQUAL(0, splitIndex[0]);
        APSARA_TEST_EQUAL(1, lineFeed);
        delete[] buffer;
    }

    // case 2: invalid regex beginning
    {
        string invalidLog = "xx" + LOG_BEGIN_TIME + "invalid log 1\nyy" + LOG_BEGIN_TIME + "invalid log 2\n";
        int32_t invalidLogSize = invalidLog.size();
        buffer = new char[invalidLogSize + 1];
        strcpy(buffer, invalidLog.c_str());
        buffer[invalidLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, invalidLogSize, lineFeed);
        APSARA_TEST_EQUAL(0, splitIndex.size());
        APSARA_TEST_EQUAL(2, lineFeed);
        delete[] buffer;
    }


    // case 3: invalid one + valid one, no discard unmatch
    {
        CommonRegLogFileReader logFileReader("100_proj", "", "", "", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false, false);
        logFileReader.SetLogBeginRegex(LOG_BEGIN_REGEX);
        string invalidLog = "xx" + LOG_BEGIN_TIME + "invalid log 1\n" + LOG_BEGIN_TIME + "invalid log 2\n";
        int32_t invalidLogSize = invalidLog.size();
        buffer = new char[invalidLogSize + 1];
        strcpy(buffer, invalidLog.c_str());
        buffer[invalidLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, invalidLogSize, lineFeed);
        APSARA_TEST_EQUAL(2, splitIndex.size());
        APSARA_TEST_EQUAL(0, splitIndex[0]);
        APSARA_TEST_EQUAL(35, splitIndex[1]);
        APSARA_TEST_EQUAL(2, lineFeed);
        delete[] buffer;
    }

    // case 4: invalid one + valid one, discard unmatch
    {
        string invalidLog = "xx" + LOG_BEGIN_TIME + "invalid log 1\n" + LOG_BEGIN_TIME + "invalid log 2\n";
        int32_t invalidLogSize = invalidLog.size();
        buffer = new char[invalidLogSize + 1];
        strcpy(buffer, invalidLog.c_str());
        buffer[invalidLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, invalidLogSize, lineFeed);
        APSARA_TEST_EQUAL(1, splitIndex.size());
        APSARA_TEST_EQUAL(35, splitIndex[0]);
        APSARA_TEST_EQUAL(2, lineFeed);
        delete[] buffer;
    }

    // case 5: valid one + invalid one, no discard unmatch
    {
        CommonRegLogFileReader logFileReader("100_proj", "", "", "", INT32_FLAG(default_tail_limit_kb), "", "", "", ENCODING_UTF8, false, false);
        logFileReader.SetLogBeginRegex(LOG_BEGIN_REGEX);
        string invalidLog = LOG_BEGIN_TIME + "invalid log 1\nyy" + LOG_BEGIN_TIME + "invalid log 2\n";
        int32_t invalidLogSize = invalidLog.size();
        buffer = new char[invalidLogSize + 1];
        strcpy(buffer, invalidLog.c_str());
        buffer[invalidLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, invalidLogSize, lineFeed);
        APSARA_TEST_EQUAL(1, splitIndex.size());
        APSARA_TEST_EQUAL(0, splitIndex[0]);
        APSARA_TEST_EQUAL(2, lineFeed);
        delete[] buffer;
    }

    // case 6: valid one + invalid one, discard unmatch
    {
        string invalidLog = LOG_BEGIN_TIME + "invalid log 1\nyy" + LOG_BEGIN_TIME + "invalid log 2\n";
        int32_t invalidLogSize = invalidLog.size();
        buffer = new char[invalidLogSize + 1];
        strcpy(buffer, invalidLog.c_str());
        buffer[invalidLogSize - 1] = '\0';
        splitIndex = logFileReader.LogSplit(buffer, invalidLogSize, lineFeed);
        APSARA_TEST_EQUAL(1, splitIndex.size());
        APSARA_TEST_EQUAL(0, splitIndex[0]);
        APSARA_TEST_EQUAL(2, lineFeed);
        delete[] buffer;
    }

    LOG_INFO(sLogger, ("TestLogSplit() end", time(NULL)));
}

} // namespace logtail

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
