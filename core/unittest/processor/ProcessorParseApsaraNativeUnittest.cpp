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

#include <cstdlib>

#include "common/JsonUtil.h"
#include "config/Config.h"
#include "models/LogEvent.h"
#include "models/StringView.h"
#include "plugin/instance/ProcessorInstance.h"
#include "processor/inner/ProcessorMergeMultilineLogNative.h"
#include "processor/ProcessorParseApsaraNative.h"
#include "processor/inner/ProcessorSplitLogStringNative.h"
#include "processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorParseApsaraNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        BOOL_FLAG(ilogtail_discard_old_data) = false;
    }

    void TestInit();
    void TestProcessWholeLine();
    void TestProcessWholeLinePart();
    void TestProcessKeyOverwritten();
    void TestUploadRawLog();
    void TestAddLog();
    void TestProcessEventKeepUnmatch();
    void TestProcessEventDiscardUnmatch();
    void TestMultipleLines();
    void TestProcessEventMicrosecondUnmatch();
    void TestApsaraEasyReadLogTimeParser();
    void TestApsaraLogLineParser();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLine);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessWholeLinePart);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessKeyOverwritten);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestUploadRawLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestAddLog);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventDiscardUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestMultipleLines);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestProcessEventMicrosecondUnmatch);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestApsaraEasyReadLogTimeParser);
UNIT_TEST_CASE(ProcessorParseApsaraNativeUnittest, TestApsaraLogLineParser);

void ProcessorParseApsaraNativeUnittest::TestApsaraEasyReadLogTimeParser() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "GMT+08:00";
    ProcessorParseApsaraNative* processor = new ProcessorParseApsaraNative;
    processor->SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    StringView buffer = "[1378972170425093]\tA:B";
    StringView lastStr;
    LogtailTime lastTime = {0, 0};
    int64_t microTime = 0;
    uint32_t dateTime = 0;

    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972170);
    APSARA_TEST_EQUAL(microTime, 1378972170425093);
    APSARA_TEST_EQUAL(lastTime.tv_sec, 0);

    buffer = "[1378972171093]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972171);
    APSARA_TEST_EQUAL(microTime, 1378972171093000);
    APSARA_TEST_EQUAL(lastTime.tv_sec, 0);

    buffer = "[1378972172]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378972172);
    APSARA_TEST_EQUAL(microTime, 1378972172000000);
    APSARA_TEST_EQUAL(lastTime.tv_sec, 0);

    buffer = "[2013-09-12 22:18:28.819129]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995508);
    APSARA_TEST_EQUAL(microTime, 1378995508819129);
    APSARA_TEST_EQUAL(dateTime, lastTime.tv_sec);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:28");

    buffer = "[2013-09-12 22:18:28.819139]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995508);
    APSARA_TEST_EQUAL(microTime, 1378995508819139);
    APSARA_TEST_EQUAL(dateTime, lastTime.tv_sec);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:28");

    buffer = "[2013-09-12 22:18:29.819139]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995509);
    APSARA_TEST_EQUAL(microTime, 1378995509819139);
    APSARA_TEST_EQUAL(dateTime, lastTime.tv_sec);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:29");
    LOG_INFO(sLogger, ("TestApsaraEasyReadLogTimeParser() end", time(NULL)));

    buffer = "[2013-09-12 22:18:29.819]\tA:B";
    microTime = 0;
    dateTime = 0;
    dateTime = processor->ApsaraEasyReadLogTimeParser(buffer, lastStr, lastTime, microTime);
    APSARA_TEST_EQUAL(dateTime, 1378995509);
    APSARA_TEST_EQUAL(microTime, 1378995509819000);
    APSARA_TEST_EQUAL(dateTime, lastTime.tv_sec);
    APSARA_TEST_EQUAL(lastStr, "2013-09-12 22:18:29");
}

