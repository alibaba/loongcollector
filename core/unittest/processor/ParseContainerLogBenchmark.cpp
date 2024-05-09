// Copyright 2023 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>

#include "config/Config.h"
#include "models/LogEvent.h"
#include "plugin/instance/ProcessorInstance.h"
#include "processor/inner/ProcessorParseContainerLogNative.h"
#include "unittest/Unittest.h"


using namespace logtail;


std::string formatSize(long long size) {
    static const char* units[] = {" B", "KB", "MB", "GB", "TB"};
    int index = 0;
    double doubleSize = static_cast<double>(size);
    while (doubleSize >= 1024.0 && index < 4) {
        doubleSize /= 1024.0;
        index++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << std::setw(6) << std::setfill(' ') << doubleSize << " " << units[index];
    return ss.str();
}

static void BM_DockerJson(int size, int batchSize) {
    logtail::Logger::Instance().InitGlobalLoggers();

    PipelineContext mContext;
    mContext.SetConfigName("project##config_0");

    Json::Value config;
    config["IgnoringStdout"] = false;
    config["IgnoringStderr"] = false;
    ProcessorParseContainerLogNative processor;
    processor.SetContext(mContext);
    processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");

    std::string data1
        = R"({"log":"Exception in thread \"main\" java.lang.NullPointerExceptionat  com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitle\n","stream":"stdout","time":"2024-04-07T08:02:40.873971412Z"})";
    std::string data2
        = R"({"log":"    at com.example.myproject.Book.getTitle\n","stream":"stdout","time":"2024-04-07T08:02:40.873976048Z"})";
    std::string data3
        = R"({"log":"    at com.example.myproject.Book.getTitle\n","stream":"stdout","time":"2024-04-07T08:02:40.873978568Z"})";
    std::string data4
        = R"({"log":"    at com.example.myproject.Book.getTitle\n","stream":"stdout","time":"2024-04-07T08:02:40.87398107Z"})";
    std::cout << "log size:\t" << formatSize((data1.size() + data2.size() + data3.size() + data4.size() + 7 * 4) * size)
              << std::endl;

    // make events
    Json::Value root;
    Json::Value events;
    for (int i = 0; i < size; i++) {
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data1;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data2;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data3;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data4;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
    }

    root["events"] = events;
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(root, &oss);
    std::string inJson = oss.str();

    bool init = processor.Init(config);
    if (init) {
        int count = 0;
        // Perform setup here
        uint64_t durationTime = 0;
        for (int i = 0; i < batchSize; i++) {
            count++;
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::DOCKER_JSON_FILE);
            eventGroup.FromJsonString(inJson);

            uint64_t startTime = GetCurrentTimeInMicroSeconds();
            processor.Process(eventGroup);
            durationTime += GetCurrentTimeInMicroSeconds() - startTime;

            // std::string outJson = eventGroup.ToJsonString();
            // std::cout << "outJson: " << outJson << std::endl;
        }
        std::cout << "durationTime: " << durationTime << std::endl;
        std::cout << "process: "
                  << formatSize((data1.size() + data2.size() + data3.size() + data4.size() + 7 * 4) * (uint64_t)count
                                * 1000000 * (uint64_t)size / durationTime)
                  << std::endl;
    }
}

static void BM_ContainerdText(int size, int batchSize) {
    logtail::Logger::Instance().InitGlobalLoggers();

    PipelineContext mContext;
    mContext.SetConfigName("project##config_0");

    Json::Value config;
    config["IgnoringStdout"] = false;
    config["IgnoringStderr"] = false;
    ProcessorParseContainerLogNative processor;
    processor.SetContext(mContext);
    processor.SetMetricsRecordRef(ProcessorParseContainerLogNative::sName, "1");

    std::string data1
        = R"(2024-04-08T12:48:59.665663286+08:00 stdout P Exception in thread "main" java.lang.NullPointerExceptionat  com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat )";
    std::string data2
        = R"(2024-04-08T12:48:59.665663286+08:00 stdout P com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat)";

    std::string data3
        = R"(2024-04-08T12:48:59.66566455+08:00 stdout P com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitleat com.example.myproject.Book.getTitle)";
    std::string data4 = R"(2024-04-08T12:48:59.665665738+08:00 stdout F     at com.example.myproject.Book.getTitle)";
    std::cout << "log size:\t" << formatSize((data1.size() + data2.size() + data3.size() + data4.size() + 7 * 4) * size)
              << std::endl;

    // make events
    Json::Value root;
    Json::Value events;
    for (int i = 0; i < size; i++) {
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data1;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data2;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data3;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
        {
            Json::Value event;
            event["type"] = 1;
            event["timestamp"] = 1234567890;
            event["timestampNanosecond"] = 0;
            {
                Json::Value contents;
                contents["content"] = data4;
                event["contents"] = std::move(contents);
            }
            events.append(event);
        }
    }

    root["events"] = events;
    Json::StreamWriterBuilder builder;
    builder["commentStyle"] = "None";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    std::ostringstream oss;
    writer->write(root, &oss);
    std::string inJson = oss.str();

    bool init = processor.Init(config);
    if (init) {
        int count = 0;
        // Perform setup here
        uint64_t durationTime = 0;
        for (int i = 0; i < batchSize; i++) {
            count++;
            auto sourceBuffer = std::make_shared<SourceBuffer>();
            PipelineEventGroup eventGroup(sourceBuffer);
            eventGroup.SetMetadata(EventGroupMetaKey::LOG_FORMAT, ProcessorParseContainerLogNative::CONTAINERD_TEXT);
            eventGroup.FromJsonString(inJson);

            uint64_t startTime = GetCurrentTimeInMicroSeconds();
            processor.Process(eventGroup);
            durationTime += GetCurrentTimeInMicroSeconds() - startTime;

            // std::string outJson = eventGroup.ToJsonString();
            // std::cout << "outJson: " << outJson << std::endl;
        }
        std::cout << "durationTime: " << durationTime << std::endl;
        std::cout << "process: "
                  << formatSize((data1.size() + data2.size() + data3.size() + data4.size() + 7 * 4) * (uint64_t)count
                                * 1000000 * (uint64_t)size / durationTime)
                  << std::endl;
    }
}

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
#ifdef NDEBUG
    std::cout << "release" << std::endl;
#else
    std::cout << "debug" << std::endl;
#endif
    std::cout << "docker json" << std::endl;
    BM_DockerJson(512, 100);
    std::cout << "containerdText" << std::endl;
    BM_ContainerdText(512, 100);
    return 0;
}