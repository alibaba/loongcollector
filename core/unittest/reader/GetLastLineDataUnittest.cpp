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

#include "common/FileSystemUtil.h"
#include "reader/LogFileReader.h"
#include "common/memory/SourceBuffer.h"
#include "unittest/Unittest.h"

namespace logtail {

const std::string LOG_PART = "2021-08-25T07:00:00.000000000Z stdout P ";
const std::string LOG_FULL = "2021-08-25T07:00:00.000000000Z stdout F ";
const std::string LOG_FULL_NOT_FOUND = "2021-08-25T07:00:00.000000000Z stdout ";
const std::string LOG_ERROR = "2021-08-25T07:00:00.000000000Z stdout";

class LastMatchedContainerdTextLineUnittest : public ::testing::Test {
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

    void TestLastContainerdTextLineMerge();
    void TestLastContainerdTextLineSingleLine();

    std::unique_ptr<char[]> expectedContent;
    FileReaderOptions readerOpts;
    PipelineContext ctx;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
};

UNIT_TEST_CASE(LastMatchedContainerdTextLineUnittest, TestLastContainerdTextLineSingleLine);
UNIT_TEST_CASE(LastMatchedContainerdTextLineUnittest, TestLastContainerdTextLineMerge);

std::string LastMatchedContainerdTextLineUnittest::logPathDir;
std::string LastMatchedContainerdTextLineUnittest::gbkFile;
std::string LastMatchedContainerdTextLineUnittest::utf8File;

void LastMatchedContainerdTextLineUnittest::TestLastContainerdTextLineSingleLine() {
    {
        MultilineOptions multilineOpts;
        LogFileReader logFileReader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        BaseLineParse* baseLineParsePtr = nullptr;
        baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
        logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
        // 异常情况+有回车
        {
            // case: PartLogFlag存在，第三个空格存在但空格后无内容
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P \n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag存在，第三个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag不存在，第二个空格存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout \n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第二个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL(testLog.substr(0, size - 1), line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第一个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00stdout\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL(testLog.substr(0, size - 1), line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
        }
        // 异常情况+无回车
        {
            // case: PartLogFlag存在，第三个空格存在但空格后无内容
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P ";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag存在，第三个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag不存在，第二个空格存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout ";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第二个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL(testLog, line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第一个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00stdout";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

                APSARA_TEST_EQUAL(testLog, line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
        }
        // case: F + P + P
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: F + P + P + '\n'
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: F + P + P + F
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789";
            std::string expectedLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: F + P + P + F + '\n'
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789\n";
            std::string expectedLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // F + errorLog
        {
            std::string testLog = LOG_FULL + "456\n" + LOG_ERROR + "789";
            std::string expectedLog = LOG_FULL + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // F + errorLog + '\n'
        {
            std::string testLog = LOG_FULL + "456\n" + LOG_ERROR + "789\n";
            std::string expectedLog = LOG_FULL + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: P + P + F
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P + F + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789\n";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: P + P + error
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_ERROR + "789";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P + error + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_ERROR + "789\n";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456";
            std::string expectedLog = LOG_PART + "123\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(false, line.fullLine);
        }
        // case: P + P + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n";
            std::string expectedLog = LOG_PART + "123\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs, true);

            APSARA_TEST_EQUAL("", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(false, line.fullLine);
        }
    }
}

void LastMatchedContainerdTextLineUnittest::TestLastContainerdTextLineMerge() {
    {
        MultilineOptions multilineOpts;
        LogFileReader logFileReader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        BaseLineParse* baseLineParsePtr = nullptr;
        baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
        logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
        // 异常情况+有回车
        {
            // case: PartLogFlag存在，第三个空格存在但空格后无内容
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P \n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag存在，第三个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag不存在，第二个空格存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout \n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第二个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL(testLog.substr(0, size - 1), line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第一个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00stdout\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL(testLog.substr(0, size - 1), line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
        }
        // 异常情况+无回车
        {
            // case: PartLogFlag存在，第三个空格存在但空格后无内容
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P ";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag存在，第三个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout P";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // case: PartLogFlag不存在，第二个空格存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout ";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第二个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00 stdout";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL(testLog, line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // case: 第一个空格不存在
            {
                std::string testLog = "2024-01-05T23:28:06.818486411+08:00stdout";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);

                APSARA_TEST_EQUAL(testLog, line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
        }
        // case: F + P + P
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: F + P + P + '\n'
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: F + P + P + F
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: F + P + P + F + '\n'
        {
            std::string testLog = LOG_FULL + "789\n" + LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789\n";
            std::string expectedLog = LOG_FULL + "789\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // F + errorLog
        {
            std::string testLog = LOG_FULL + "456\n" + LOG_ERROR + "789";
            std::string expectedLog = LOG_FULL + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // F + errorLog + '\n'
        {
            std::string testLog = LOG_FULL + "456\n" + LOG_ERROR + "789\n";
            std::string expectedLog = LOG_FULL + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL(LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(int(expectedLog.size()), line.lineBegin);
            APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: P + P + F
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P + F + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_FULL + "789\n";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }

        // case: P + P + error
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_ERROR + "789";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456" + LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P + error + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n" + LOG_ERROR + "789\n";
            std::string expectedLog = LOG_PART + "123\n" + LOG_PART + "456\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("123456" + LOG_ERROR + "789", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(true, line.fullLine);
        }
        // case: P + P
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456";
            std::string expectedLog = LOG_PART + "123\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(false, line.fullLine);
        }
        // case: P + P + '\n'
        {
            std::string testLog = LOG_PART + "123\n" + LOG_PART + "456\n";
            std::string expectedLog = LOG_PART + "123\n";

            int32_t size = testLog.size();
            int32_t endPs; // the position of \n or \0
            if (testLog[size - 1] == '\n') {
                endPs = size - 1;
            } else {
                endPs = size;
            }
            LineInfo line = logFileReader.GetLastLine(testLog, endPs);

            APSARA_TEST_EQUAL("", line.data.to_string());
            APSARA_TEST_EQUAL(0, line.lineBegin);
            APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
            APSARA_TEST_EQUAL(false, line.fullLine);
        }
    }
}

class LastMatchedDockerJsonFileUnittest : public ::testing::Test {
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

    void TestLastDockerJsonFile();

    std::unique_ptr<char[]> expectedContent;
    FileReaderOptions readerOpts;
    PipelineContext ctx;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
};

UNIT_TEST_CASE(LastMatchedDockerJsonFileUnittest, TestLastDockerJsonFile);

std::string LastMatchedDockerJsonFileUnittest::logPathDir;
std::string LastMatchedDockerJsonFileUnittest::gbkFile;
std::string LastMatchedDockerJsonFileUnittest::utf8File;

void LastMatchedDockerJsonFileUnittest::TestLastDockerJsonFile() {
    {
        MultilineOptions multilineOpts;
        LogFileReader logFileReader(
            logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
        BaseLineParse* baseLineParsePtr = nullptr;
        baseLineParsePtr = logFileReader.GetParser<DockerJsonFileParser>(0);
        logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
        // 不带回车
        // 不带回车
        {
            // 合法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)",
                                  line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // log非法
            {
                std::string testLog
                    = R"({"log1":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // stream非法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream1":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // time非法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time1":"2024-02-19T03:49:37.793533014Z"})";
                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // 非法json
            {
                std::string testLog
                    = R"(log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
        }
        // 带回车
        {
            // 合法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                testLog += "\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)",
                                  line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(true, line.fullLine);
            }
            // log非法
            {
                std::string testLog
                    = R"({"log1":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                testLog += "\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // stream非法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream1":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                testLog += "\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // time非法
            {
                std::string testLog
                    = R"({"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time1":"2024-02-19T03:49:37.793533014Z"})";
                testLog += "\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
            // 非法json
            {
                std::string testLog
                    = R"(log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
                testLog += "\n";

                int32_t size = testLog.size();
                int32_t endPs; // the position of \n or \0
                if (testLog[size - 1] == '\n') {
                    endPs = size - 1;
                } else {
                    endPs = size;
                }
                LineInfo line = logFileReader.GetLastLine(testLog, endPs);
                APSARA_TEST_EQUAL(1, line.rollbackLineFeedCount);
                APSARA_TEST_EQUAL("", line.data.to_string());
                APSARA_TEST_EQUAL(0, line.lineBegin);
                APSARA_TEST_EQUAL(false, line.fullLine);
            }
        }
    }
}


class LastMatchedContainerdTextWithDockerJsonUnittest : public ::testing::Test {
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

    void TestContainerdTextWithDockerJson();
    void TestDockerJsonWithContainerdText();

    std::unique_ptr<char[]> expectedContent;
    FileReaderOptions readerOpts;
    PipelineContext ctx;
    static std::string logPathDir;
    static std::string gbkFile;
    static std::string utf8File;
};

UNIT_TEST_CASE(LastMatchedContainerdTextWithDockerJsonUnittest, TestContainerdTextWithDockerJson);
UNIT_TEST_CASE(LastMatchedContainerdTextWithDockerJsonUnittest, TestDockerJsonWithContainerdText);

std::string LastMatchedContainerdTextWithDockerJsonUnittest::logPathDir;
std::string LastMatchedContainerdTextWithDockerJsonUnittest::gbkFile;
std::string LastMatchedContainerdTextWithDockerJsonUnittest::utf8File;

void LastMatchedContainerdTextWithDockerJsonUnittest::TestContainerdTextWithDockerJson() {
    MultilineOptions multilineOpts;
    LogFileReader logFileReader(
        logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<DockerJsonFileParser>(0);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    {
        std::string testLog
            = R"({"log":"2021-07-13T16:32:21.212861448Z stdout P Exception in thread  \"main\" java\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
        testLog += "\n";
        testLog
            += R"({"log":"2021-07-13T16:32:21.212861448Z stdout F .lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
        int32_t size = testLog.size();
        int32_t endPs; // the position of \n or \0
        if (testLog[size - 1] == '\n') {
            endPs = size - 1;
        } else {
            endPs = size;
        }
        LineInfo line = logFileReader.GetLastLine(testLog, endPs);
        APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
        APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)", line.data.to_string());
        APSARA_TEST_EQUAL(0, line.lineBegin);
        APSARA_TEST_EQUAL(true, line.fullLine);
    }
    {
        std::string testLog
            = R"({"log":"2021-07-13T16:32:21.212861448Z stdout P Exception in thread  \"main\" java\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
        testLog += "\n";
        testLog
            += R"({"log":"2021-07-13T16:32:21.212861448Z stdout F .lang.NullPoinntterException\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
        testLog += "\n";
        testLog
            += R"({"log":"2021-07-13T16:32:21.212861448Z stdout P 哈哈哈哈\n","stream":"stdout","time":"2024-02-19T03:49:37.79353)";
        testLog += "\n";
        testLog
            += R"({"log":"2021-07-13T16:32:21.212861448Z stdout F 啦啦啦啦\n","stream":"stdout","time":"2024-02-19T03:49:37.79353)";
        int32_t size = testLog.size();
        int32_t endPs; // the position of \n or \0
        if (testLog[size - 1] == '\n') {
            endPs = size - 1;
        } else {
            endPs = size;
        }
        LineInfo line = logFileReader.GetLastLine(testLog, endPs);
        APSARA_TEST_EQUAL(4, line.rollbackLineFeedCount);
        APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)", line.data.to_string());
        APSARA_TEST_EQUAL(0, line.lineBegin);
        APSARA_TEST_EQUAL(true, line.fullLine);
    }
}

void LastMatchedContainerdTextWithDockerJsonUnittest::TestDockerJsonWithContainerdText() {
    MultilineOptions multilineOpts;
    LogFileReader logFileReader(
        logPathDir, utf8File, DevInode(), std::make_pair(&readerOpts, &ctx), std::make_pair(&multilineOpts, &ctx));
    BaseLineParse* baseLineParsePtr = nullptr;
    baseLineParsePtr = logFileReader.GetParser<ContainerdTextParser>(LogFileReader::BUFFER_SIZE);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    baseLineParsePtr = logFileReader.GetParser<DockerJsonFileParser>(0);
    logFileReader.mLineParsers.emplace_back(baseLineParsePtr);
    {
        std::string testLog
            = R"(2021-07-13T16:32:21.212861448Z stdout P {"log":"Exception in thread  \"main\" java.lang.NullPoinntterException\n","stream":"stdout","t)";
        testLog += "\n";
        testLog += R"(2021-07-13T16:32:21.212861448Z stdout F ime":"2024-02-19T03:49:37.793533014Z"})";
        int32_t size = testLog.size();
        int32_t endPs; // the position of \n or \0
        if (testLog[size - 1] == '\n') {
            endPs = size - 1;
        } else {
            endPs = size;
        }
        LineInfo line = logFileReader.GetLastLine(testLog, endPs);
        APSARA_TEST_EQUAL(2, line.rollbackLineFeedCount);
        APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)", line.data.to_string());
        APSARA_TEST_EQUAL(0, line.lineBegin);
        APSARA_TEST_EQUAL(true, line.fullLine);
    }
    {
        std::string testLog
            = R"(2021-07-13T16:32:21.212861448Z stdout P {"log":"Exception in thread  \"main\" java.lang.NullPoinntterEx)";
        testLog += "\n";
        testLog
            += R"(2021-07-13T16:32:21.212861448Z stdout F ception\n","stream":"stdout","time":"2024-02-19T03:49:37.793533014Z"})";
        testLog += "\n";
        testLog += R"(2021-07-13T16:32:21.212861448Z stdo)";
        int32_t size = testLog.size();
        int32_t endPs; // the position of \n or \0
        if (testLog[size - 1] == '\n') {
            endPs = size - 1;
        } else {
            endPs = size;
        }
        LineInfo line = logFileReader.GetLastLine(testLog, endPs);
        APSARA_TEST_EQUAL(3, line.rollbackLineFeedCount);
        APSARA_TEST_EQUAL(R"(Exception in thread  "main" java.lang.NullPoinntterException)", line.data.to_string());
        APSARA_TEST_EQUAL(0, line.lineBegin);
        APSARA_TEST_EQUAL(true, line.fullLine);
    }
}
} // namespace logtail

UNIT_TEST_MAIN
