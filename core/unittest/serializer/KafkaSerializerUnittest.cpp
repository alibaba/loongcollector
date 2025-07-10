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
#include "collection_pipeline/serializer/KafkaSerializer.h"
#include "common/JsonUtil.h"
#include "constants/TagConstants.h"
#include "models/PipelineEventGroup.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class KafkaSerializerUnittest : public ::testing::Test {
public:
    void TestSerializeLogEvents();
    void TestSerializeMetricEvents();
    void TestSerializeSpanEvents();
    void TestSerializeRawEvents();
    void TestSerializeEmptyEventGroup();
    void TestTagConversion();
    void TestK8sEnvironmentTagConversion();
    void TestSpecialFields();
    void TestHostEnvironmentConversion();
    void TestDockerEnvironmentConversion();
    void TestK8sDaemonsetEnvironmentConversion();
    void TestK8sSidecarEnvironmentConversion();
    void TestStdoutConversion();

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
    void ValidateTagConversion(const rapidjson::Value& tags, bool isK8sEnvironment = false);

    static unique_ptr<FlusherMock> sFlusher;
    CollectionPipelineContext mCtx;
};

unique_ptr<FlusherMock> KafkaSerializerUnittest::sFlusher;

void KafkaSerializerUnittest::TestSerializeLogEvents() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

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

void KafkaSerializerUnittest::TestSerializeMetricEvents() {
    KafkaEventGroupSerializer serializer(sFlusher.get());
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

void KafkaSerializerUnittest::TestSerializeSpanEvents() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

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

void KafkaSerializerUnittest::TestSerializeRawEvents() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedRawEvents();
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());
    APSARA_TEST_EQUAL("", errorMsg);
    ValidateCustomSingleJSON(res, 1);
}

void KafkaSerializerUnittest::TestSerializeEmptyEventGroup() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

    string res;
    string errorMsg;
    auto batch = CreateBatchedEmptyEvents();
    APSARA_TEST_FALSE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_EQUAL("", res);
    APSARA_TEST_EQUAL("empty event group", errorMsg);
}

void KafkaSerializerUnittest::TestTagConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());
    {
        PipelineEventGroup group(make_shared<SourceBuffer>());
        group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");
        group.SetTag(LOG_RESERVED_KEY_SOURCE, "test-source");

        LogEvent* e = group.AddLogEvent();
        e->SetContent(string("method"), string("GET"));
        e->SetContent(string("__tag__:__path__"), string("/var/log/test.log"));
        e->SetContent(string("__tag__:_container_name_"), string("test-container"));
        e->SetTimestamp(1234567890);

        BatchedEvents batch(std::move(group.MutableEvents()),
                            std::move(group.GetSizedTags()),
                            std::move(group.GetSourceBuffer()),
                            group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                            std::move(group.GetExactlyOnceCheckpoint()));

        string res;
        string errorMsg;
        APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
        APSARA_TEST_FALSE(res.empty());

        rapidjson::Document doc;
        doc.Parse(res.c_str());
        APSARA_TEST_FALSE(doc.HasParseError());
        APSARA_TEST_TRUE(doc.IsObject());

        APSARA_TEST_TRUE(doc.HasMember("tags"));
        const rapidjson::Value& tags = doc["tags"];
        APSARA_TEST_TRUE(tags.IsObject());

        APSARA_TEST_TRUE(tags.HasMember("log.file.path"));
        APSARA_TEST_EQUAL(string("/var/log/test.log"), string(tags["log.file.path"].GetString()));

        APSARA_TEST_TRUE(tags.HasMember("container.name"));
        APSARA_TEST_EQUAL(string("test-container"), string(tags["container.name"].GetString()));

        APSARA_TEST_TRUE(tags.HasMember("host.name"));
        APSARA_TEST_EQUAL(string("test-host"), string(tags["host.name"].GetString()));
    }
}

void KafkaSerializerUnittest::TestK8sEnvironmentTagConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "10.0.0.1");
#else
    setenv("KUBERNETES_SERVICE_HOST", "10.0.0.1", 1);