void ProcessorParseApsaraNativeUnittest::TestApsaraLogLineParser() {
    const char* logLine[] = {
        "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]", // 1
        "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]\t", // 2
        "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1754]\tsomestring", // 3
        "[2013-03-13 "
        "18:05:09.493309]\t[WARNING]\t[13000]\t[build/debug64/ilogtail/core/ilogtail.cpp:1755]\tRealRecycle#Command:rm "
        "-rf /apsara/tubo/.fuxi_tubo_trash/*", // 4
        "[2013-03-13 "
        "18:14:57.365716]\t[ERROR]\t[12835]\t[build/debug64/ilogtail/core/"
        "ilogtail.cpp:1945]\tParseWhiteListOK:{\n\\\"sys/"
        "pangu/ChunkServerRole\\\": \\\"\\\",\n\\\"sys/pangu/PanguMasterRole\\\": \\\"\\\"}", // 5
        "[2013-03-13 18:14:57.365716]\t[12835]\t[ERROR]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]", // 6
        "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]", // 7
        "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[ERROR]", // 8
        "[2013-03-13 18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]\t[5432187]", // 9
        "[2013-03-13 "
        "18:14:57.365716]\t[build/debug64/ilogtail/core/ilogtail.cpp:1945]\t[12835]\t[ERROR]\t[5432187]\tcount:55", // 10
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]", // 11
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t", // 12
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\n", // 13
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tother\tcount:45", // 14
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tother:\tcount:45", // 15
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tcount:45", // 16
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\tcount:45\tnum:88\tjob:ss", // 17
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corrupt\tcount:45\tnum:88\tjob:ss", // 18
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corruptcount:45\tnum:88\tjob:ss", // 19
        "[2013-03-13 18:14:57.365716]\t[trace_id:787]\t[ERROR]\t[corrupt]count:45\tnum:88\tjob:ss", // 20
        "[2013-03-13 18:14:57.365716]\t[build/debug64]\t[ERROR]\tcount:45\tnum:88\tjob:ss", // 21
        "[2013-03-13 18:14:57.365716]\t[build/debug64:]\t[ERROR]\tcount:45\tnum:88\tjob:ss", // 22
        "[2013-03-13 18:14:57.365716]\t[build/debug64:]\t[ERROR]\tcount:45\t:88\tjob:ss", // 23
        "[2013-03-13 18:14:57.365716]", // 24
        "[2013-03-13 18:14:57.365716]\t", // 25
        "[2013-03-13 18:14:57.365716]\n", // 26
        "[2013-03-13 18:14:57.365716]\t\t\t", // 27
        "", // 28
        "[2013-03-13 "
        "18:05:09.493309]\t[WARNING]\t[13000]\t[13003]\t[ERROR]\t[build/debug64/ilogtail/core/ilogtail.cpp:1753]", // 29
        "[2013-03-13 18:05:09.493309]\t[WARNING]\t[13000]\t[13003]\t[ERROR]\t[tubo.cpp:1753]", // 30
        "[2013-03-13 18:05:09.493309" // 31
    };
    static const char* APSARA_FIELD_LEVEL = "__LEVEL__";
    static const char* APSARA_FIELD_THREAD = "__THREAD__";
    static const char* APSARA_FIELD_FILE = "__FILE__";
    static const char* APSARA_FIELD_LINE = "__LINE__";
    const char* logParseResult[][16] = {
        {"microtime",
         "1363169109493309",
         APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1753",
         NULL}, // 1
        {"microtime",
         "1363169109493309",
         APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1753",
         NULL}, // 2
        {"microtime",
         "1363169109493309",
         APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1754",
         NULL}, // 3
        {APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1755",
         "RealRecycle#Command",
         "rm -rf /apsara/tubo/.fuxi_tubo_trash/*",
         NULL}, // 4
        {APSARA_FIELD_LEVEL,
         "ERROR",
         APSARA_FIELD_THREAD,
         "12835",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         "ParseWhiteListOK",
         "{\n\"sys/pangu/ChunkServerRole\": \"\",\n\"sys/pangu/PanguMasterRole\": \"\"}",
         NULL}, // 5
        {APSARA_FIELD_THREAD,
         "12835",
         APSARA_FIELD_LEVEL,
         "ERROR",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         NULL}, // 6
        {APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         APSARA_FIELD_THREAD,
         "12835",
         APSARA_FIELD_LEVEL,
         "ERROR",
         NULL}, // 7
        {APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         APSARA_FIELD_LEVEL,
         "ERROR",
         NULL}, // 8
        {APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         APSARA_FIELD_THREAD,
         "12835",
         APSARA_FIELD_LEVEL,
         "ERROR",
         NULL}, // 9
        {APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1945",
         APSARA_FIELD_THREAD,
         "12835",
         APSARA_FIELD_LEVEL,
         "ERROR",
         "count",
         "55",
         NULL}, // 10
        {APSARA_FIELD_LEVEL, "ERROR", NULL}, // 11
        {APSARA_FIELD_LEVEL, "ERROR", NULL}, // 12
        {APSARA_FIELD_LEVEL, "ERROR", NULL}, // 13
        {APSARA_FIELD_LEVEL, "ERROR", "count", "45", NULL}, // 14
        {APSARA_FIELD_LEVEL, "ERROR", "other", "", "count", "45", NULL}, // 15
        {APSARA_FIELD_LEVEL, "ERROR", "count", "45", NULL}, // 16
        {APSARA_FIELD_LEVEL, "ERROR", "count", "45", "num", "88", "job", "ss", NULL}, // 17
        {APSARA_FIELD_LEVEL, "ERROR", "count", "45", "num", "88", "job", "ss", NULL}, // 18
        {APSARA_FIELD_LEVEL, "ERROR", "[corruptcount", "45", "num", "88", "job", "ss", NULL}, // 19
        {APSARA_FIELD_LEVEL, "ERROR", "[corrupt]count", "45", "num", "88", "job", "ss", NULL}, // 20
        {APSARA_FIELD_FILE,
         "build/debug64",
         APSARA_FIELD_LEVEL,
         "ERROR",
         "count",
         "45",
         "num",
         "88",
         "job",
         "ss",
         NULL}, // 21
        {APSARA_FIELD_FILE,
         "build/debug64",
         APSARA_FIELD_LINE,
         "",
         APSARA_FIELD_LEVEL,
         "ERROR",
         "count",
         "45",
         "num",
         "88",
         "job",
         "ss",
         NULL}, // 22
        {APSARA_FIELD_FILE,
         "build/debug64",
         APSARA_FIELD_LINE,
         "",
         APSARA_FIELD_LEVEL,
         "ERROR",
         "count",
         "45",
         "",
         "88",
         "job",
         "ss",
         NULL}, // 23
        {"microtime", "1363169697365716", NULL}, // 24
        {"microtime", "1363169697365716", NULL}, // 25
        {"microtime", "1363169697365716", NULL}, // 26
        {"microtime", "1363169697365716", NULL}, // 27
        {"content", "", NULL}, // 28
        {APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "build/debug64/ilogtail/core/ilogtail.cpp",
         APSARA_FIELD_LINE,
         "1753",
         NULL}, // 29
        {APSARA_FIELD_LEVEL,
         "WARNING",
         APSARA_FIELD_THREAD,
         "13000",
         APSARA_FIELD_FILE,
         "tubo.cpp",
         APSARA_FIELD_LINE,
         "1753",
         NULL}, // 30
        {NULL} // 31
    };


    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "GMT+08:00";
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(new ProcessorParseApsaraNative, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    for (uint32_t i = 0; i < sizeof(logLine) / sizeof(logLine[0]); i++) {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
            + std::string(logLine[i]) +
            R"("
                    },
                    "timestamp" : 12345678901,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        std::vector<PipelineEventGroup> eventGroupList;
        eventGroupList.emplace_back(std::move(eventGroup));
        processorInstance.Process(eventGroupList);

        Json::Value outJson = eventGroupList[0].ToJson();
        if (logParseResult[i][0] == NULL) {
            APSARA_TEST_EQUAL(eventGroupList[0].ToJsonString(), "null");
            continue;
        }
        for (int j = 0; j < 10 && logParseResult[i][j] != NULL; j++) {
            if (j % 2 == 0) {
                APSARA_TEST_TRUE(outJson.isMember("events"));
                APSARA_TEST_TRUE(outJson["events"].isArray());
                APSARA_TEST_TRUE(outJson["events"][0].isObject());
                APSARA_TEST_TRUE(outJson["events"][0].isMember("contents"));
                APSARA_TEST_TRUE(outJson["events"][0]["contents"].isMember(logParseResult[i][j]));
                APSARA_TEST_EQUAL(outJson["events"][0]["contents"][logParseResult[i][j]],
                                  std::string(logParseResult[i][j + 1]));
            } else {
                continue;
            }
        }
    }
}

