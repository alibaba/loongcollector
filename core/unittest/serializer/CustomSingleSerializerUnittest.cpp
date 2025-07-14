/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cstdlib>
#include <rapidjson/document.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "checkpoint/RangeCheckpoint.h"
#include "collection_pipeline/serializer/CustomSingleSerializer.h"
#include "common/JsonUtil.h"
#include "constants/TagConstants.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class CustomSingleSerializerUnittest : public ::testing::Test {
public:
    void TestSerializeLogEvents();
    void TestSerializeMetricEvents();
    void TestSerializeSpanEvents();
    void TestSerializeRawEvents();
    void TestSerializeEmptyEventGroup();
    void TestHostTags();
    void TestSpecialFields();

protected:
    static void SetUpTestCase() { sFlusher = make_unique<FlusherMock>(); }

    void SetUp() override {
        mCtx.SetConfigName("test_config");
        sFlusher->SetContext(mCtx);
        sFlusher->CreateMetricsRecordRef(FlusherMock::sName, "1");
        sFlusher->CommitMetricsRecordRef();
    }

private:
    BatchedEvents CreateBatchedLogEvents(bool withTags = true, bool withSpecialFields = false);
    BatchedEvents CreateBatchedMetricEvents(bool withTags = true, bool withMultiValue = false);
    BatchedEvents CreateBatchedSpanEvents(bool withTags = true);
    BatchedEvents CreateBatchedRawEvents(bool withTags = true);
    BatchedEvents CreateBatchedEmptyEvents();

    void ValidateCustomSingleJSON(const string& jsonStr, size_t expectedCount);
    void ValidateLogEventJSON(const rapidjson::Value& logObj,
                              const string& expectedMethod = "GET",
                              const string& expectedStatus = "200");
    void ValidateMetricEventJSON(const rapidjson::Value& logObj,
                                 const string& expectedName,
                                 const std::map<string, double>& expectedValues);
    void ValidateSpanEventJSON(const rapidjson::Value& logObj, const SpanEvent* expectedSpan);
    void ValidateRawEventJSON(const rapidjson::Value& logObj, const string& expectedContent = "raw log content");

    static unique_ptr<FlusherMock> sFlusher;
    CollectionPipelineContext mCtx;
};

unique_ptr<FlusherMock> CustomSingleSerializerUnittest::sFlusher;

void CustomSingleSerializerUnittest::TestSerializeLogEvents() {
    CustomSingleSerializer serializer(sFlusher.get());

    {
        string res;
        string errorMsg;
        auto batch = CreateBatchedLogEvents();
        APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_FALSE(res.empty());
        APSARA_TEST_EQUAL("", errorMsg);
        ValidateCustomSingleJSON(res, 1);
    }
    {
        string res;
        string errorMsg;
        auto batch = CreateBatchedLogEvents(true, true);
        APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_FALSE(res.empty());
        APSARA_TEST_EQUAL("", errorMsg);
        ValidateCustomSingleJSON(res, 1);
    }
}

void CustomSingleSerializerUnittest::TestSerializeMetricEvents() {
    CustomSingleSerializer serializer(sFlusher.get());
    {
        string res;
        string errorMsg;
        auto batch = CreateBatchedMetricEvents();
        APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_FALSE(res.empty());
        APSARA_TEST_EQUAL("", errorMsg);
        rapidjson::Document doc;
        doc.Parse(res.c_str());
        ValidateMetricEventJSON(doc, "test_metric", {{"__value__", 0.85}});
    }
    {
        string res;
        string errorMsg;
        auto batch = CreateBatchedMetricEvents(true, true);
        APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_FALSE(res.empty());
        APSARA_TEST_EQUAL("", errorMsg);
        rapidjson::Document doc;
        doc.Parse(res.c_str());
        ValidateMetricEventJSON(doc, "test_metric", {{"value1", 10.0}, {"value2", 20.0}});
    }
}

void CustomSingleSerializerUnittest::TestSerializeSpanEvents() {
    CustomSingleSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedSpanEvents();
    const auto& expectedSpan = batch.mEvents[0].Cast<SpanEvent>();

    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());
    APSARA_TEST_EQUAL("", errorMsg);
    rapidjson::Document doc;
    doc.Parse(res.c_str());
    ValidateSpanEventJSON(doc, &expectedSpan);
}

void CustomSingleSerializerUnittest::TestSerializeRawEvents() {
    CustomSingleSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedRawEvents();
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());
    APSARA_TEST_EQUAL("", errorMsg);
    ValidateCustomSingleJSON(res, 1);
}

void CustomSingleSerializerUnittest::TestSerializeEmptyEventGroup() {
    CustomSingleSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedEmptyEvents();
    APSARA_TEST_FALSE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_EQUAL("", res);
    APSARA_TEST_EQUAL("empty event group", errorMsg);
}