#endif

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("GET"));
    e->SetContent(string("__tag__:_container_name_"), string("test-container"));
    e->SetContent(string("__tag__:_container_ip_"), string("10.0.0.100"));
    e->SetContent(string("__tag__:_image_name_"), string("test-image"));
    e->SetTimestamp(1234567890);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];

    APSARA_TEST_TRUE(tags.HasMember("k8s.container.name"));
    APSARA_TEST_EQUAL(string("test-container"), string(tags["k8s.container.name"].GetString()));

    APSARA_TEST_TRUE(tags.HasMember("k8s.container.ip"));
    APSARA_TEST_EQUAL(string("10.0.0.100"), string(tags["k8s.container.ip"].GetString()));

    APSARA_TEST_TRUE(tags.HasMember("k8s.container.image.name"));
    APSARA_TEST_EQUAL(string("test-image"), string(tags["k8s.container.image.name"].GetString()));

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "");
#else
    unsetenv("KUBERNETES_SERVICE_HOST");
#endif
}

void KafkaSerializerUnittest::TestSpecialFields() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "test-host");

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("GET"));
    e->SetContent(string("__log_topic__"), string("test-topic"));
    e->SetContent(string("__tag__:__user_defined_id__"), string("should-be-ignored"));
    e->SetTimestamp(1234567890);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];
    const rapidjson::Value& contents = doc["contents"];

    APSARA_TEST_TRUE(tags.HasMember("log.topic"));
    APSARA_TEST_EQUAL(string("test-topic"), string(tags["log.topic"].GetString()));

    APSARA_TEST_FALSE(tags.HasMember("__user_defined_id__"));

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_FALSE(contents.HasMember("__log_topic__"));
    APSARA_TEST_FALSE(contents.HasMember("__tag__:__user_defined_id__"));
}