void ProcessorParseApsaraNativeUnittest::TestMultipleLines() {
    // 第一个contents 测试多行下的解析，第二个contents测试多行下time的解析
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:33.2]\t[INFO]\t[2]\t/ilogtail/AppConfigBase.cpp:2\t\tAppConfigBase AppConfigBase:2
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15
:50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1
[2023-09-04 13:15:22.3]\t[WARNING]\t[3]\t/ilogtail/AppConfigBase.cpp:3\t\tAppConfigBase AppConfigBase:3"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";


    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "1",
                    "AppConfigBase AppConfigBase": "1",
                    "__LEVEL__": "ERROR",
                    "__THREAD__": "1",
                    "microtime": "1693833350100000"
                },
                "timestamp": 1693833350,
                "timestampNanosecond": 100000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "2",
                    "AppConfigBase AppConfigBase": "2",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "2",
                    "microtime": "1693833333200000"
                },
                "timestamp": 1693833333,
                "timestampNanosecond": 200000000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": "[2023-09-04 13:15"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "__raw__": ":50.1]\t[ERROR]\t[1]\t/ilogtail/AppConfigBase.cpp:1\t\tAppConfigBase AppConfigBase:1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "3",
                    "AppConfigBase AppConfigBase": "3",
                    "__LEVEL__": "WARNING",
                    "__THREAD__": "3",
                    "microtime": "1693833322300000"
                },
                "timestamp": 1693833322,
                "timestampNanosecond": 300000000,
                "type": 1
            }
        ]
    })";

    // ProcessorSplitLogStringNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Timezone"] = "GMT+00:00";
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["CopingRawLog"] = false;
        config["RenamedSourceKey"] = "__raw__";
        config["AppendingLogPositionMeta"] = false;

        std::string pluginId = "testID";
        // run function ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processorSplitLogStringNative;
        processorSplitLogStringNative.SetContext(mContext);
        APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
        processorSplitLogStringNative.Process(eventGroup);

        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);

        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
    // ProcessorSplitMultilineLogStringNative
    {
        // make events
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.FromJsonString(inJson);

        // make config
        Json::Value config;
        config["SourceKey"] = "content";
        config["Timezone"] = "GMT+00:00";
        config["KeepingSourceWhenParseFail"] = true;
        config["KeepingSourceWhenParseSucceed"] = false;
        config["CopingRawLog"] = false;
        config["RenamedSourceKey"] = "__raw__";
        config["StartPattern"] = "[a-zA-Z0-9]*";
        config["UnmatchedContentTreatment"] = "single_line";
        config["AppendingLogPositionMeta"] = false;

        std::string pluginId = "testID";

        // run function ProcessorSplitMultilineLogStringNative
        ProcessorSplitMultilineLogStringNative processorSplitMultilineLogStringNative;
        processorSplitMultilineLogStringNative.SetContext(mContext);
        processorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processorSplitMultilineLogStringNative.Init(config));
        processorSplitMultilineLogStringNative.Process(eventGroup);

        // run function ProcessorParseApsaraNative
        ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
        ProcessorInstance processorInstance(&processor, pluginId);
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        processor.Process(eventGroup);

        // judge result
        std::string outJson = eventGroup.ToJsonString();

        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseApsaraNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";

    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLine() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1693833364862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833364862181"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 862181000,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestProcessWholeLinePart() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:0]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:0[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[1234560	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833300000000"
                },
                "timestamp": 1693833300,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__THREAD__": "385658",
                    "microtime": "1693833360000000"
                },
                "timestamp": 1693833360,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    APSARA_TEST_EQUAL_FATAL(1, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(3), processorInstance.mProcInRecordsTotal->GetValue());
    // only timestamp failed, so output is 2
    APSARA_TEST_EQUAL_FATAL(uint64_t(2), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(1), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(1), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessKeyOverwritten() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	content:100		rawLog:success		__raw_log__:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "__raw_log__": "success",
                    "content": "100",
                    "microtime": "1693833304862181",
                    "rawLog": "success"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents" :
                {
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestUploadRawLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = true;
    config["CopingRawLog"] = true;
    config["RenamedSourceKey"] = "rawLog";
    config["Timezone"] = "";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04.862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833304862181",
                    "rawLog" : "[2023-09-04 13:15:04.862181]\t[INFO]\t[385658]\t/ilogtail/AppConfigBase.cpp:100\t\tAppConfigBase AppConfigBase:success"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents" :
                {
                    "__raw_log__": "value1",
                    "rawLog": "value1"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseApsaraNativeUnittest::TestAddLog() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";

    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto eventGroup = PipelineEventGroup(std::make_shared<SourceBuffer>());
    auto logEvent = eventGroup.CreateLogEvent();
    char key[] = "key";
    char value[] = "value";
    processor.AddLog(key, value, *logEvent);
    // check observability
    APSARA_TEST_EQUAL_FATAL(int(strlen(key) + strlen(value) + 5),
                            processor.GetContext().GetProcessProfile().logGroupSize);
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "rawLog" : "value1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond": 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcOutRecordsTotal->GetValue());
    expectValue = "rawLogvalue1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseOutSizeBytes->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcDiscardRecordsTotal->GetValue());

    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = false;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "value1"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    // check observablity
    int count = 5;
    APSARA_TEST_EQUAL_FATAL(count, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processorInstance.mProcInRecordsTotal->GetValue());
    std::string expectValue = "value1";
    APSARA_TEST_EQUAL_FATAL((expectValue.length()) * count, processor.mProcParseInSizeBytes->GetValue());
    // discard unmatch, so output is 0
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcParseOutSizeBytes->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(count), processor.mProcParseErrorTotal->GetValue());
}

