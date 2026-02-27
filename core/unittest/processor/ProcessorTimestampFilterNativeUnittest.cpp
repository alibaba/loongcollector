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

#include <limits>

#include "collection_pipeline/plugin/instance/ProcessorInstance.h"
#include "common/JsonUtil.h"
#include "plugin/processor/ProcessorTimestampFilterNative.h"
#include "unittest/Unittest.h"

using namespace logtail;

namespace logtail {

class ProcessorTimestampFilterNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestInitWithDefaultValues();
    void TestProcessWithEventTimestamp();
    void TestProcessWithSourceKey();
    void TestProcessWithBounds();
    void TestProcessWithDifferentPrecisions();
    void TestProcessWithInvalidTimestamp();
    void TestProcessWithMissingSourceKey();
    void TestProcessWithNanosecondPrecisionEvent();
    void TestSecondPrecisionOptimization();

    CollectionPipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestInitWithDefaultValues);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithEventTimestamp);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithSourceKey);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithBounds);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithDifferentPrecisions);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithInvalidTimestamp);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithMissingSourceKey);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestProcessWithNanosecondPrecisionEvent);
UNIT_TEST_CASE(ProcessorTimestampFilterNativeUnittest, TestSecondPrecisionOptimization);

PluginInstance::PluginMeta getPluginMeta() {
    PluginInstance::PluginMeta pluginMeta{"1"};
    return pluginMeta;
}

void ProcessorTimestampFilterNativeUnittest::TestInit() {
    Json::Value config;
    config["SourceKey"] = "timestamp";
    config["TimestampPrecision"] = "second";
    config["UpperBound"] = 1000000000;
    config["LowerBound"] = 1000000;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    APSARA_TEST_EQUAL_FATAL("timestamp", processor.mSourceKey);
    APSARA_TEST_EQUAL_FATAL("second", processor.mTimestampPrecision);
    APSARA_TEST_EQUAL_FATAL(false, processor.mUseNanosecondComparison);
    APSARA_TEST_EQUAL_FATAL(1000000000, processor.mUpperBoundSec);
    APSARA_TEST_EQUAL_FATAL(1000000, processor.mLowerBoundSec);
}

