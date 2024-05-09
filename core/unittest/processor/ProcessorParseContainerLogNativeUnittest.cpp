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

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "boost/utility/string_view.hpp"
#include "common/Constants.h"
#include "common/JsonUtil.h"
#include "config/Config.h"
#include "models/LogEvent.h"
#include "processor/inner/ProcessorMergeMultilineLogNative.h"
#include "processor/inner/ProcessorParseContainerLogNative.h"
#include "processor/inner/ProcessorSplitLogStringNative.h"
#include "unittest/Unittest.h"
namespace logtail {

const std::string LOG_BEGIN_STRING = "Exception in thread 'main' java.lang.NullPointerException";
const std::string LOG_BEGIN_REGEX = R"(Exception.*)";
const std::string LOG_CONTINUE_STRING = "    at com.example.myproject.Book.getTitle(Book.java:16)";
const std::string LOG_CONTINUE_REGEX = R"(\s+at\s.*)";
const std::string LOG_END_STRING = "    ...23 more";
const std::string LOG_END_REGEX = R"(\s*\.\.\.\d+ more)";
const std::string LOG_UNMATCH = "unmatch log";

class ProcessorParseContainerLogNativeUnittest : public ::testing::Test {
public:
    void SetUp() override { mContext.SetConfigName("project##config_0"); }

    void TestInit();
    void TestContainerdLog();
    void TestIgnoringStdoutStderr();
    void TestContainerdLogWithSplit();
    void TestDockerJsonLogLineParserWithSplit();
    void TestFindAndSearchPerformance();
    void TestDockerJsonLogLineParser();
    void TestKeepingSourceWhenParseFail();
    void TestParseDockerLog();

    PipelineContext mContext;
};

UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestInit);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestContainerdLog);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestIgnoringStdoutStderr);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestContainerdLogWithSplit);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestDockerJsonLogLineParserWithSplit);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestDockerJsonLogLineParser);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestKeepingSourceWhenParseFail);
UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestParseDockerLog);
// UNIT_TEST_CASE(ProcessorParseContainerLogNativeUnittest, TestFindAndSearchPerformance);

// 生成一个随机字符串
std::string generate_random_string(size_t length) {
    std::string str(length, '\0');
    static const char alphabet[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, sizeof(alphabet) - 2);

    std::generate_n(str.begin(), length, [&]() { return alphabet[distribution(generator)]; });
    return str;
}

void ProcessorParseContainerLogNativeUnittest::TestFindAndSearchPerformance() {
    const size_t string_length = 1'0000;
    const char char_to_find = 'X'; // 这是我们要搜索的字符
    const std::string substring_to_search = "X"; // 这是我们要搜索的子字符串
    const int num_trials = 100000000;
    std::string random_string = generate_random_string(string_length);

    // Benchmark string_view.find
    std::chrono::duration<double, std::milli> boost_find_duration_total;
    for (int i = 0; i < num_trials; ++i) {
        boost::string_view string_view(random_string); // 创建 boost::string_view

        auto start = std::chrono::high_resolution_clock::now();
        string_view.find(char_to_find);
        auto end = std::chrono::high_resolution_clock::now();

        boost_find_duration_total += end - start;
    }
    double boost_find_avg_duration = boost_find_duration_total.count();
    std::cout << "Total string_view.find took " << boost_find_avg_duration << " milliseconds." << std::endl;

    // Benchmark std::find
    std::chrono::duration<double, std::milli> find_duration_total;
    for (int i = 0; i < num_trials; ++i) {
        boost::string_view string_view(random_string); // 创建 boost::string_view

        auto start = std::chrono::high_resolution_clock::now();
        std::find(string_view.begin(), string_view.end(), char_to_find);
        auto end = std::chrono::high_resolution_clock::now();

        find_duration_total += end - start;
    }
    double find_avg_duration = find_duration_total.count();
    std::cout << "Total std::find took " << find_avg_duration << " milliseconds." << std::endl;

    // Benchmark std::search
    std::chrono::duration<double, std::milli> search_duration_total;
    for (int i = 0; i < num_trials; ++i) {
        boost::string_view string_view(random_string); // 创建 boost::string_view

        auto start = std::chrono::high_resolution_clock::now();
        std::search(string_view.begin(), string_view.end(), substring_to_search.begin(), substring_to_search.end());
        auto end = std::chrono::high_resolution_clock::now();

        search_duration_total += end - start;
    }
    double search_avg_duration = search_duration_total.count();
    std::cout << "Total std::search took " << search_avg_duration << " milliseconds." << std::endl;
}