void KafkaSerializerUnittest::TestHostEnvironmentConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "alje834hgf");
    group.SetTag(string("__pack_id__"), string("AEDCFGHNJUIOPLMN-1E"));

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("PUT"));
    e->SetContent(string("status"), string("200"));
    e->SetContent(string("__tag__:__path__"), string("/root/test/origin/example.log"));
    e->SetContent(string("__log_topic__"), string("file"));
    e->SetTimestamp(1662434209);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];
    const rapidjson::Value& contents = doc["contents"];

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_EQUAL(string("PUT"), string(contents["method"].GetString()));
    APSARA_TEST_TRUE(contents.HasMember("status"));
    APSARA_TEST_EQUAL(string("200"), string(contents["status"].GetString()));

    APSARA_TEST_TRUE(tags.HasMember("log.file.path"));
    APSARA_TEST_EQUAL(string("/root/test/origin/example.log"), string(tags["log.file.path"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("host.name"));
    APSARA_TEST_EQUAL(string("alje834hgf"), string(tags["host.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("log.topic"));
    APSARA_TEST_EQUAL(string("file"), string(tags["log.topic"].GetString()));
}

void KafkaSerializerUnittest::TestDockerEnvironmentConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "");
#else
    unsetenv("KUBERNETES_SERVICE_HOST");
#endif

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "alje834hgf");
    group.SetTag(string("__pack_id__"), string("AEDCFGHNJUIOPLMN-1E"));

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("GET"));
    e->SetContent(string("status"), string("404"));
    e->SetContent(string("__tag__:__user_defined_id__"), string("machine"));
    e->SetContent(string("__tag__:__path__"), string("/root/test/origin/example.log"));
    e->SetContent(string("__tag__:_container_name_"), string("container"));
    e->SetContent(string("__tag__:_container_ip_"), string("172.10.0.45"));
    e->SetContent(string("__tag__:_image_name_"), string("image"));
    e->SetContent(string("__log_topic__"), string("file"));
    e->SetTimestamp(1662434487);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];
    const rapidjson::Value& contents = doc["contents"];

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_EQUAL(string("GET"), string(contents["method"].GetString()));
    APSARA_TEST_TRUE(contents.HasMember("status"));
    APSARA_TEST_EQUAL(string("404"), string(contents["status"].GetString()));

    APSARA_TEST_TRUE(tags.HasMember("log.file.path"));
    APSARA_TEST_TRUE(tags.HasMember("host.name"));
    APSARA_TEST_TRUE(tags.HasMember("log.topic"));
    APSARA_TEST_TRUE(tags.HasMember("container.name"));
    APSARA_TEST_EQUAL(string("container"), string(tags["container.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("container.ip"));
    APSARA_TEST_EQUAL(string("172.10.0.45"), string(tags["container.ip"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("container.image.name"));
    APSARA_TEST_EQUAL(string("image"), string(tags["container.image.name"].GetString()));
}

void KafkaSerializerUnittest::TestK8sDaemonsetEnvironmentConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "10.0.0.1");
#else
    setenv("KUBERNETES_SERVICE_HOST", "10.0.0.1", 1);
#endif

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "alje834hgf");
    group.SetTag(string("__pack_id__"), string("AEDCFGHNJUIOPLMN-1E"));

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("PUT"));
    e->SetContent(string("status"), string("200"));
    e->SetContent(string("__tag__:__user_defined_id__"), string("machine"));
    e->SetContent(string("__tag__:__path__"), string("/root/test/origin/example.log"));
    e->SetContent(string("__tag__:_node_name_"), string("node"));
    e->SetContent(string("__tag__:_node_ip_"), string("172.10.1.19"));
    e->SetContent(string("__tag__:_namespace_"), string("default"));
    e->SetContent(string("__tag__:_pod_name_"), string("container"));
    e->SetContent(string("__tag__:_pod_uid_"), string("12AFERR234SG-SBH6D67HJ9-AAD-VF34"));
    e->SetContent(string("__tag__:_container_name_"), string("container"));
    e->SetContent(string("__tag__:_container_ip_"), string("172.10.0.45"));
    e->SetContent(string("__tag__:_image_name_"), string("image"));
    e->SetContent(string("__tag__:label"), string("tag"));
    e->SetContent(string("__log_topic__"), string("file"));
    e->SetTimestamp(1662434209);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];


    APSARA_TEST_TRUE(tags.HasMember("log.file.path"));
    APSARA_TEST_TRUE(tags.HasMember("host.name"));
    APSARA_TEST_TRUE(tags.HasMember("log.topic"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.ip"));
    APSARA_TEST_EQUAL(string("172.10.1.19"), string(tags["k8s.node.ip"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.name"));
    APSARA_TEST_EQUAL(string("node"), string(tags["k8s.node.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.namespace.name"));
    APSARA_TEST_EQUAL(string("default"), string(tags["k8s.namespace.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.name"));
    APSARA_TEST_EQUAL(string("container"), string(tags["k8s.pod.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.uid"));
    APSARA_TEST_EQUAL(string("12AFERR234SG-SBH6D67HJ9-AAD-VF34"), string(tags["k8s.pod.uid"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.name"));
    APSARA_TEST_EQUAL(string("container"), string(tags["k8s.container.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.ip"));
    APSARA_TEST_EQUAL(string("172.10.0.45"), string(tags["k8s.container.ip"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.image.name"));
    APSARA_TEST_EQUAL(string("image"), string(tags["k8s.container.image.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("label"));
    APSARA_TEST_EQUAL(string("tag"), string(tags["label"].GetString()));

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "");
#else
    unsetenv("KUBERNETES_SERVICE_HOST");
#endif
}

void KafkaSerializerUnittest::TestK8sSidecarEnvironmentConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "");
#else
    unsetenv("KUBERNETES_SERVICE_HOST");
#endif

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "alje834hgf");
    group.SetTag(string("__pack_id__"), string("AEDCFGHNJUIOPLMN-1E"));

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("GET"));
    e->SetContent(string("status"), string("404"));
    e->SetContent(string("__tag__:__user_defined_id__"), string("machine"));
    e->SetContent(string("__tag__:__path__"), string("/root/test/origin/example.log"));
    e->SetContent(string("__tag__:_node_name_"), string("node"));
    e->SetContent(string("__tag__:_node_ip_"), string("172.10.1.19"));
    e->SetContent(string("__tag__:_namespace_"), string("default"));
    e->SetContent(string("__tag__:_pod_name_"), string("container"));
    e->SetContent(string("__tag__:_pod_ip_"), string("172.10.0.45"));
    e->SetContent(string("__log_topic__"), string("file"));
    e->SetTimestamp(1662434487);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];

    APSARA_TEST_TRUE(tags.HasMember("log.file.path"));
    APSARA_TEST_TRUE(tags.HasMember("host.name"));
    APSARA_TEST_TRUE(tags.HasMember("log.topic"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.ip"));
    APSARA_TEST_EQUAL(string("172.10.1.19"), string(tags["k8s.node.ip"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.name"));
    APSARA_TEST_EQUAL(string("node"), string(tags["k8s.node.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.namespace.name"));
    APSARA_TEST_EQUAL(string("default"), string(tags["k8s.namespace.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.name"));
    APSARA_TEST_EQUAL(string("container"), string(tags["k8s.pod.name"].GetString()));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.ip"));
    APSARA_TEST_EQUAL(string("172.10.0.45"), string(tags["k8s.pod.ip"].GetString()));
}

void KafkaSerializerUnittest::TestStdoutConversion() {
    KafkaEventGroupSerializer serializer(sFlusher.get());

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "10.0.0.1");
#else
    setenv("KUBERNETES_SERVICE_HOST", "10.0.0.1", 1);
#endif

    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(DEFAULT_LOG_TAG_HOST_NAME, "alje834hgf");
    group.SetTag(string("__pack_id__"), string("AEDCFGHNJUIOPLMN-1E"));

    LogEvent* e = group.AddLogEvent();
    e->SetContent(string("method"), string("PUT"));
    e->SetContent(string("status"), string("200"));
    e->SetContent(string("label"), string("tag"));
    e->SetContent(string("__tag__:_node_name_"), string("node"));
    e->SetContent(string("__tag__:_node_ip_"), string("172.10.1.19"));
    e->SetContent(string("_namespace_"), string("default"));
    e->SetContent(string("_pod_name_"), string("container"));
    e->SetContent(string("_pod_uid_"), string("12AFERR234SG-SBH6D67HJ9-AAD-VF34"));
    e->SetContent(string("_container_name_"), string("container"));
    e->SetContent(string("_container_ip_"), string("172.10.0.45"));
    e->SetContent(string("_image_name_"), string("image"));
    e->SetTimestamp(1662434209);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));

    string res;
    string errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    APSARA_TEST_FALSE(res.empty());

    rapidjson::Document doc;
    doc.Parse(res.c_str());
    APSARA_TEST_FALSE(doc.HasParseError());

    const rapidjson::Value& tags = doc["tags"];
    const rapidjson::Value& contents = doc["contents"];

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_EQUAL(string("PUT"), string(contents["method"].GetString()));
    APSARA_TEST_TRUE(contents.HasMember("status"));
    APSARA_TEST_EQUAL(string("200"), string(contents["status"].GetString()));
    APSARA_TEST_TRUE(contents.HasMember("label"));
    APSARA_TEST_EQUAL(string("tag"), string(contents["label"].GetString()));

    APSARA_TEST_TRUE(tags.HasMember("host.name"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.ip"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.node.name"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.namespace.name"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.name"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.pod.uid"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.name"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.ip"));
    APSARA_TEST_TRUE(tags.HasMember("k8s.container.image.name"));

#ifdef _WIN32
    _putenv_s("KUBERNETES_SERVICE_HOST", "");
#else
    unsetenv("KUBERNETES_SERVICE_HOST");
#endif
}

BatchedEvents KafkaSerializerUnittest::CreateBatchedLogEvents(bool withTags, bool withSpecialFields) {
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
    }

    e->SetTimestamp(1234567890);

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents KafkaSerializerUnittest::CreateBatchedMetricEvents(bool withTags, bool withMultiValue) {
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

BatchedEvents KafkaSerializerUnittest::CreateBatchedSpanEvents(bool withTags) {
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

BatchedEvents KafkaSerializerUnittest::CreateBatchedRawEvents(bool withTags) {
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

BatchedEvents KafkaSerializerUnittest::CreateBatchedEmptyEvents() {
    PipelineEventGroup group(make_shared<SourceBuffer>());

    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

void KafkaSerializerUnittest::ValidateCustomSingleJSON(const string& jsonStr, size_t expectedCount) {
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

void KafkaSerializerUnittest::ValidateLogEventJSON(const rapidjson::Value& logObj,
                                                   const string& expectedMethod,
                                                   const string& expectedStatus) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember("method"));
    APSARA_TEST_EQUAL(expectedMethod, string(contents["method"].GetString()));

    APSARA_TEST_TRUE(contents.HasMember("status"));
    APSARA_TEST_EQUAL(expectedStatus, string(contents["status"].GetString()));
}

void KafkaSerializerUnittest::ValidateMetricEventJSON(const rapidjson::Value& logObj,
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

void KafkaSerializerUnittest::ValidateSpanEventJSON(const rapidjson::Value& logObj, const SpanEvent* expectedSpan) {
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
    APSARA_TEST_TRUE(contents["attributes"].IsString());
    rapidjson::Document attributesDoc;
    attributesDoc.Parse(contents["attributes"].GetString());
    APSARA_TEST_FALSE(attributesDoc.HasParseError());
    for (auto it = expectedSpan->TagsBegin(); it != expectedSpan->TagsEnd(); ++it) {
        APSARA_TEST_TRUE(attributesDoc.HasMember(it->first.to_string().c_str()));
        APSARA_TEST_EQUAL(it->second.to_string(), string(attributesDoc[it->first.to_string().c_str()].GetString()));
    }

    APSARA_TEST_TRUE(contents.HasMember("scope"));
    APSARA_TEST_TRUE(contents["scope"].IsString());
    rapidjson::Document scopeDoc;
    scopeDoc.Parse(contents["scope"].GetString());
    APSARA_TEST_FALSE(scopeDoc.HasParseError());
    for (auto it = expectedSpan->ScopeTagsBegin(); it != expectedSpan->ScopeTagsEnd(); ++it) {
        APSARA_TEST_TRUE(scopeDoc.HasMember(it->first.to_string().c_str()));
        APSARA_TEST_EQUAL(it->second.to_string(), string(scopeDoc[it->first.to_string().c_str()].GetString()));
    }
}

void KafkaSerializerUnittest::ValidateRawEventJSON(const rapidjson::Value& logObj, const string& expectedContent) {
    APSARA_TEST_TRUE(logObj.HasMember("contents"));
    const rapidjson::Value& contents = logObj["contents"];

    APSARA_TEST_TRUE(contents.HasMember("content"));
    APSARA_TEST_EQUAL(expectedContent, string(contents["content"].GetString()));
}

void KafkaSerializerUnittest::ValidateTagConversion(const rapidjson::Value& tags, bool isK8sEnvironment) {
    if (tags.HasMember("host.name")) {
        APSARA_TEST_TRUE(tags["host.name"].IsString());
    }

    if (isK8sEnvironment) {
        if (tags.HasMember("k8s.container.name")) {
            APSARA_TEST_TRUE(tags["k8s.container.name"].IsString());
        }
    } else {
        if (tags.HasMember("container.name")) {
            APSARA_TEST_TRUE(tags["container.name"].IsString());
        }
    }
}

UNIT_TEST_CASE(KafkaSerializerUnittest, TestSerializeLogEvents)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestSerializeMetricEvents)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestSerializeSpanEvents)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestSerializeRawEvents)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestSerializeEmptyEventGroup)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestTagConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestK8sEnvironmentTagConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestSpecialFields)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestHostEnvironmentConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestDockerEnvironmentConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestK8sDaemonsetEnvironmentConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestK8sSidecarEnvironmentConversion)
UNIT_TEST_CASE(KafkaSerializerUnittest, TestStdoutConversion)

} // namespace logtail

UNIT_TEST_MAIN
