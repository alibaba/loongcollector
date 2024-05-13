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
#include "processor/inner/ProcessorSplitLogStringNative.h"
#include "processor/inner/ProcessorSplitMultilineLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

const std::string LOG_BEGIN_STRING = "Exception in thread 'main' java.lang.NullPointerException";
const std::string LOG_BEGIN_REGEX = R"(Exception.*)";
const std::string LOG_CONTINUE_STRING = "    at com.example.myproject.Book.getTitle(Book.java:16)";
const std::string LOG_CONTINUE_REGEX = R"(\s+at\s.*)";
const std::string LOG_END_STRING = "    ...23 more";
const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more)";
const std::string LOG_UNMATCH = "unmatch log";


class ProcessorSplitMultilineLogDisacardUnmatchUnittest : public ::testing::Test {
public:
    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();

protected:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

private:
    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitMultilineLogDisacardUnmatchUnittest, TestLogSplitWithBeginContinue)
UNIT_TEST_CASE(ProcessorSplitMultilineLogDisacardUnmatchUnittest, TestLogSplitWithBeginEnd)
UNIT_TEST_CASE(ProcessorSplitMultilineLogDisacardUnmatchUnittest, TestLogSplitWithBegin)
UNIT_TEST_CASE(ProcessorSplitMultilineLogDisacardUnmatchUnittest, TestLogSplitWithContinueEnd)
UNIT_TEST_CASE(ProcessorSplitMultilineLogDisacardUnmatchUnittest, TestLogSplitWithEnd)

