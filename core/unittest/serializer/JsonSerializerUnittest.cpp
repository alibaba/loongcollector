// Copyright 2025 iLogtail Authors
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


#include "collection_pipeline/serializer/JsonSerializer.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

class JsonSerializerUnittest : public ::testing::Test {
public:
    void TestSerializeEventGroup();

protected:
    static void SetUpTestCase() { sFlusher = make_unique<FlusherMock>(); }

    void SetUp() override {
        mCtx.SetConfigName("test_config");
        sFlusher->SetContext(mCtx);
        sFlusher->CreateMetricsRecordRef(FlusherMock::sName, "1");
        sFlusher->CommitMetricsRecordRef();
    }

private:
    BatchedEvents
    createBatchedLogEvents(bool enableNanosecond, bool withEmptyContent = false, bool withNonEmptyContent = true);
    BatchedEvents createBatchedMetricEvents(
        bool enableNanosecond, uint32_t nanoTimestamp, bool emptyValue, bool onlyOneTag, bool multiValue = false);
    BatchedEvents
    createBatchedRawEvents(bool enableNanosecond, bool withEmptyContent = false, bool withNonEmptyContent = true);
    BatchedEvents createBatchedSpanEvents();

    static unique_ptr<FlusherMock> sFlusher;

    CollectionPipelineContext mCtx;
};

unique_ptr<FlusherMock> JsonSerializerUnittest::sFlusher;

