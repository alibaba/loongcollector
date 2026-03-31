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
#include "config/CollectionConfig.h"
#include "constants/Constants.h"
#include "models/LogEvent.h"
#include "plugin/processor/inner/ProcessorNoSplitLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorNoSplitLogStringNativeUnittest : public ::testing::Test {
public:
    void TestSingleLine();
    void TestMultiline();
    void TestMultipleInputEvents();
    void TestEmptyEvents();
    void TestEnableRawEvent();
    void TestMetrics();

protected:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

private:
    CollectionPipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestSingleLine)
UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestMultiline)
UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestMultipleInputEvents)
UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestEmptyEvents)
UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestEnableRawEvent)
UNIT_TEST_CASE(ProcessorNoSplitLogStringNativeUnittest, TestMetrics)

void ProcessorNoSplitLogStringNativeUnittest::TestSingleLine() {
    Json::Value config;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case: single line
    // input: 1 event, 1 line
    // output: 1 event, content unchanged
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "single line log"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);

        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "single line log"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorNoSplitLogStringNativeUnittest::TestMultiline() {
    Json::Value config;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case: multiline content should NOT be split, entire content is one log entry
    // input: 1 event, 3 lines
    // output: 1 event, all lines aggregated (content unchanged, \n preserved)
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "line1\nline2\nline3"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "line1\nline2\nline3"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // case: Java stack trace style multiline, all aggregated as one event
    // input: 1 event, 5 lines
    // output: 1 event, content unchanged
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::string stackTrace = "Exception in thread 'main' java.lang.NullPointerException\\n"
                                 "    at com.example.myproject.Book.getTitle(Book.java:16)\\n"
                                 "    at com.example.myproject.Author.getBookTitles(Author.java:25)\\n"
                                 "    at com.example.myproject.Bootstrap.main(Bootstrap.java:14)\\n"
                                 "    ...23 more";
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
               << stackTrace << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << stackTrace << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorNoSplitLogStringNativeUnittest::TestMultipleInputEvents() {
    Json::Value config;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case: multiple input events, each treated independently as one log entry
    // input: 2 events, each with multiline content
    // output: 2 events, each content unchanged
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "first\nlog\nentry"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "second\nlog\nentry"
                    },
                    "timestamp" : 12345678902,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
        APSARA_TEST_EQUAL(2U, eventGroup.GetEvents().size());

        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "first\nlog\nentry"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "second\nlog\nentry"
                    },
                    "timestamp" : 12345678902,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorNoSplitLogStringNativeUnittest::TestEmptyEvents() {
    Json::Value config;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case: empty event group
    // input: 0 events
    // output: 0 events
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" : []
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);

        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
}

void ProcessorNoSplitLogStringNativeUnittest::TestEnableRawEvent() {
    Json::Value config;
    config["EnableRawContent"] = true;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case: multiline with EnableRawContent
    // input: 1 event, 3 lines
    // output: 1 raw event (type=4), content unchanged
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "line1\nline2\nline3"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
        APSARA_TEST_EQUAL(1U, eventGroup.GetEvents().size());

        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "content" : "line1\nline2\nline3",
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 4
                }
            ]
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorNoSplitLogStringNativeUnittest::TestMetrics() {
    Json::Value config;

    ProcessorNoSplitLogStringNative processor;
    processor.SetContext(mContext);
    processor.CreateMetricsRecordRef(ProcessorNoSplitLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.CommitMetricsRecordRef();

    // case 1: 1 event, 3 lines
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "line1\nline2\nline3"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
    }

    // case 2: 1 event, 1 line
    {
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJson;
        inJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "single line"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        processor.Process(eventGroup);
    }

    // 2 matched events total (1 from case 1 + 1 from case 2)
    APSARA_TEST_EQUAL(2, processor.mMatchedEventsTotal->GetValue());
    // 4 matched lines total (3 from case 1 + 1 from case 2)
    APSARA_TEST_EQUAL(4, processor.mMatchedLinesTotal->GetValue());
}

} // namespace logtail

UNIT_TEST_MAIN