void ProcessorTimestampFilterNativeUnittest::TestInitWithDefaultValues() {
    Json::Value config;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    APSARA_TEST_EQUAL_FATAL("", processor.mSourceKey);
    APSARA_TEST_EQUAL_FATAL("second", processor.mTimestampPrecision);
    APSARA_TEST_EQUAL_FATAL(false, processor.mUseNanosecondComparison);
    APSARA_TEST_EQUAL_FATAL(std::numeric_limits<time_t>::max(), processor.mUpperBoundSec);
    APSARA_TEST_EQUAL_FATAL(0, processor.mLowerBoundSec);
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithEventTimestamp() {
    Json::Value config;
    // Use default (event timestamp)
    config["LowerBound"] = 1000000000;
    config["UpperBound"] = 2000000000;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t testTimestamp = 1500000000; // Within bounds
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "message" : "test message 1"
                },
                "timestamp" : )"
             << testTimestamp << R"(,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "message" : "test message 2"
                },
                "timestamp" : )"
             << (testTimestamp - 1000000000) << R"(,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "message" : "test message 3"
                },
                "timestamp" : )"
             << (testTimestamp + 1000000000) << R"(,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Only the first event should remain (within bounds)
    APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_TRUE_FATAL(outJson.find("test message 1") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("test message 2") == std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("test message 3") == std::string::npos);
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithSourceKey() {
    Json::Value config;
    config["SourceKey"] = "ts";
    config["TimestampPrecision"] = "second";
    config["LowerBound"] = 1000000000;
    config["UpperBound"] = 2000000000;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t testTimestamp = 1500000000;
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "ts" : ")"
             << testTimestamp << R"(",
                    "message" : "test message 1"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : ")"
             << (testTimestamp - 1000000000) << R"(",
                    "message" : "test message 2"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Only the first event should remain (within bounds)
    APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_TRUE_FATAL(outJson.find("test message 1") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("test message 2") == std::string::npos);
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithBounds() {
    Json::Value config;
    config["SourceKey"] = "ts";
    config["TimestampPrecision"] = "second";
    config["LowerBound"] = 1000000000;
    config["UpperBound"] = 2000000000;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "ts" : "1500000000",
                    "message" : "within bounds"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : "500000000",
                    "message" : "below lower bound"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : "2500000000",
                    "message" : "above upper bound"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : "1000000000",
                    "message" : "at lower bound"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : "2000000000",
                    "message" : "at upper bound"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Events at bounds and within bounds should remain (3 events)
    APSARA_TEST_EQUAL_FATAL(3U, eventGroupList[0].GetEvents().size());
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_TRUE_FATAL(outJson.find("within bounds") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("at lower bound") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("at upper bound") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("below lower bound") == std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("above upper bound") == std::string::npos);
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithDifferentPrecisions() {
    // Test millisecond precision
    {
        Json::Value config;
        config["SourceKey"] = "ts";
        config["TimestampPrecision"] = "millisecond";
        config["LowerBound"] = 1000000000000; // 1000000000 seconds in milliseconds
        config["UpperBound"] = 2000000000000; // 2000000000 seconds in milliseconds

        ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        APSARA_TEST_EQUAL_FATAL(true, processor.mUseNanosecondComparison);

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJsonSs;
        inJsonSs << R"({
            "events":
            [
                {
                    "contents" :
                    {
                        "ts" : "1500000000000",
                        "message" : "within bounds ms"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "ts" : "500000000000",
                        "message" : "below lower bound ms"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJsonSs.str());

        std::vector<PipelineEventGroup> eventGroupList;
        eventGroupList.emplace_back(std::move(eventGroup));
        processorInstance.Process(eventGroupList);

        APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
        std::string outJson = eventGroupList[0].ToJsonString();
        APSARA_TEST_TRUE_FATAL(outJson.find("within bounds ms") != std::string::npos);
        APSARA_TEST_TRUE_FATAL(outJson.find("below lower bound ms") == std::string::npos);
    }

    // Test nanosecond precision
    {
        Json::Value config;
        config["SourceKey"] = "ts";
        config["TimestampPrecision"] = "nanosecond";
        config["LowerBound"]
            = Json::Value(static_cast<int64_t>(1000000000000000000LL)); // 1000000000 seconds in nanoseconds
        config["UpperBound"]
            = Json::Value(static_cast<int64_t>(2000000000000000000LL)); // 2000000000 seconds in nanoseconds

        ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
        ProcessorInstance processorInstance(&processor, getPluginMeta());
        APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
        APSARA_TEST_EQUAL_FATAL(true, processor.mUseNanosecondComparison);

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        PipelineEventGroup eventGroup(sourceBuffer);
        std::stringstream inJsonSs;
        inJsonSs << R"({
            "events":
            [
                {
                    "contents" :
                    {
                        "ts" : "1500000000000000000",
                        "message" : "within bounds ns"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "ts" : "500000000000000000",
                        "message" : "below lower bound ns"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJsonSs.str());

        std::vector<PipelineEventGroup> eventGroupList;
        eventGroupList.emplace_back(std::move(eventGroup));
        processorInstance.Process(eventGroupList);

        APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
        std::string outJson = eventGroupList[0].ToJsonString();
        APSARA_TEST_TRUE_FATAL(outJson.find("within bounds ns") != std::string::npos);
        APSARA_TEST_TRUE_FATAL(outJson.find("below lower bound ns") == std::string::npos);
    }
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithInvalidTimestamp() {
    Json::Value config;
    config["SourceKey"] = "ts";
    config["TimestampPrecision"] = "second";

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "ts" : "invalid_timestamp",
                    "message" : "invalid timestamp"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "ts" : "1500000000",
                    "message" : "valid timestamp"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Events with invalid timestamp should be kept (not discarded)
    APSARA_TEST_EQUAL_FATAL(2U, eventGroupList[0].GetEvents().size());
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithMissingSourceKey() {
    Json::Value config;
    config["SourceKey"] = "nonexistent";
    config["TimestampPrecision"] = "second";

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "message" : "missing source key"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Events with missing source key should be kept (not discarded)
    APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
}

void ProcessorTimestampFilterNativeUnittest::TestProcessWithNanosecondPrecisionEvent() {
    // Test that event's nanosecond part is correctly considered when using nanosecond precision
    Json::Value config;
    config["TimestampPrecision"] = "nanosecond";
    // Set bounds around a specific nanosecond timestamp
    // 1500000000 seconds + 500000000 nanoseconds = 1500000000500000000 nanoseconds
    config["LowerBound"]
        = Json::Value(static_cast<int64_t>(1500000000000000000LL)); // 1500000000 seconds in nanoseconds
    config["UpperBound"]
        = Json::Value(static_cast<int64_t>(1500000001000000000LL)); // 1500000001 seconds in nanoseconds

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    APSARA_TEST_EQUAL_FATAL(true, processor.mUseNanosecondComparison);

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t testTimestampSec = 1500000000;
    uint32_t testTimestampNs = 500000000; // 500ms
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "message" : "within bounds with ns"
                },
                "timestamp" : )"
             << testTimestampSec << R"(,
                "timestampNanosecond" : )"
             << testTimestampNs << R"(,
                "type" : 1
            },
            {
                "contents" :
                {
                    "message" : "outside bounds with ns"
                },
                "timestamp" : )"
             << testTimestampSec << R"(,
                "timestampNanosecond" : 1500000000,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Only the first event should remain (within bounds)
    APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_TRUE_FATAL(outJson.find("within bounds with ns") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("outside bounds with ns") == std::string::npos);
}

void ProcessorTimestampFilterNativeUnittest::TestSecondPrecisionOptimization() {
    // Test that second precision uses second-level comparison (optimization)
    Json::Value config;
    config["TimestampPrecision"] = "second";
    config["LowerBound"] = 1000000000;
    config["UpperBound"] = 2000000000;

    ProcessorTimestampFilterNative& processor = *(new ProcessorTimestampFilterNative);
    ProcessorInstance processorInstance(&processor, getPluginMeta());
    APSARA_TEST_TRUE_FATAL(processorInstance.Init(config, mContext));
    APSARA_TEST_EQUAL_FATAL(false, processor.mUseNanosecondComparison);
    APSARA_TEST_EQUAL_FATAL(1000000000, processor.mLowerBoundSec);
    APSARA_TEST_EQUAL_FATAL(2000000000, processor.mUpperBoundSec);

    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    time_t testTimestampSec = 1500000000;
    uint32_t testTimestampNs = 999999999; // Max nanosecond value
    std::stringstream inJsonSs;
    inJsonSs << R"({
        "events":
        [
            {
                "contents" :
                {
                    "message" : "within bounds second precision"
                },
                "timestamp" : )"
             << testTimestampSec << R"(,
                "timestampNanosecond" : )"
             << testTimestampNs << R"(,
                "type" : 1
            },
            {
                "contents" :
                {
                    "message" : "outside bounds second precision"
                },
                "timestamp" : )"
             << (testTimestampSec - 1000000000) << R"(,
                "timestampNanosecond" : )"
             << testTimestampNs << R"(,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJsonSs.str());

    std::vector<PipelineEventGroup> eventGroupList;
    eventGroupList.emplace_back(std::move(eventGroup));
    processorInstance.Process(eventGroupList);

    // Only the first event should remain (within bounds based on second precision)
    APSARA_TEST_EQUAL_FATAL(1U, eventGroupList[0].GetEvents().size());
    std::string outJson = eventGroupList[0].ToJsonString();
    APSARA_TEST_TRUE_FATAL(outJson.find("within bounds second precision") != std::string::npos);
    APSARA_TEST_TRUE_FATAL(outJson.find("outside bounds second precision") == std::string::npos);
}

} // namespace logtail

UNIT_TEST_MAIN