void JsonSerializerUnittest::TestSerializeEventGroup() {
    JsonEventGroupSerializer serializer(sFlusher.get());
    { // log
        { // nano second disabled, and set
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedLogEvents(false), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"key\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // nano second enabled, and set
          // Todo
        }
        { // nano second enabled, not set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedLogEvents(false), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"key\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // with empty event
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedLogEvents(false, true, true), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"key\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // only empty event
            string res;
            string errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(createBatchedLogEvents(false, true, false), res, errorMsg));
            APSARA_TEST_EQUAL("", res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
    }
    { // metric
        { // only 1 tag
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedMetricEvents(false, 0, false, true), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"__labels__\":{\"key1\":"
                              "\"value1\"},\"__name__\":\"test_gauge\",\"__value__\":0.1}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // multi value
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(
                serializer.DoSerialize(createBatchedMetricEvents(false, 0, false, false, true), res, errorMsg));
            APSARA_TEST_EQUAL(
                "{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":\"source\",\"__"
                "topic__\":\"topic\",\"__time__\":1234567890,\"__labels__\":{\"key1\":\"value1\",\"key2\":\"value2\"},"
                "\"__name__\":\"test_gauge\",\"__value__\":{\"test-1\":10.0,\"test-2\":2.0}}\n",
                res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // nano second disabled
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedMetricEvents(false, 0, false, false), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"__labels__\":{\"key1\":"
                              "\"value1\",\"key2\":\"value2\"},\"__name__\":\"test_gauge\",\"__value__\":0.1}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // nano second enabled
          // Todo
        }
        { // empty metric value
            string res;
            string errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(createBatchedMetricEvents(false, 0, true, false), res, errorMsg));
            APSARA_TEST_EQUAL("", res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
    }
    { // span
        string res;
        string errorMsg;
        auto events = createBatchedSpanEvents();
        APSARA_TEST_EQUAL(events.mEvents.size(), 1U);
        APSARA_TEST_TRUE(events.mEvents[0]->GetType() == PipelineEvent::Type::SPAN);
        APSARA_TEST_FALSE(serializer.DoSerialize(std::move(events), res, errorMsg));
        APSARA_TEST_EQUAL("", res);
        APSARA_TEST_EQUAL("invalid event type, span type is not yet supported", errorMsg);
    }
    { // raw
        { // nano second disabled, and set
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedRawEvents(false), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"content\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // nano second enabled, and set
          // Todo
        }
        { // nano second enabled, not set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedRawEvents(false), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"content\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // with empty event
            string res;
            string errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(createBatchedRawEvents(false, true, true), res, errorMsg));
            APSARA_TEST_EQUAL("{\"__machine_uuid__\":\"machine_uuid\",\"__pack_id__\":\"pack_id\",\"__source__\":"
                              "\"source\",\"__topic__\":\"topic\",\"__time__\":1234567890,\"content\":\"value\"}\n",
                              res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
        { // only empty event
            string res;
            string errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(createBatchedRawEvents(false, true, false), res, errorMsg));
            APSARA_TEST_EQUAL("", res);
            APSARA_TEST_EQUAL("", errorMsg);
        }
    }
    { // empty log group
        PipelineEventGroup group(make_shared<SourceBuffer>());
        BatchedEvents batch(std::move(group.MutableEvents()),
                            std::move(group.GetSizedTags()),
                            std::move(group.GetSourceBuffer()),
                            group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                            std::move(group.GetExactlyOnceCheckpoint()));
        string res;
        string errorMsg;
        APSARA_TEST_FALSE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_EQUAL("", res);
        APSARA_TEST_EQUAL("empty event group", errorMsg);
    }
}


BatchedEvents
JsonSerializerUnittest::createBatchedLogEvents(bool enableNanosecond, bool withEmptyContent, bool withNonEmptyContent) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "machine_uuid");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "pack_id");
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    if (withNonEmptyContent) {
        LogEvent* e = group.AddLogEvent();
        e->SetContent(string("key"), string("value"));
        if (enableNanosecond) {
            e->SetTimestamp(1234567890, 1);
        } else {
            e->SetTimestamp(1234567890);
        }
    }
    if (withEmptyContent) {
        LogEvent* e = group.AddLogEvent();
        if (enableNanosecond) {
            e->SetTimestamp(1234567890, 1);
        } else {
            e->SetTimestamp(1234567890);
        }
    }
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents JsonSerializerUnittest::createBatchedMetricEvents(
    bool enableNanosecond, uint32_t nanoTimestamp, bool emptyValue, bool onlyOneTag, bool multiValue) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "machine_uuid");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "pack_id");

    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    MetricEvent* e = group.AddMetricEvent();
    e->SetTag(string("key1"), string("value1"));
    if (!onlyOneTag) {
        e->SetTag(string("key2"), string("value2"));
    }
    if (enableNanosecond) {
        e->SetTimestamp(1234567890, nanoTimestamp);
    } else {
        e->SetTimestamp(1234567890);
    }

    if (!emptyValue) {
        if (!multiValue) {
            double value = 0.1;
            e->SetValue<UntypedSingleValue>(value);
        } else {
            UntypedMultiDoubleValues v({{"test-1", {UntypedValueMetricType::MetricTypeCounter, 10.0}},
                                        {"test-2", {UntypedValueMetricType::MetricTypeGauge, 2.0}}},
                                       nullptr);
            e->SetValue(v);
        }
    }
    e->SetName("test_gauge");
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents
JsonSerializerUnittest::createBatchedRawEvents(bool enableNanosecond, bool withEmptyContent, bool withNonEmptyContent) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "machine_uuid");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "pack_id");
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    if (withNonEmptyContent) {
        RawEvent* e = group.AddRawEvent();
        e->SetContent(string("value"));
        if (enableNanosecond) {
            e->SetTimestamp(1234567890, 1);
        } else {
            e->SetTimestamp(1234567890);
        }
    }
    if (withEmptyContent) {
        RawEvent* e = group.AddRawEvent();
        e->SetContent(string(""));
        if (enableNanosecond) {
            e->SetTimestamp(1234567890, 1);
        } else {
            e->SetTimestamp(1234567890);
        }
    }
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents JsonSerializerUnittest::createBatchedSpanEvents() {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "aaa");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "bbb");
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    // auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    SpanEvent* spanEvent = group.AddSpanEvent();
    spanEvent->SetScopeTag(std::string("scope-tag-0"), std::string("scope-value-0"));
    spanEvent->SetTag(std::string("workloadName"), std::string("arms-oneagent-test-ql"));
    spanEvent->SetTag(std::string("workloadKind"), std::string("faceless"));
    spanEvent->SetTag(std::string("source_ip"), std::string("10.54.0.33"));
    spanEvent->SetTag(std::string("host"), std::string("10.54.0.33"));
    spanEvent->SetTag(std::string("rpc"), std::string("/oneagent/qianlu/local/1"));
    spanEvent->SetTag(std::string("rpcType"), std::string("25"));
    spanEvent->SetTag(std::string("callType"), std::string("http-client"));
    spanEvent->SetTag(std::string("statusCode"), std::string("200"));
    spanEvent->SetTag(std::string("version"), std::string("HTTP1.1"));
    auto innerEvent = spanEvent->AddEvent();
    innerEvent->SetTag(std::string("innner-event-key-0"), std::string("inner-event-value-0"));
    innerEvent->SetTag(std::string("innner-event-key-1"), std::string("inner-event-value-1"));
    innerEvent->SetName("inner-event");
    innerEvent->SetTimestampNs(1000);
    auto innerLink = spanEvent->AddLink();
    innerLink->SetTag(std::string("innner-link-key-0"), std::string("inner-link-value-0"));
    innerLink->SetTag(std::string("innner-link-key-1"), std::string("inner-link-value-1"));
    innerLink->SetTraceId("inner-link-traceid");
    innerLink->SetSpanId("inner-link-spanid");
    innerLink->SetTraceState("inner-link-trace-state");
    spanEvent->SetName("/oneagent/qianlu/local/1");
    spanEvent->SetKind(SpanEvent::Kind::Client);
    spanEvent->SetStatus(SpanEvent::StatusCode::Ok);
    spanEvent->SetSpanId("span-1-2-3-4-5");
    spanEvent->SetTraceId("trace-1-2-3-4-5");
    spanEvent->SetParentSpanId("parent-1-2-3-4-5");
    spanEvent->SetTraceState("test-state");
    spanEvent->SetStartTimeNs(1000);
    spanEvent->SetEndTimeNs(2000);
    spanEvent->SetTimestamp(seconds);
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

UNIT_TEST_CASE(JsonSerializerUnittest, TestSerializeEventGroup)

} // namespace logtail

UNIT_TEST_MAIN