void ProcessorParseContainerLogNativeUnittest::TestInit() {
    // 参数非法情况
    Json::Value config;
    config["IgnoringStdout"] = "true";
    config["IgnoringStderr"] = 1;
    ProcessorParseContainerLogNative processor;
    processor.SetContext(mContext);
    processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
}

void ProcessorParseContainerLogNativeUnittest::TestIgnoringStdoutStderr() {
    // 测试IgnoringStdout和IgnoringStderr都为true
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = true;
        config["IgnoringStderr"] = true;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        {
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
            std::string inJson = R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:07.818486411+08:00 stdout P  in thread"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:08.818486411+08:00 stdout P   'main'"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  java.lang.NullPoinntterException"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc1"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc2"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc3"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  abc4"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson);
            // run test function
            processor.Process(eventGroup);
            std::stringstream expectJson;
            expectJson << R"({
                "metadata": {
                    "container.type": "containerd_text"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
    // 测试IgnoringStdout 为true
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = true;
        config["IgnoringStderr"] = false;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        {
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
            std::string inJson = R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:07.818486411+08:00 stdout P  in thread"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:08.818486411+08:00 stdout P   'main'"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  java.lang.NullPoinntterException"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc1"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc2"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc3"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  abc4"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson);
            // run test function
            processor.Process(eventGroup);
            std::stringstream expectJson;
            expectJson << R"({
                "events": [
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc1"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc2"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc3"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ],
                "metadata": {
                    "container.type": "containerd_text"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
    // 测试 IgnoringStderr 为true
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = false;
        config["IgnoringStderr"] = true;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        {
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
            std::string inJson = R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:07.818486411+08:00 stdout P  in thread"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:08.818486411+08:00 stdout P   'main'"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  java.lang.NullPoinntterException"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc1"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc2"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc3"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  abc4"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson);
            // run test function
            processor.Process(eventGroup);
            std::stringstream expectJson;
            expectJson << R"({
                "events": [
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:06.818486411+08:00",
                            "content": "Exception"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:07.818486411+08:00",
                            "content": " in thread"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:08.818486411+08:00",
                            "content": "  'main'"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " java.lang.NullPoinntterException"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc4"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ],
                "metadata": {
                    "container.type": "containerd_text",
                    "has.part.log": "P"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
    // 测试都不为true
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = false;
        config["IgnoringStderr"] = false;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // make eventGroup
        auto sourceBuffer = std::make_shared<SourceBuffer>();
        {
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
            std::string inJson = R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:07.818486411+08:00 stdout P  in thread"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:08.818486411+08:00 stdout P   'main'"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  java.lang.NullPoinntterException"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc1"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc2"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stderr F  abc3"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    },
                    {
                        "contents" :
                        {
                            "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  abc4"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson);
            // run test function
            processor.Process(eventGroup);
            std::stringstream expectJson;
            expectJson << R"({
                "events": [
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:06.818486411+08:00",
                            "content": "Exception"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:07.818486411+08:00",
                            "content": " in thread"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "P": "",
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:08.818486411+08:00",
                            "content": "  'main'"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " java.lang.NullPoinntterException"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc1"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc2"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stderr",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc3"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    },
                    {
                        "contents": {
                            "_source_": "stdout",
                            "_time_": "2024-01-05T23:28:09.818486411+08:00",
                            "content": " abc4"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ],
                "metadata": {
                    "container.type": "containerd_text",
                    "has.part.log": "P"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseContainerLogNativeUnittest::TestContainerdLog() {
    // make config
    Json::Value config;
    // make ProcessorParseContainerLogNative
    ProcessorParseContainerLogNative processor;
    processor.SetContext(mContext);
    processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    {
        // case1: PartLogFlag存在，第三个空格存在但空格后无内容
        // case2: PartLogFlag存在，第三个空格不存在
        // case3: PartLogFlag不存在，第二个空格存在
        // case4: 第二个空格不存在
        // case5: 第一个空格不存在
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P "
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout "
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run test function
        processor.Process(eventGroup);
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "P": "",
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": "P"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ],
            "metadata": {
                "container.type": "containerd_text",
                "has.part.log": "P"
            }
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
    {
        // case：正常情况下的日志
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:07.818486411+08:00 stdout P  in thread"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:08.818486411+08:00 stdout P   'main'"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:09.818486411+08:00 stdout F  java.lang.NullPoinntterException"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run test function
        processor.Process(eventGroup);
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "P": "",
                        "_source_": "stdout",
                        "_time_" : "2024-01-05T23:28:06.818486411+08:00",
                        "content" : "Exception"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "P": "",
                        "_source_": "stdout",
                        "_time_" : "2024-01-05T23:28:07.818486411+08:00",
                        "content" : " in thread"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "P": "",
                        "_source_": "stdout",
                        "_time_" : "2024-01-05T23:28:08.818486411+08:00",
                        "content" : "  'main'"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "_source_": "stdout",
                        "_time_" : "2024-01-05T23:28:09.818486411+08:00",
                        "content" : " java.lang.NullPoinntterException"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ],
            "metadata": {
                "container.type": "containerd_text",
                "has.part.log": "P"
            }
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }
}

void ProcessorParseContainerLogNativeUnittest::TestContainerdLogWithSplit() {
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
    inJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P Exception i\n2024-01-05T23:28:06.818486411+08:00 stdout P n thread  'main' java.lang.Null\n2024-01-05T23:28:06.818486411+08:00 stdout P PoinntterExcept\n2024-01-05T23:28:06.818486411+08:00 stdout F ion\n2024-01-05T23:28:06.818486411+08:00 stdout F      at com.example.myproject.Book.getTitle\n2024-01-05T23:28:06.818486411+08:00 stdout F      at com.example.myproject.Book.getTitle\n2024-01-05T23:28:06.818486411+08:00 stdout P      at com.exa\n2024-01-05T23:28:06.818486411+08:00 stdout P mple.myproject.Book.g\n2024-01-05T23:28:06.818486411+08:00 stdout P etTit\n2024-01-05T23:28:06.818486411+08:00 stdout F le\n2024-01-05T23:28:06.818486411+08:00 stdout F     ...23 more\n2024-01-05T23:31:06.818486411+08:00 stdout P Exception i\n2024-01-05T23:28:06.818486411+08:00 stdout P n thread  'main' java.lang.Null\n2024-01-05T23:28:06.818486411+08:00 stdout P PoinntterExcept\n2024-01-05T23:28:06.818486411+08:00 stdout F ion\n2024-01-05T23:28:06.818486411+08:00 stdout F      at com.example.myproject.Book.getTitle\n2024-01-05T23:28:06.818486411+08:00 stdout F      at com.example.myproject.Book.getTitle\n2024-01-05T23:28:06.818486411+08:00 stdout P      at com.exa\n2024-01-05T23:28:06.818486411+08:00 stdout P mple.myproject.Book.g\n2024-01-05T23:28:06.818486411+08:00 stdout P etTit\n2024-01-05T23:28:06.818486411+08:00 stdout F le\n2024-01-05T23:28:06.818486411+08:00 stdout F     ...23 more"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());

    // ProcessorSplitLogStringNative
    {
        // make config
        Json::Value config;
        // make ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorParseContainerLogNative
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = false;
        config["IgnoringStderr"] = true;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorMergeMultilineLogNative
    {
        // make config
        Json::Value config;
        config["MergeType"] = "flag";
        config["UnmatchedContentTreatment"] = "single_line";
        // make ProcessorMergeMultilineLogNative
        ProcessorMergeMultilineLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorMergeMultilineLogNative
    {
        // make config
        Json::Value config;
        config["StartPattern"] = LOG_BEGIN_REGEX;
        config["MergeType"] = "regex";
        config["UnmatchedContentTreatment"] = "single_line";
        // make ProcessorMergeMultilineLogNative
        ProcessorMergeMultilineLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }

    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "_source_": "stdout",
                    "_time_": "2024-01-05T23:28:06.818486411+08:00",
                    "content" : "Exception in thread  'main' java.lang.NullPoinntterException\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n    ...23 more"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "_source_": "stdout",
                    "_time_": "2024-01-05T23:31:06.818486411+08:00",
                    "content" : "Exception in thread  'main' java.lang.NullPoinntterException\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n    ...23 more"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "metadata":{
            "container.type":"containerd_text"
        }
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseContainerLogNativeUnittest::TestDockerJsonLogLineParserWithSplit() {
    // make eventGroup
    auto sourceBuffer = std::make_shared<SourceBuffer>();
    PipelineEventGroup eventGroup(sourceBuffer);
    std::stringstream inJson;
    eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
    inJson << R"({
        "events": [
            {
                "contents": {
                    "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793563414Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793566551Z\"}\n{\"log\":\"    ...23 more\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793569514Z\"}\n{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:55:17.514807564Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:55:17.514841003Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:55:17.514853553Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:55:17.514856538Z\"}\n{\"log\":\"    ...23 more\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:55:17.514858843Z\"}"
                },
                "timestamp": 12345678901,
                "timestampNanosecond": 0,
                "type": 1
            }
        ]
    })";
    eventGroup.FromJsonString(inJson.str());

    // ProcessorSplitLogStringNative
    {
        // make config
        Json::Value config;

        // make ProcessorSplitLogStringNative
        ProcessorSplitLogStringNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorParseContainerLogNative
    {
        // make config
        Json::Value config;
        config["IgnoringStdout"] = false;
        config["IgnoringStderr"] = true;
        // make ProcessorParseContainerLogNative
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorMergeMultilineLogNative
    {
        // make config
        Json::Value config;
        config["MergeType"] = "flag";
        config["UnmatchedContentTreatment"] = "single_line";
        // make ProcessorMergeMultilineLogNative
        ProcessorMergeMultilineLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }
    // ProcessorMergeMultilineLogNative
    {
        // make config
        Json::Value config;
        config["StartPattern"] = LOG_BEGIN_REGEX;
        config["MergeType"] = "regex";
        config["UnmatchedContentTreatment"] = "single_line";
        // make ProcessorMergeMultilineLogNative
        ProcessorMergeMultilineLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // run test function
        processor.Process(eventGroup);
    }

    // judge result
    std::stringstream expectJson;
    expectJson << R"({
        "events" :
        [
            {
                "contents" :
                {
                    "_source_": "stdout",
                    "_time_": "2024-02-19T03:49:37.793533014Z",
                    "content" : "Exception in thread  \"main\" java.lang.NullPoinntterException\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n    ...23 more"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            },
            {
                "contents" :
                {
                    "_source_": "stdout",
                    "_time_": "2024-02-19T03:55:17.514807564Z",
                    "content" : "Exception in thread  \"main\" java.lang.NullPoinntterException\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n     at com.example.myproject.Book.getTitle\n    ...23 more"
                },
                "timestamp" : 12345678901,
                "timestampNanosecond" : 0,
                "type" : 1
            }
        ],
        "metadata":{
            "container.type":"docker_json-file"
        }
    })";
    std::string outJson = eventGroup.ToJsonString();
    APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
}

void ProcessorParseContainerLogNativeUnittest::TestDockerJsonLogLineParser() {
    Json::Value config;
    config["IgnoringStdout"] = false;
    config["IgnoringStderr"] = false;
    ProcessorParseContainerLogNative processor;
    processor.SetContext(mContext);
    processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
    APSARA_TEST_TRUE_FATAL(processor.Init(config));
    // log 测试
    {
        // log 不存在情况下
        {
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log1\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());
            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log1\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // log为空
        {
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());
            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "_source_": "stdout",
                            "_time_": "2024-02-19T03:49:37.793559367Z",
                            "content": ""
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // log为不是string
        {
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":1,\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());
            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":1,\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
    // time
    {
        // time不存在
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time1\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time1\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // time 为空
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // time 不是string
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":1}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":1}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
    // stream
    {
        // stream不存在
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream1\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream1\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // stream非法
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
        // stream不是string
        {
            // make eventGroup
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            std::stringstream inJson;
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            inJson << R"({
                "events": [
                    {
                        "contents": {
                            "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":1,\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp": 12345678901,
                        "timestampNanosecond": 0,
                        "type": 1
                    }
                ]
            })";
            eventGroup.FromJsonString(inJson.str());

            // run test function
            processor.Process(eventGroup);
            // judge result
            std::stringstream expectJson;
            expectJson << R"({
                "events" :
                [
                    {
                        "contents" :
                        {
                            "content": "{\"log\":\"Exception in thread  \"main\" java.lang.NullPoinntterException\nn\\n\",\"stream\":1,\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                        },
                        "timestamp" : 12345678901,
                        "timestampNanosecond" : 0,
                        "type" : 1
                    }
                ],
                "metadata":{
                    "container.type":"docker_json-file"
                }
            })";
            std::string outJson = eventGroup.ToJsonString();
            APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
        }
    }
}

void ProcessorParseContainerLogNativeUnittest::TestKeepingSourceWhenParseFail() {
    // containerd_text
    {
        Json::Value config;
        config["KeepingSourceWhenParseFail"] = false;

        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));

        auto sourceBuffer = std::make_shared<SourceBuffer>();
        // case1: PartLogFlag存在，第三个空格存在但空格后无内容
        // case2: PartLogFlag存在，第三个空格不存在
        // case3: PartLogFlag不存在，第二个空格存在
        // case4: 第二个空格不存在
        // case5: 第一个空格不存在
        PipelineEventGroup eventGroup(sourceBuffer);
        eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
        std::string inJson = R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P "
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout P"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout "
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00 stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "content" : "2024-01-05T23:28:06.818486411+08:00stdout"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ]
        })";
        eventGroup.FromJsonString(inJson);
        // run test function
        processor.Process(eventGroup);
        std::stringstream expectJson;
        expectJson << R"({
            "events" :
            [
                {
                    "contents" :
                    {
                        "P": "",
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": "P"
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                },
                {
                    "contents" :
                    {
                        "_source_": "stdout",
                        "_time_": "2024-01-05T23:28:06.818486411+08:00",
                        "content": ""
                    },
                    "timestamp" : 12345678901,
                    "timestampNanosecond" : 0,
                    "type" : 1
                }
            ],
            "metadata": {
                "container.type": "containerd_text",
                "has.part.log": "P"
            }
        })";
        std::string outJson = eventGroup.ToJsonString();
        APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
    }

    // docker
    {
        Json::Value config;
        config["IgnoringStdout"] = false;
        config["IgnoringStderr"] = false;
        config["KeepingSourceWhenParseFail"] = false;
        ProcessorParseContainerLogNative processor;
        processor.SetContext(mContext);
        processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");
        APSARA_TEST_TRUE_FATAL(processor.Init(config));
        // log 测试
        {
            // log 不存在情况下
            {
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log1\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());
                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // log为空
            {
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());
                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "events": [
                        {
                            "contents" :
                            {
                                "_source_": "stdout",
                                "_time_": "2024-02-19T03:49:37.793559367Z",
                                "content": ""
                            },
                            "timestamp" : 12345678901,
                            "timestampNanosecond" : 0,
                            "type" : 1
                        }
                    ],
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // log为不是string
            {
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":1,\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());
                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
        }
        // time
        {
            // time不存在
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time1\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // time 为空
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":\"\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // time 不是string
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"stdout\",\"time\":1}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
        }
        // stream
        {
            // stream不存在
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream1\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // stream非法
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":\"stdout\",\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
            // stream不是string
            {
                // make eventGroup
                auto sourceBuffer = std::make_shared<SourceBuffer>();
                PipelineEventGroup eventGroup(sourceBuffer);
                std::stringstream inJson;
                eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT,
                                       ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
                inJson << R"({
                    "events": [
                        {
                            "contents": {
                                "content": "{\"log\":\"Exception in thread  \\\"main\\\" java.lang.NullPoinntterException\\n\",\"stream\":1,\"time\":\"2024-02-19T03:49:37.793533014Z\"}\n{\"log\":\"     at com.example.myproject.Book.getTitle\\n\",\"stream\":\"std\",\"time\":\"2024-02-19T03:49:37.793559367Z\"}"
                            },
                            "timestamp": 12345678901,
                            "timestampNanosecond": 0,
                            "type": 1
                        }
                    ]
                })";
                eventGroup.FromJsonString(inJson.str());

                // run test function
                processor.Process(eventGroup);
                // judge result
                std::stringstream expectJson;
                expectJson << R"({
                    "metadata":{
                        "container.type":"docker_json-file"
                    }
                })";
                std::string outJson = eventGroup.ToJsonString();
                APSARA_TEST_STREQ(CompactJson(expectJson.str()).c_str(), CompactJson(outJson).c_str());
            }
        }
    }
}

void ProcessorParseContainerLogNativeUnittest::TestParseDockerLog() {
    {
        DockerLog dockerLog;

        std::string str
            = R"({"log":"Exception in thread \"main\" java.lang.NullPointerExceptionat  com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitle\n","stream":"stdout","time":"2024-04-09T02:21:28.744862802Z"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_TRUE(result);
        APSARA_TEST_STREQ(
            "Exception in thread \"main\" java.lang.NullPointerExceptionat  com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat "
            "com.example.myproject.Book.getTitle\n",
            dockerLog.log.to_string().c_str());
        APSARA_TEST_EQUAL("stdout", dockerLog.stream);
        APSARA_TEST_EQUAL("2024-04-09T02:21:28.744862802Z", dockerLog.time);
        delete[] buffer;
    }
    {
        DockerLog dockerLog;
        std::string str = R"({"log":"Hello, World!","stream":"stdout","time":"2021-12-01T00:00:00.000Z)";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
    {
        DockerLog dockerLog;
        std::string str = R"()";
        int32_t size = str.size();

        char* buffer = new char[1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }

    // Test with a valid log message but missing stream and time fields.
    {
        DockerLog dockerLog;
        std::string str = R"({"log":"Hello, World!"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
    // Test with a valid log message and stream but missing time field.
    {
        DockerLog dockerLog;
        std::string str = R"({"log":"Hello, World!","stream":"stdout"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
    // Test with a valid log message and time but missing stream field.
    {
        DockerLog dockerLog;
        std::string str = R"({"log":"Hello, World!","time":"2021-12-01T00:00:00.000Z"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
    // Test with a log message that contains special characters.
    {
        DockerLog dockerLog;
        std::string str
            = R"({"log":"Hello, @#$%^&*()_+{}|:\"<>?~`","stream":"stdout","time":"2021-12-01T00:00:00.000Z"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_TRUE(result);
        APSARA_TEST_STREQ("Hello, @#$%^&*()_+{}|:\"<>?~`", dockerLog.log.to_string().c_str());
        APSARA_TEST_EQUAL("stdout", dockerLog.stream);
        APSARA_TEST_EQUAL("2021-12-01T00:00:00.000Z", dockerLog.time);
        delete[] buffer;
    }
    // Test with a log message that contains escape characters and special characters.
    {
        DockerLog dockerLog;
        std::string str
            = R"({"log":"ברי צקלהHello 你好, \\u \" \ / \b \f \n \r \t 🌍 \u0069\u004c\u006f\u0067\u0074\u0061\u0069\u006c\u0020\u4e3a\u53ef\u89c2\u6d4b\u573a\u666f\u800c\u751f\uff0c\u62e5\u6709\u7684\u8f7b\u91cf\u7ea7\u3001\u9ad8\u6027\u80fd\u3001\u81ea\u52a8\u5316\u914d\u7f6e\u7b49\u8bf8\u591a\u751f\u4ea7\u7ea7\u522b\u7279\u6027\uff0c\u5728\u963f\u91cc\u5df4\u5df4\u4ee5\u53ca\u5916\u90e8\u6570\u4e07\u5bb6\u963f\u91cc\u4e91\u5ba2\u6237\u5185\u90e8\u5e7f\u6cdb\u5e94\u7528\u3002\u4f60\u53ef\u4ee5\u5c06\u5b83\u90e8\u7f72\u4e8e\u7269\u7406\u673a\uff0c\u865a\u62df\u673a\uff0c\u004b\u0075\u0062\u0065\u0072\u006e\u0065\u0074\u0065\u0073\u7b49\u591a\u79cd\u73af\u5883\u4e2d\u6765\u91c7\u96c6\u9065\u6d4b\u6570\u636e\uff0c\u4f8b\u5982\u006c\u006f\u0067\u0073\u3001\u0074\u0072\u0061\u0063\u0065\u0073\u548c\u006d\u0065\u0074\u0072\u0069\u0063\u0073\u3002\n","stream":"stdout","time":"2021-12-01T00:00:00.000Z"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_TRUE(result);
        APSARA_TEST_STREQ("ברי צקלהHello 你好, \\u \" \\ / \b \f \n \r \t 🌍 iLogtail "
                          "为可观测场景而生，拥有的轻量级、高性能、自动化配置等诸多生产级别特性，在阿里巴巴以及外部数万"
                          "家阿里云客户内部广泛应用。你可以将它部署于物理机，虚拟机，Kubernetes等多种环境中来采集遥测数"
                          "据，例如logs、traces和metrics。\n",
                          dockerLog.log.to_string().c_str());
        APSARA_TEST_EQUAL("stdout", dockerLog.stream);
        APSARA_TEST_EQUAL("2021-12-01T00:00:00.000Z", dockerLog.time);
        delete[] buffer;
    }
    // Test with a incomplete log
    {
        DockerLog dockerLog;
        std::string str = R"({"log":"Hello, Wor)";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
    // Test with a incomplete log
    {
        DockerLog dockerLog;
        std::string str = R"(og":"Hello, world","stream":"stdout","time":"2021-12-01T00:00:00.000Z"})";
        int32_t size = str.size();

        char* buffer = new char[size + 1]();
        strcpy(buffer, str.c_str());

        bool result = ProcessorParseContainerLogNative::ParseDockerLog(buffer, size, dockerLog);

        APSARA_TEST_FALSE(result);
        delete[] buffer;
    }
}

} // namespace logtail

UNIT_TEST_MAIN