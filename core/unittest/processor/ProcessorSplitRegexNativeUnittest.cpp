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

#include "common/Constants.h"
#include "common/JsonUtil.h"
#include "config/Config.h"
#include "models/LogEvent.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "unittest/Unittest.h"

namespace logtail {

const std::string LOG_BEGIN_STRING = "Exception in thread 'main' java.lang.NullPointerException";
const std::string LOG_BEGIN_REGEX = R"(Exception.*)";
const std::string LOG_CONTINUE_STRING = "    at com.example.myproject.Book.getTitle(Book.java:16)";
const std::string LOG_CONTINUE_REGEX = R"(\s+at\s.*)";
const std::string LOG_END_STRING = "    ...23 more";
const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more)";
const std::string LOG_UNMATCH = "unmatch log";

class ProcessorSplitRegexNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcessEventSingleLine();
    void TestProcessEventMultiline();
    void TestProcessEventMultilineKeepUnmatch();
    void TestProcessEventMultilineDiscardUnmatch();
    void TestProcessEventMultilineAllNotMatchKeepUnmatch();
    void TestProcessEventMultilineAllNotMatchDiscardUnmatch();
    void TestProcess();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventSingleLine);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventMultiline);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventMultilineKeepUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventMultilineDiscardUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventMultilineAllNotMatchKeepUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventMultilineAllNotMatchDiscardUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcess);

void ProcessorSplitRegexNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["StartPattern"] = ".*";
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventSingleLine() {
    // make config
    Json::Value config;
    config["StartPattern"] = ".*";
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "content" : "line1\nline2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::string expectJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "line1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventMultiline() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : ")"
           << LOG_BEGIN_STRING << R"(first.\nmultiline1\nmultiline2\n)" << LOG_BEGIN_STRING
           << R"(second.\nmultiline1\nmultiline2)"
           << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : ")"
               << LOG_BEGIN_STRING << R"(first.\nmultiline1\nmultiline2)"
               << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : ")"
               << LOG_BEGIN_STRING << R"(second.\nmultiline1\nmultiline2)"
               << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventMultilineKeepUnmatch() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : ")"
           << R"(first.\nmultiline1\nmultiline2\n)" << LOG_BEGIN_STRING << R"(second.\nmultiline1\nmultiline2)"
           << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "first."
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "multiline1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "multiline2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : ")"
               << LOG_BEGIN_STRING << R"(second.\nmultiline1\nmultiline2)"
               << R"("
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventMultilineDiscardUnmatch() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : ")"
           << R"(first.\nmultiline1\nmultiline2\n)" << LOG_BEGIN_STRING << R"(second.\nmultiline1\nmultiline2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : ")"
               << LOG_BEGIN_STRING << R"(second.\nmultiline1\nmultiline2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventMultilineAllNotMatchKeepUnmatch() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "first.\nmultiline1\nsecond.\nmultiline1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "first."
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "multiline1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "second."
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "multiline1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventMultilineAllNotMatchDiscardUnmatch() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "first.\nmultiline1\nsecond.\nmultiline1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
    eventGroup.SwapEvents(newEvents);
    // judge result
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
}

void ProcessorSplitRegexNativeUnittest::TestProcess() {
    // make config
    Json::Value config;
    config["StartPattern"] = "line.*";
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = true;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "content" : "line1\ncontinue\nline2\ncontinue"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    std::string logPath("/var/log/message");
    // run test function
    processor.Process(eventGroup);
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "content" : "line1\ncontinue"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": ")"
               << strlen(R"(line1ncontinuen)") << R"(",
                    "content" : "line2\ncontinue"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    // check observability
    APSARA_TEST_EQUAL_FATAL(4, processor.GetContext().GetProcessProfile().feedLines);
    APSARA_TEST_EQUAL_FATAL(2, processor.GetContext().GetProcessProfile().splitLines);
}

class ProcessorSplitRegexDisacardUnmatchUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitRegexDisacardUnmatchUnittest, TestLogSplitWithBeginContinue);
UNIT_TEST_CASE(ProcessorSplitRegexDisacardUnmatchUnittest, TestLogSplitWithBeginEnd);
UNIT_TEST_CASE(ProcessorSplitRegexDisacardUnmatchUnittest, TestLogSplitWithBegin);
UNIT_TEST_CASE(ProcessorSplitRegexDisacardUnmatchUnittest, TestLogSplitWithContinueEnd);
UNIT_TEST_CASE(ProcessorSplitRegexDisacardUnmatchUnittest, TestLogSplitWithEnd);

void ProcessorSplitRegexDisacardUnmatchUnittest::TestLogSplitWithBeginContinue() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log (only begin)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexDisacardUnmatchUnittest::TestLogSplitWithBeginEnd() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_END_STRING
               << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: incomplete log (begin)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexDisacardUnmatchUnittest::TestLogSplitWithBegin() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexDisacardUnmatchUnittest::TestLogSplitWithContinueEnd() {
    // make config
    Json::Value config;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log (only end)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexDisacardUnmatchUnittest::TestLogSplitWithEnd() {
    // make config
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL("null", CompactJson(outJson).c_str());
    }
}

class ProcessorSplitRegexKeepUnmatchUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitRegexKeepUnmatchUnittest, TestLogSplitWithBeginContinue);
UNIT_TEST_CASE(ProcessorSplitRegexKeepUnmatchUnittest, TestLogSplitWithBeginEnd);
UNIT_TEST_CASE(ProcessorSplitRegexKeepUnmatchUnittest, TestLogSplitWithBegin);
UNIT_TEST_CASE(ProcessorSplitRegexKeepUnmatchUnittest, TestLogSplitWithContinueEnd);
UNIT_TEST_CASE(ProcessorSplitRegexKeepUnmatchUnittest, TestLogSplitWithEnd);

void ProcessorSplitRegexKeepUnmatchUnittest::TestLogSplitWithBeginContinue() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log (only begin)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexKeepUnmatchUnittest::TestLogSplitWithBeginEnd() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_END_STRING
               << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: incomplete log (begin)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexKeepUnmatchUnittest::TestLogSplitWithBegin() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexKeepUnmatchUnittest::TestLogSplitWithContinueEnd() {
    // make config
    Json::Value config;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: complete log (only end)
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorSplitRegexKeepUnmatchUnittest::TestLogSplitWithEnd() {
    // make config
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "split";
    config["AppendingLogPositionMeta"] = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    { // case: complete log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    { // case: no match log
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());
        std::string logPath("/var/log/message");
        EventsContainer newEvents;
        // run test function
        processor.ProcessEvent(eventGroup, logPath, std::move(eventGroup.MutableEvents()[0]), newEvents);
        eventGroup.SwapEvents(newEvents);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ_FATAL(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

} // namespace logtail

UNIT_TEST_MAIN