void CustomSingleSerializerUnittest::TestHostTags() {
    CustomSingleSerializer serializer(sFlusher.get());
    string res;
    string errorMsg;
    auto batch = CreateBatchedLogEvents();
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());
    APSARA_TEST_TRUE(doc.IsObject());
    APSARA_TEST_TRUE(doc.HasMember("tags"));
    const rapidjson::Value& tags = doc["tags"];
    APSARA_TEST_TRUE(tags.IsObject());
    APSARA_TEST_TRUE(tags.HasMember(DEFAULT_LOG_TAG_HOST_NAME.data()));
    APSARA_TEST_EQUAL(string("test-host"), string(tags[DEFAULT_LOG_TAG_HOST_NAME.data()].GetString()));
    APSARA_TEST_TRUE(tags.HasMember(LOG_RESERVED_KEY_SOURCE.data()));
    APSARA_TEST_EQUAL(string("test-source"), string(tags[LOG_RESERVED_KEY_SOURCE.data()].GetString()));
}

void CustomSingleSerializerUnittest::TestSpecialFields() {
    CustomSingleSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedLogEvents(true, true);
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];
    const rapidjson::Value& contents = doc["contents"];


    APSARA_TEST_TRUE(contents.HasMember("__log_topic__"));
    APSARA_TEST_EQUAL(string("test-topic"), string(contents["__log_topic__"].GetString()));
    APSARA_TEST_FALSE(tags.HasMember("log.topic"));


    APSARA_TEST_TRUE(tags.HasMember("__path__"));
    APSARA_TEST_EQUAL(string("/var/log/test.log"), string(tags["__path__"].GetString()));
    APSARA_TEST_FALSE(contents.HasMember("__tag__:__path__"));


    APSARA_TEST_FALSE(tags.HasMember("__user_defined_id__"));
}

