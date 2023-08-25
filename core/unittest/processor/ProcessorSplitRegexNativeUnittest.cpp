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
#include "unittest/Unittest.h"
#include "common/Constants.h"
#include "config/Config.h"
#include "processor/ProcessorSplitRegexNative.h"
#include "models/LogEvent.h"

namespace logtail {

class ProcessorSplitRegexNativeUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
    }

    void TestInit();
    void TestProcessEventWholeLine();
    void TestProcessEventDiscardUnmatch();
    void TestProcessEventKeepUnmatch();
    void TestProcess();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventWholeLine);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventDiscardUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcessEventKeepUnmatch);
UNIT_TEST_CASE(ProcessorSplitRegexNativeUnittest, TestProcess);

void ProcessorSplitRegexNativeUnittest::TestInit() {
    Config config;
    config.mLogBeginReg = ".*";
    config.mDiscardUnmatch = false;
    config.mAdvancedConfig.mEnableLogPositionMeta = false;
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventWholeLine() {
    // make config
    Config config;
    config.mLogBeginReg = ".*";
    config.mDiscardUnmatch = false;
    config.mAdvancedConfig.mEnableLogPositionMeta = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    logEvent->SetContent(StringView(DEFAULT_CONTENT_KEY), StringView("line1\nline2"));
    logEvent->SetContent(EVENT_META_LOG_FILE_OFFSET, StringView("0"));
    eventGroup.AddEvent(std::move(logEvent));
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, eventGroup.GetEvents()[0], newEvents);
    APSARA_TEST_EQUAL_FATAL(2L, newEvents.size());
    APSARA_TEST_TRUE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line1"), newEvents[0].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_FALSE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
    APSARA_TEST_TRUE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line2"), newEvents[1].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_FALSE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventDiscardUnmatch() {
    // make config
    Config config;
    config.mLogBeginReg = "line.*";
    config.mDiscardUnmatch = true;
    config.mAdvancedConfig.mEnableLogPositionMeta = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    logEvent->SetContent(StringView(DEFAULT_CONTENT_KEY), StringView("badline1\ncontinue\nline2\ncontinue"));
    logEvent->SetContent(EVENT_META_LOG_FILE_OFFSET, StringView("0"));
    eventGroup.AddEvent(std::move(logEvent));
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, eventGroup.GetEvents()[0], newEvents);
    APSARA_TEST_EQUAL_FATAL(1L, newEvents.size());
    APSARA_TEST_TRUE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line2\ncontinue"),
                            newEvents[0].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_FALSE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
}

void ProcessorSplitRegexNativeUnittest::TestProcessEventKeepUnmatch() {
    // make config
    Config config;
    config.mLogBeginReg = "line.*";
    config.mDiscardUnmatch = false;
    config.mAdvancedConfig.mEnableLogPositionMeta = false;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    logEvent->SetContent(StringView(DEFAULT_CONTENT_KEY), StringView("badline1\ncontinue\nline2\ncontinue"));
    logEvent->SetContent(EVENT_META_LOG_FILE_OFFSET, StringView("0"));
    eventGroup.AddEvent(std::move(logEvent));
    std::string logPath("/var/log/message");
    EventsContainer newEvents;
    // run test function
    processor.ProcessEvent(eventGroup, logPath, eventGroup.GetEvents()[0], newEvents);
    APSARA_TEST_EQUAL_FATAL(2L, newEvents.size());
    APSARA_TEST_TRUE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("badline1\ncontinue"),
                            newEvents[0].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_FALSE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
    APSARA_TEST_TRUE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line2\ncontinue"),
                            newEvents[1].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_FALSE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
}

void ProcessorSplitRegexNativeUnittest::TestProcess() {
    // make config
    Config config;
    config.mLogBeginReg = "line.*";
    config.mDiscardUnmatch = false;
    config.mAdvancedConfig.mEnableLogPositionMeta = true;
    // make processor
    ProcessorSplitRegexNative processor;
    processor.SetContext(mContext);
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    eventGroup.SetMetadata(EVENT_META_LOG_FILE_PATH, StringView("/var/log/message"));
    auto logEvent = LogEvent::CreateEvent(sourceBuffer);
    logEvent->SetContent(StringView(DEFAULT_CONTENT_KEY), StringView("line1\ncontinue\nline2\ncontinue"));
    logEvent->SetContent(EVENT_META_LOG_FILE_OFFSET, StringView("0"));
    eventGroup.AddEvent(std::move(logEvent));
    std::string logPath("/var/log/message");
    // run test function
    processor.Process(eventGroup);
    auto& newEvents = eventGroup.GetEvents();
    APSARA_TEST_EQUAL_FATAL(2L, newEvents.size());
    APSARA_TEST_TRUE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line1\ncontinue"),
                            newEvents[0].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_TRUE_FATAL(newEvents[0].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
    APSARA_TEST_EQUAL_FATAL(StringView("0"), newEvents[0].Cast<LogEvent>().GetContent(EVENT_META_LOG_FILE_OFFSET));

    APSARA_TEST_TRUE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_EQUAL_FATAL(StringView("line2\ncontinue"),
                            newEvents[1].Cast<LogEvent>().GetContent(DEFAULT_CONTENT_KEY));
    APSARA_TEST_TRUE_FATAL(newEvents[1].Cast<LogEvent>().HasContent(EVENT_META_LOG_FILE_OFFSET));
    APSARA_TEST_EQUAL_FATAL(StringView(std::to_string(sizeof("line1\ncontinue"))),
                            newEvents[1].Cast<LogEvent>().GetContent(EVENT_META_LOG_FILE_OFFSET));
    // check observability
    APSARA_TEST_EQUAL_FATAL(4, processor.GetContext().GetProcessProfile().feedLines);
    APSARA_TEST_EQUAL_FATAL(2, processor.GetContext().GetProcessProfile().splitLines);
}

class ProcessorSplitRegexNativeLogSplitUnittest : public ::testing::Test {
public:
    void SetUp() override {
        mContext.SetConfigName("project##config_0");
        mContext.SetLogstoreName("logstore");
        mContext.SetProjectName("project");
        mContext.SetRegion("cn-shanghai");
    }

    void TestWholeLineSplit();
    // ...

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorSplitRegexNativeLogSplitUnittest, TestWholeLineSplit);

void ProcessorSplitRegexNativeLogSplitUnittest::TestWholeLineSplit() {
    // ...
}

} // namespace logtail

UNIT_TEST_MAIN