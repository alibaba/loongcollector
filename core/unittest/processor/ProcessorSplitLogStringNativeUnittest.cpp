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
#include "config/PipelineConfig.h"
#include "pipeline/plugin/instance/ProcessorInstance.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
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

UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestInit)
UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestProcessJson)
UNIT_TEST_CASE(ProcessorSplitLogStringNativeUnittest, TestProcessCommon)

PluginInstance::PluginMeta getPluginMeta(){
    PluginInstance::PluginMeta pluginMeta{"1"};
    return pluginMeta;
}

void ProcessorSplitLogStringNativeUnittest::TestInit() {
    // make config
    Json::Value config;
    config["AppendingLogPositionMeta"] = false;

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
    std::stringstream inJson;
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "{\n\"k1\":\"v1\"\n}\u0000{\n\"k2\":\"v2\"\n}"
                },
                "fileOffset": 1,
                "rawSize": )"
           << strlen(R"({n"k1":"v1"n}0{n"k2":"v2"n})") << R"(,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    APSARA_TEST_TRUE_FATAL(eventGroup.FromJsonString(inJson.str()));
    // run function
    ProcessorSplitLogStringNative processor;
    processor.SetContext(mContext);

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
                    "__file_offset__": "1",
                    "content" : "{\n\"k1\":\"v1\"\n}"
                },
                "fileOffset": 1,
                "rawSize": )"
               << strlen(R"({n"k1":"v1"n}0)") << R"(,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "__file_offset__": ")"
               << strlen(R"({n"k1":"v1"n}0)") + 1 << R"(",
                    "content" : "{\n\"k2\":\"v2\"\n}"
                },
                "fileOffset": )"
               << strlen(R"({n"k1":"v1"n}0)") + 1 << R"(,
                "rawSize": )"
               << strlen(R"({n"k2":"v2"n})") << R"(,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString(true);
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
                    "content" : "line1\nline2"
                },
                "fileOffset": 1,
                "rawSize": 12,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line3\nline4"
                },
                "fileOffset": 0,
                "rawSize": 11,
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
                "fileOffset": 1,
                "rawSize": 6,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line2"
                },
                "fileOffset": 7,
                "rawSize": 6,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line3"
                },
                "fileOffset": 0,
                "rawSize": 6,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "content" : "line4"
                },
                "fileOffset": 6,
                "rawSize": 5,
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    std::string outJson = eventGroup.ToJsonString(true);
    APSARA_TEST_STREQ_FATAL(CompactJson(expectJson).c_str(), CompactJson(outJson).c_str());
    // check observability
    APSARA_TEST_EQUAL_FATAL(4, processor.GetContext().GetProcessProfile().splitLines);
}

} // namespace logtail

UNIT_TEST_MAIN