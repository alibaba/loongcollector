/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstring>
#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>

#include "protobuf/forward/loongsuite.grpc.pb.h"
#include "protobuf/models/pipeline_event_group.pb.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class LoongSuiteGrpcClientUnittest : public testing::Test {
public:
    void TestSendLogEventData();
    void TestSendMetricEventData();
    void TestSendSpanEventData();

    // 用于实际发送数据的工具方法
    void RunGrpcClientTool(const std::string& server_address = "0.0.0.0:7899",
                           const std::string& data_type = "log",
                           const std::string& config_name = "",
                           int count = 1);

protected:
    void SetUp() override {
        // 初始化
    }

    void TearDown() override {
        // 清理
    }

private:
    // gRPC客户端相关
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<LoongSuiteForwardService::Stub> stub_;

    // 辅助方法
    bool CreateGrpcClient(const std::string& server_address);
    void GenerateLogEventData(models::PipelineEventGroup& eventGroup);
    void GenerateMetricEventData(models::PipelineEventGroup& eventGroup);
    void GenerateSpanEventData(models::PipelineEventGroup& eventGroup);
    bool SendPipelineEventGroup(const models::PipelineEventGroup& eventGroup, const std::string& configName = "");
    bool SendRawData(const std::string& data, const std::string& configName = "");
    void AddCommonTagsAndMetadata(models::PipelineEventGroup& eventGroup);
};

bool LoongSuiteGrpcClientUnittest::CreateGrpcClient(const std::string& server_address) {
    try {
        // 创建不安全的gRPC连接 (适用于测试)
        channel_ = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        if (!channel_) {
            LOG_ERROR(sLogger, ("Failed to create gRPC channel to", server_address));
            return false;
        }

        // 创建stub
        stub_ = LoongSuiteForwardService::NewStub(channel_);
        if (!stub_) {
            LOG_ERROR(sLogger, ("Failed to create gRPC stub", ""));
            return false;
        }

        LOG_INFO(sLogger, ("gRPC client created successfully for", server_address));
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("Exception creating gRPC channel", e.what()));
        return false;
    }
}

