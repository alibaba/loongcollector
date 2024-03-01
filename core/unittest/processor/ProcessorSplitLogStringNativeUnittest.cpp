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
#include <sstream>

#include "common/Constants.h"
#include "common/JsonUtil.h"
#include "config/Config.h"
#include "processor/ProcessorSplitLogStringNative.h"
#include "unittest/Unittest.h"

namespace logtail {

class ProcessorSplitLogStringNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestProcessJson();
    void TestProcessCommon();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestProcessJson);
UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestProcessCommon);

void ProcessorSplitLogStringNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["AppendingLogPositionMeta"] = false;

    std::string pluginId = "testID";
    ProcessorSplitLogStringNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
}

void ProcessorSplitLogStringNativeUnittest::TestProcessJson() {
    // make config
    Json::Value config;
    config["SplitChar"] = '\0';
    config["AppendingLogPositionMeta"] = true;
    // make events
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::string inJson = R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\n\"k1\":\"v1\"\n}\u0000{\n\"k2\":\"v2\"\n}",
                    "__file_offset__": "0"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    APSARA_TEST_TRUE_FATAL(eventGroup.FromJsonString(inJson));
    // run function
    ProcessorSplitLogStringNative processor;
    processor.SetContext(mContext);

    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.Process(eventGroup);
    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "content" : "{\n\"k1\":\"v1\"\n}"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": ")"
               << strlen(R"({n"k1":"v1"n}0)") << R"(",
                    "content" : "{\n\"k2\":\"v2\"\n}"
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
    APSARA_TEST_EQUAL_FATAL(2, processor.GetContext().GetProcessProfile().splitLines);
}

void ProcessorSplitLogStringNativeUnittest::TestProcessCommon() {
    // make config
    Json::Value config;
    config["AppendingLogPositionMeta"] = false;
    // make events
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
            },
            {
                "contents" :
                {
                    "__file_offset__": "0",
                    "content" : "line3\nline4"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson);
    // run function
    ProcessorSplitLogStringNative processor;
    processor.SetContext(mContext);
    std::string pluginId = "testID";
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    processor.Process(eventGroup);
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
            },
            {
                "contents" :
                {
                    "content" : "line3"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line4"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observability
    APSARA_TEST_EQUAL_FATAL(4, processor.GetContext().GetProcessProfile().splitLines);
}

} // namespace logtail

UNIT_TEST_MAIN