void ProcessorParseApsaraNativeUnittest::TestProcessEventMicrosecondUnmatch() {
    // make config
    Json::Value config;
    config["SourceKey"] = "content";
    config["KeepingSourceWhenParseFail"] = true;
    config["KeepingSourceWhenParseSucceed"] = false;
    config["CopingRawLog"] = false;
    config["RenamedSourceKey"] = "rawLog";
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:15:04,862181]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:16:04]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:17:04,1]	[INFO]	[385658]	/ilogtail/AppConfigBase.cpp:100		AppConfigBase AppConfigBase:success"
                },
                "timestamp" : 12345678901,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "[2023-09-04 13:18:04"
                },
                "timestamp" : 12345678901,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorParseApsaraNative& processor = *(new ProcessorParseApsaraNative);
    std::string pluginId = "testID";
    ProcessorInstance processorInstance(&processor, pluginId);
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // judge result
    std::string expectJson = R"({
        "events": [
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833304862181"
                },
                "timestamp": 1693833304,
                "timestampNanosecond": 862181000,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833364000000"
                },
                "timestamp": 1693833364,
                "timestampNanosecond": 0,
                "type": 1
            },
            {
                "contents": {
                    "/ilogtail/AppConfigBase.cpp": "100",
                    "AppConfigBase AppConfigBase": "success",
                    "__LEVEL__": "INFO",
                    "__THREAD__": "385658",
                    "microtime": "1693833424100000"
                },
                "timestamp": 1693833424,
                "timestampNanosecond": 100000000,
                "type": 1
            },
            {
                "contents": {
                    "rawLog": "[2023-09-04 13:18:04"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observablity
    APSARA_TEST_EQUAL_FATAL(1, processor.GetContext().GetProcessProfile().parseFailures);
    APSARA_TEST_EQUAL_FATAL(uint64_t(4), processorInstance.mProcInRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(4), processorInstance.mProcOutRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(0), processor.mProcDiscardRecordsTotal->GetValue());
    APSARA_TEST_EQUAL_FATAL(uint64_t(1), processor.mProcParseErrorTotal->GetValue());
}

} // namespace logtail

UNIT_TEST_MAIN