void LoongSuiteGrpcClientUnittest::AddCommonTagsAndMetadata(models::PipelineEventGroup& eventGroup) {
    // 添加公共tags
    (*eventGroup.mutable_tags())["service.name"] = "grpc-client-unittest";
    (*eventGroup.mutable_tags())["service.version"] = "1.0.0";
    (*eventGroup.mutable_tags())["environment"] = "test";
    (*eventGroup.mutable_tags())["host.name"] = "unittest-host";

    // 添加metadata
    (*eventGroup.mutable_metadata())["source"] = "unittest";
    (*eventGroup.mutable_metadata())["timestamp"] = std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

void LoongSuiteGrpcClientUnittest::GenerateLogEventData(models::PipelineEventGroup& eventGroup) {
    // 清空现有events
    eventGroup.clear_logs();
    eventGroup.clear_metrics();
    eventGroup.clear_spans();

    // 创建LogEvents
    auto* logEvents = eventGroup.mutable_logs();

    // 添加log event
    auto* logEvent = logEvents->add_events();
    logEvent->set_timestamp(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    logEvent->set_level("INFO");

    auto* content1 = logEvent->add_contents();
    content1->set_key("message");
    content1->set_value("Test log message from gRPC client unittest");

    auto* content2 = logEvent->add_contents();
    content2->set_key("thread");
    content2->set_value("unittest");

    AddCommonTagsAndMetadata(eventGroup);

    LOG_INFO(sLogger, ("Generated LogEvent data with events", logEvents->events_size()));
}

void LoongSuiteGrpcClientUnittest::GenerateMetricEventData(models::PipelineEventGroup& eventGroup) {
    // 清空现有events
    eventGroup.clear_logs();
    eventGroup.clear_metrics();
    eventGroup.clear_spans();

    // 创建MetricEvents
    auto* metricEvents = eventGroup.mutable_metrics();

    // 添加metric event
    auto* metricEvent = metricEvents->add_events();
    metricEvent->set_timestamp(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    metricEvent->set_name("test.metric");
    metricEvent->mutable_untypedsinglevalue()->set_value(42.0);

    (*metricEvent->mutable_tags())["host"] = "unittest-host";
    (*metricEvent->mutable_tags())["component"] = "grpc-client";

    AddCommonTagsAndMetadata(eventGroup);

    LOG_INFO(sLogger, ("Generated MetricEvent data with events", metricEvents->events_size()));
}

void LoongSuiteGrpcClientUnittest::GenerateSpanEventData(models::PipelineEventGroup& eventGroup) {
    // 清空现有events
    eventGroup.clear_logs();
    eventGroup.clear_metrics();
    eventGroup.clear_spans();

    // 创建SpanEvents
    auto* spanEvents = eventGroup.mutable_spans();

    // 添加span event
    auto* spanEvent = spanEvents->add_events();
    spanEvent->set_traceid("unittest1234567890abcdef1234567890ab");
    spanEvent->set_spanid("unittest12345678");
    spanEvent->set_parentspanid("parent1234567890");
    spanEvent->set_name("/unittest/api");
    spanEvent->set_kind(models::SpanEvent::SERVER);
    spanEvent->set_timestamp(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
    spanEvent->set_starttime(1748313840259486017ULL);
    spanEvent->set_endtime(1748313840259765375ULL);
    spanEvent->set_status(models::SpanEvent::Ok);

    (*spanEvent->mutable_tags())["http.method"] = "POST";
    (*spanEvent->mutable_tags())["http.path"] = "/unittest/api";
    (*spanEvent->mutable_tags())["http.status_code"] = "200";
    (*spanEvent->mutable_tags())["service.name"] = "unittest-service";

    AddCommonTagsAndMetadata(eventGroup);

    LOG_INFO(sLogger, ("Generated SpanEvent data with events", spanEvents->events_size()));
}

bool LoongSuiteGrpcClientUnittest::SendPipelineEventGroup(const models::PipelineEventGroup& eventGroup,
                                                          const std::string& configName) {
    if (!stub_) {
        LOG_ERROR(sLogger, ("gRPC stub not initialized", ""));
        return false;
    }

    try {
        // 序列化PipelineEventGroup
        std::string serializedData;
        if (!eventGroup.SerializeToString(&serializedData)) {
            LOG_ERROR(sLogger, ("Failed to serialize PipelineEventGroup", ""));
            return false;
        }

        LOG_INFO(sLogger, ("Serialized data size", serializedData.size()));

        // 发送数据
        return SendRawData(serializedData, configName);

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("Exception in SendPipelineEventGroup", e.what()));
        return false;
    }
}

bool LoongSuiteGrpcClientUnittest::SendRawData(const std::string& data, const std::string& configName) {
    if (!stub_) {
        LOG_ERROR(sLogger, ("gRPC stub not initialized", ""));
        return false;
    }

    try {
        // 创建请求
        LoongSuiteForwardRequest request;
        request.set_data(data);
        // 创建响应
        LoongSuiteForwardResponse response;

        // 创建上下文
        grpc::ClientContext context;
        if (!configName.empty()) {
            context.AddMetadata("x-loongsuite-apm-configname", configName);
        }

        // 设置超时时间
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(30);
        context.set_deadline(deadline);

        LOG_INFO(sLogger, ("Sending data, size", data.size())("config", configName));

        // 发送请求
        grpc::Status status = stub_->Forward(&context, request, &response);

        if (status.ok()) {
            LOG_INFO(sLogger, ("Data sent successfully", ""));
            return true;
        } else {
            LOG_ERROR(sLogger, ("gRPC call failed", status.error_code())("message", status.error_message()));
            if (!status.error_details().empty()) {
                LOG_ERROR(sLogger, ("Error details", status.error_details()));
            }
            return false;
        }

    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("Exception in SendRawData", e.what()));
        return false;
    }
}

void LoongSuiteGrpcClientUnittest::RunGrpcClientTool(const std::string& server_address,
                                                     const std::string& data_type,
                                                     const std::string& config_name,
                                                     int count) {
    LOG_INFO(sLogger, ("=== LoongSuite gRPC Client Tool ===", ""));
    LOG_INFO(sLogger, ("Server", server_address));
    if (!config_name.empty()) {
        LOG_INFO(sLogger, ("Config", config_name));
    }

    // 创建gRPC客户端
    if (!CreateGrpcClient(server_address)) {
        LOG_ERROR(sLogger, ("Failed to connect to gRPC server", server_address));
        return;
    }

    LOG_INFO(sLogger, ("Successfully connected to gRPC server", ""));

    // 处理数据发送
    bool success = true;

    for (int i = 0; i < count; ++i) {
        if (count > 1) {
            LOG_INFO(sLogger, ("Sending batch", i + 1)("total", count));
        }

        models::PipelineEventGroup eventGroup;

        if (data_type == "log") {
            GenerateLogEventData(eventGroup);
        } else if (data_type == "metric") {
            GenerateMetricEventData(eventGroup);
        } else if (data_type == "span") {
            GenerateSpanEventData(eventGroup);
        } else {
            LOG_ERROR(sLogger, ("Invalid data type", data_type));
            return;
        }

        bool send_result = SendPipelineEventGroup(eventGroup, config_name);
        if (!send_result) {
            success = false;
            break;
        }

        // 如果有多次发送，添加小延迟
        if (count > 1 && i < count - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    if (success) {
        LOG_INFO(sLogger, ("All data sent successfully", ""));
        if (count > 1) {
            LOG_INFO(sLogger, ("Total batches sent", count));
        }
    } else {
        LOG_ERROR(sLogger, ("Some data failed to send", ""));
    }
}

void LoongSuiteGrpcClientUnittest::TestSendLogEventData() {
    LOG_INFO(sLogger, ("Running TestSendLogEventData", ""));
    RunGrpcClientTool("0.0.0.0:7899", "log", "unittest-log", 10);
}

void LoongSuiteGrpcClientUnittest::TestSendMetricEventData() {
    LOG_INFO(sLogger, ("Running TestSendMetricEventData", ""));
    RunGrpcClientTool("0.0.0.0:7899", "metric", "unittest-metric", 10);
}

void LoongSuiteGrpcClientUnittest::TestSendSpanEventData() {
    LOG_INFO(sLogger, ("Running TestSendSpanEventData", ""));
    RunGrpcClientTool("0.0.0.0:7899", "span", "unittest-span", 10);
}

UNIT_TEST_CASE(LoongSuiteGrpcClientUnittest, TestSendLogEventData)
UNIT_TEST_CASE(LoongSuiteGrpcClientUnittest, TestSendMetricEventData)
UNIT_TEST_CASE(LoongSuiteGrpcClientUnittest, TestSendSpanEventData)

} // namespace logtail

UNIT_TEST_MAIN