void ProcessorSplitMultilineLogDisacardUnmatchUnittest::TestLogSplitWithBeginContinue() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["SplitType"] = "regex";
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["UnmatchedContentTreatment"] = "discard";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + unmatch
    // input: 1 event, 2 lines
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: start + unmatch
    // input: 1 event, 2 lines
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + start + continue + continue + unmatch
    // input: 1 event, 5 lines
    // output: 1 event, 3 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + start + start
    // input: 1 event, 3 lines
    // output: 2 events, 2 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + start + continue + continue
    // input: 1 event, 4 lines
    // output: 1 event, 3 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: continue
    // input: 1 event, 1 line
    // output: 0 event, 0 line
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 1 + 2 + 1 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 3 + 2 + 3 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(2 + 1 + 2 + 1 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogDisacardUnmatchUnittest::TestLogSplitWithBeginEnd() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["SplitType"] = "regex";
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + unmatch
    // input: 1 event, 2 lines
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: unmatch+start+unmatch
    // input: 1 event, 3 lines
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: unmatch+start+End+unmatch
    // input: 1 event, 4 lines
    // output: 1 event, 2 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"(\n)" << LOG_UNMATCH
               << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: start+start
    // input: 1 event, 2 lines
    // output: 0 event, 0 line
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: unmatch+start+End
    // input: 1 event, 3 lines
    // output: 1 event, 2 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch+start+unmatch+End+unmatch
    // input: 1 event, 4 lines
    // output: 1 event, 3 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 1 + 0 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 2 + 0 + 2 + 3,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(2 + 3 + 2 + 2 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogDisacardUnmatchUnittest::TestLogSplitWithBegin() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["SplitType"] = "regex";
    config["UnmatchedContentTreatment"] = "discard";


    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + start
    // input: 1 event, 2 lines
    // output: 1 event, 1 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch
    // input: 1 event, 1 line
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: start + start
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: start + unmatch
    // input: 1 event, 2 lines
    // output: 1 event, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
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
    // case: unmatch + start + unmatch + unmatch
    // input: 1 event, 4 lines
    // output: 1 event, 3 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH
               << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n)" << LOG_UNMATCH << R"("
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

    // metric
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2 + 2 + 3, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 1 + 0 + 0 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogDisacardUnmatchUnittest::TestLogSplitWithContinueEnd() {
    // make config
    Json::Value config;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["SplitType"] = "regex";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch
    // input: 1 event, 1 line
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: Continue + unmatch
    // input: 1 event, 2 lines
    // output: 0 event, 0 line
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: Continue + Continue + end
    // input: 1 event, 3 lines
    // output: 1 event, 3 lines
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: continue
    // input: 1 event, 1 line
    // output: 0 event, 0 line
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: end
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 1 + 0 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 3 + 0 + 1, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 2 + 0 + 1 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogDisacardUnmatchUnittest::TestLogSplitWithEnd() {
    // make config
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "discard";
    config["SplitType"] = "regex";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: end
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch
    // input: 1 event, 1 line
    // output: 0 event, 0 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ("null", CompactJson(outJson).c_str());
    }
    // case: unmatch + end + unmatch
    // input: 1 event, 3 lines
    // output: 1 event, 2 lines
    {
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
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 1, ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 1, ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

class ProcessorSplitMultilineLogKeepUnmatchUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }
    void TestLogSplitWithBeginContinue();
    void TestLogSplitWithBeginEnd();
    void TestLogSplitWithBegin();
    void TestLogSplitWithContinueEnd();
    void TestLogSplitWithEnd();
    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitMultilineLogKeepUnmatchUnittest, TestLogSplitWithBeginContinue);
UNIT_TEST_CASE(ProcessorSplitMultilineLogKeepUnmatchUnittest, TestLogSplitWithBeginEnd);
UNIT_TEST_CASE(ProcessorSplitMultilineLogKeepUnmatchUnittest, TestLogSplitWithBegin);
UNIT_TEST_CASE(ProcessorSplitMultilineLogKeepUnmatchUnittest, TestLogSplitWithContinueEnd);
UNIT_TEST_CASE(ProcessorSplitMultilineLogKeepUnmatchUnittest, TestLogSplitWithEnd);

void ProcessorSplitMultilineLogKeepUnmatchUnittest::TestLogSplitWithBeginContinue() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["SplitType"] = "regex";
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["UnmatchedContentTreatment"] = "single_line";


    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + unmatch
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // start + unmatch
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
                   << LOG_UNMATCH << R"("
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
    // unmatch + start + continue + continue + unmatch
    // input: 1 event, 5 lines
    // output: 3 events, 5 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + start + start
    // input: 1 event, 3 lines
    // output: 3 events, 3 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + start + continue + continue
    // input: 1 event, 4 lines
    // output: 2 event, 4 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: continue
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_CONTINUE_STRING << R"("
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

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 1 + 2 + 1 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 3 + 2 + 3 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(2 + 1 + 2 + 1 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogKeepUnmatchUnittest::TestLogSplitWithBeginEnd() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["SplitType"] = "regex";
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "single_line";


    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + unmatch
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch+start+unmatch
    // input: 1 event, 3 lines
    // output: 3 events, 3 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch+start+End+unmatch
    // input: 1 event, 4 lines
    // output: 3 events, 4 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_BEGIN_STRING << R"(\n)" << LOG_END_STRING << R"(\n)" << LOG_UNMATCH
               << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: start+start
    // input: 1 event, 2 lines
    // output: 2 event, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch+start+End
    // input: 1 event, 3 lines
    // output: 2 events, 3 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch+start+unmatch+End
    // input: 1 event, 4 lines
    // output: 2 events, 4 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 1 + 0 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 2 + 0 + 2 + 3,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(2 + 3 + 2 + 2 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogKeepUnmatchUnittest::TestLogSplitWithBegin() {
    // make config
    Json::Value config;
    config["StartPattern"] = LOG_BEGIN_REGEX;
    config["UnmatchedContentTreatment"] = "single_line";
    config["SplitType"] = "regex";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch + start
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: start + start
    // input: 1 event, 2 lines
    // output: 2 events, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_BEGIN_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: start + unmatch
    // input: 1 event, 2 lines
    // output: 1 event, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"("
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
    // case: start + unmatch + \n
    // input: 1 event, 2 lines
    // output: 1 event, 2 lines
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
                        "content" : ")"
               << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_BEGIN_STRING << R"(\n)" << LOG_UNMATCH << R"(\n"
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

    // metric
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2 + 1 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2 + 2 + 2, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 1 + 0 + 0 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogKeepUnmatchUnittest::TestLogSplitWithContinueEnd() {
    // make config
    Json::Value config;
    config["ContinuePattern"] = LOG_CONTINUE_REGEX;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "single_line";
    config["SplitType"] = "regex";

    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: unmatch
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: Continue + unmatch
    // input: 1 event, 2 lines
    // output: 1 event, 2 lines
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_CONTINUE_STRING << R"("
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: Continue + Continue + end
    // input: 1 event, 3 lines
    // output: 1 event, 3 lines
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"(\n)" << LOG_CONTINUE_STRING << R"(\n)" << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: continue
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_CONTINUE_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
        // judge result
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : ")"
                   << LOG_CONTINUE_STRING << R"("
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
    // case: end
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 1 + 0 + 1,
                            ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 0 + 3 + 0 + 1, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 2 + 0 + 1 + 0,
                            ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}

void ProcessorSplitMultilineLogKeepUnmatchUnittest::TestLogSplitWithEnd() {
    // make config
    Json::Value config;
    config["EndPattern"] = LOG_END_REGEX;
    config["UnmatchedContentTreatment"] = "single_line";
    config["SplitType"] = "regex";
    // make processor

    // ProcessorSplitLogStringNative
    ProcessorSplitLogStringNative processorSplitLogStringNative;
    processorSplitLogStringNative.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processorSplitLogStringNative.Init(config));
    // ProcessorSplitMultilineLogStringNative
    ProcessorSplitMultilineLogStringNative ProcessorSplitMultilineLogStringNative;
    ProcessorSplitMultilineLogStringNative.SetContext(mContext);
    ProcessorSplitMultilineLogStringNative.SetMetricsRecordRef(ProcessorSplitMultilineLogStringNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(ProcessorSplitMultilineLogStringNative.Init(config));
    // case: end
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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
                        "content" : ")"
               << LOG_END_STRING << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch
    // input: 1 event, 1 line
    // output: 1 event, 1 line
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

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    // case: unmatch + end + unmatch
    // input: 1 event, 3 lines
    // output: 2 events, 3 lines
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
                        "content" : ")"
               << LOG_UNMATCH << R"(\n)" << LOG_END_STRING << R"(\n)" << LOG_UNMATCH << R"("
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson.str());

        // run test function
        ProcessorSplitMultilineLogStringNative.Process(eventGroup);
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
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // metric
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 1, ProcessorSplitMultilineLogStringNative.mProcMatchedEventsCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(1 + 0 + 2, ProcessorSplitMultilineLogStringNative.mProcMatchedLinesCnt->GetValue());
    APSARA_TEST_EQUAL_FATAL(0 + 1 + 1, ProcessorSplitMultilineLogStringNative.mProcUnmatchedLinesCnt->GetValue());
}
} // namespace logtail

UNIT_TEST_MAIN