BatchedEvents CustomSingleSerializerUnittest::CreateBatchedLogEvents(bool withTags, bool withSpecialFields) {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    if (withTags) {
        group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");
        group.SetTag(LOG_RESERVED_KEY_SOURCE, "test-source");
    }

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("GET"));
    e->SetContent(string("status"), string("200"));

    if (withSpecialFields) {
        e->SetContent(string("__tag__:__path__"), string("/var/log/test.log"));
        e->SetContent(string("__log_topic__"), string("test-topic"));
        e->SetContent(string("__tag__:__user_defined_id__"), string("should-be-ignored"));
    }

    e->SetTimestamp(1234567890);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents CustomSingleSerializerUnittest::CreateBatchedMetricEvents(bool withTags, bool withMultiValue) {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    if (withTags) {
        group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");
    }

    MetricEvent* e = group.AddMetricEvent();
    e->SetName("test_metric");
    e->SetTag(string("instance"), string("localhost"));
    e->SetTag(string("job"), string("test-job"));
    e->SetTimestamp(1234567890);

    if (withMultiValue) {
        e->SetValue<UntypedMultiDoubleValues>(e);
        auto* multiValue = e->MutableValue<UntypedMultiDoubleValues>();
        multiValue->SetValue(string("value1"), {UntypedValueMetricType::MetricTypeGauge, 10.0});
        multiValue->SetValue(string("value2"), {UntypedValueMetricType::MetricTypeCounter, 20.0});
    } else {
        e->SetValue<UntypedSingleValue>(0.85);
    }

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents CustomSingleSerializerUnittest::CreateBatchedSpanEvents(bool withTags) {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    if (withTags) {
        group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");
    }

    SpanEvent* e = group.AddSpanEvent();
    e->SetName("test-span");
    e->SetTraceId("trace-123");
    e->SetSpanId("span-456");
    e->SetParentSpanId("parent-789");
    e->SetTimestamp(1234567890);
    e->SetStartTimeNs(1000000000);
    e->SetEndTimeNs(2000000000);
    e->SetTag(string("service"), string("test-service"));
    e->SetScopeTag(string("scope"), string("test-scope"));

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents CustomSingleSerializerUnittest::CreateBatchedRawEvents(bool withTags) {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    if (withTags) {
        group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");
    }

    RawEvent* e = group.AddRawEvent();
    e->SetContent(string("raw log content"));
    e->SetTimestamp(1234567890);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents CustomSingleSerializerUnittest::CreateBatchedEmptyEvents() {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

void CustomSingleSerializerUnittest::ValidateCustomSingleJSON(const string& jsonStr, size_t expectedCount) {
    vector<string> lines;
    stringstream ss(jsonStr);
    string line;
    while (getline(ss, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    APSARA_TEST_EQUAL(expectedCount, lines.size());

    for (const string& jsonLine : lines) {
        rapidjson::Document doc;
        doc.Parse(jsonLine.c_str());
        APSARA_TEST_FALSE(doc.HasParseError());
        APSARA_TEST_TRUE(doc.IsObject());

        APSARA_TEST_TRUE(doc.HasMember("time"));
        APSARA_TEST_TRUE(doc.HasMember("contents"));
        APSARA_TEST_TRUE(doc.HasMember("tags"));

        APSARA_TEST_TRUE(doc["time"].IsUint());
        APSARA_TEST_TRUE(doc["contents"].IsObject());
        APSARA_TEST_TRUE(doc["tags"].IsObject());
    }
}

void CustomSingleSerializerUnittest::ValidateLogEventJSON(const rapidjson::Value& logObj,
                                                          const string& expectedMethod,
                                                          const string& expectedStatus) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_EQUAL(expectedMethod, string(contents["method"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("status"));
    APSARA_TEST_EQUAL(expectedStatus, string(contents["status"].GetString()));
}

void CustomSingleSerializerUnittest::ValidateMetricEventJSON(const rapidjson::Value& logObj,
                                                             const string& expectedName,
                                                             const std::map<string, double>& expectedValues) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember("__name__"));
    APSARA_TEST_EQUAL(expectedName, string(contents["__name__"].GetString()));

    size_t foundCount = 0;
    for (const auto& pair : expectedValues) {
        const string& key = pair.first;
        double expectedValue = pair.second;

        if (contents.HasMember(key.c_str())) {
            APSARA_TEST_TRUE(contents[key.c_str()].IsString());
            double actualValue = stod(contents[key.c_str()].GetString());
            APSARA_TEST_EQUAL(expectedValue, actualValue);
            foundCount++;
        }
    }

    APSARA_TEST_EQUAL(expectedValues.size() + 1, contents.MemberCount());
    APSARA_TEST_EQUAL(expectedValues.size(), foundCount);
}

void CustomSingleSerializerUnittest::ValidateSpanEventJSON(const rapidjson::Value& logObj,
                                                           const SpanEvent* expectedSpan) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember("trace_id"));
    APSARA_TEST_EQUAL(expectedSpan->GetTraceId().to_string(), string(contents["trace_id"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("span_id"));
    APSARA_TEST_EQUAL(expectedSpan->GetSpanId().to_string(), string(contents["span_id"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("parent_span_id"));
    APSARA_TEST_EQUAL(expectedSpan->GetParentSpanId().to_string(), string(contents["parent_span_id"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("name"));
    APSARA_TEST_EQUAL(expectedSpan->GetName().to_string(), string(contents["name"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("start_time"));
    APSARA_TEST_EQUAL(to_string(expectedSpan->GetStartTimeNs()), string(contents["start_time"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("end_time"));
    APSARA_TEST_EQUAL(to_string(expectedSpan->GetEndTimeNs()), string(contents["end_time"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("attributes"));
    APSARA_TEST_TRUE(contents["attributes"].IsObject());
    const auto& attributesDoc = contents["attributes"];
    for (auto it = expectedSpan->TagsBegin(); it != expectedSpan->TagsEnd(); ++it) {
        APSARA_TEST_TRUE(attributesDoc.HasMember(it->first.to_string().c_str()));
        APSARA_TEST_EQUAL(it->second.to_string(), string(attributesDoc[it->first.to_string().c_str()].GetString()));
    }

    APSARA_TEST_TRUE(contents.HasMember("scope"));
    APSARA_TEST_TRUE(contents["scope"].IsObject());
    const auto& scopeDoc = contents["scope"];
    for (auto it = expectedSpan->ScopeTagsBegin(); it != expectedSpan->ScopeTagsEnd(); ++it) {
        APSARA_TEST_TRUE(scopeDoc.HasMember(it->first.to_string().c_str()));
        APSARA_TEST_EQUAL(it->second.to_string(), string(scopeDoc[it->first.to_string().c_str()].GetString()));
    }
}

void CustomSingleSerializerUnittest::ValidateRawEventJSON(const rapidjson::Value& logObj,
                                                          const string& expectedContent) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember(DEFAULT_CONTENT_KEY.data()));
    APSARA_TEST_EQUAL(expectedContent, string(contents[DEFAULT_CONTENT_KEY.data()].GetString()));
}

UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSerializeLogEvents)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSerializeMetricEvents)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSerializeSpanEvents)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSerializeRawEvents)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSerializeEmptyEventGroup)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestHostTags)
UNIT_TEST_CASE(CustomSingleSerializerUnittest, TestSpecialFields)

} // namespace logtail

UNIT_TEST_MAIN
