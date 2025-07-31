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

 #include "gtest/gtest.h"
#include "models/PipelineEventGroup.h"
 #include "plugin/processor/inner/ProcessorParseFromPBNative.h"
 #include "protobuf/models/ProtocolConversion.h"
 #include "protobuf/models/pipeline_event_group.pb.h"
 #include "protobuf/models/span_event.pb.h"
 #include "unittest/Unittest.h"
 
 using namespace std;
 
 namespace logtail {
 
 class ProcessorParseFromPBNativeUnittest : public testing::Test {
 public:
     void SetUp() override { mContext.SetConfigName("project##config_0"); }
 
     void TestInit();
     void TestProcessValidSpanData();
     void TestProcessMultiValidSpanData();
     void TestProcessEmptyEventGroup();
     void TestProcessNonRawEventGroup();
     void TestProcessInvalidProtobufData();
     void TestProcessMultiInvalidProtobufData();
     void TestProcessPartialInvalidProtobufData();
 
 private:
     void prepareValidProcessor(ProcessorParseFromPBNative&);
 
     void generateValidSpanData(PipelineEventGroup&);
     void generateInvalidSpanData(PipelineEventGroup&);
     void generateNonRawEventData(PipelineEventGroup&);
     
     void assertValidSpanData(const EventsContainer&, int32_t offset);
 
     void generateHttpServerValidSpanData(logtail::models::PipelineEventGroup&);
     void generateNoSQLValidSpanData(logtail::models::PipelineEventGroup&);
 
     void assertHttpServerValidSpanData(const PipelineEventPtr&);
     void assertNoSQLValidSpanData(const PipelineEventPtr&);
 
     CollectionPipelineContext mContext;
 };
 
 void ProcessorParseFromPBNativeUnittest::TestInit() {
     ProcessorParseFromPBNative processor;
     processor.SetContext(mContext);
     processor.CreateMetricsRecordRef(ProcessorParseFromPBNative::sName, "1");
 
     // Case 1: empty config
     {
         Json::Value config;
         APSARA_TEST_FALSE(processor.Init(config));
     }
 
     // Case 2: valid Protocol config
     {
         Json::Value config;
         config["Protocol"] = "LoongSuite";
         APSARA_TEST_TRUE(processor.Init(config));
     }
 
     // Case 3: unsupported Protocol
     {
         Json::Value config;
         config["Protocol"] = "OTAP";
         APSARA_TEST_FALSE(processor.Init(config));
     }
 
     // Case 4: Protocol field type error
     {
         Json::Value config;
         config["Protocol"] = 123;
         APSARA_TEST_FALSE(processor.Init(config));
     }
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessValidSpanData() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with raw span data
     PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
     this->generateValidSpanData(eventGroup);
 
     // Process the event
     APSARA_TEST_EQUAL((size_t)1, eventGroup.GetEvents().size());
     processor.Process(eventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)2, eventGroup.GetEvents().size());
     this->assertValidSpanData(eventGroup.GetEvents(), 0);
 
     APSARA_TEST_EQUAL(uint64_t(1), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(2), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessMultiValidSpanData() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with raw span data
     PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
     this->generateValidSpanData(eventGroup);
     this->generateValidSpanData(eventGroup);
 
     // Process the event
     APSARA_TEST_EQUAL((size_t)2, eventGroup.GetEvents().size());
     processor.Process(eventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)4, eventGroup.GetEvents().size());
     this->assertValidSpanData(eventGroup.GetEvents(), 0);
     this->assertValidSpanData(eventGroup.GetEvents(), 2);
 
     APSARA_TEST_EQUAL(uint64_t(2), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(4), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessEmptyEventGroup() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with no raw event
     PipelineEventGroup invalidEventGroup(std::make_shared<SourceBuffer>());
     APSARA_TEST_EQUAL((size_t)0, invalidEventGroup.GetEvents().size());
 
     // Process the event
     processor.Process(invalidEventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)0, invalidEventGroup.GetEvents().size());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessNonRawEventGroup() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with non raw event
     PipelineEventGroup nonRawEventGroup(std::make_shared<SourceBuffer>());
     this->generateNonRawEventData(nonRawEventGroup);
 
     // Process the event
     processor.Process(nonRawEventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)0, nonRawEventGroup.GetEvents().size());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(1), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessInvalidProtobufData() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with invalid protobuf data
     PipelineEventGroup invalidEventGroup(std::make_shared<SourceBuffer>());
     this->generateInvalidSpanData(invalidEventGroup);
 
     // Process the event
     APSARA_TEST_EQUAL((size_t)1, invalidEventGroup.GetEvents().size());
     processor.Process(invalidEventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)0, invalidEventGroup.GetEvents().size());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(1), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessMultiInvalidProtobufData() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with multi invalid protobuf data
     PipelineEventGroup invalidEventGroup(std::make_shared<SourceBuffer>());
     this->generateInvalidSpanData(invalidEventGroup);
     this->generateInvalidSpanData(invalidEventGroup);
 
     // Process the event
     APSARA_TEST_EQUAL((size_t)2, invalidEventGroup.GetEvents().size());
     processor.Process(invalidEventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)0, invalidEventGroup.GetEvents().size());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(2), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::TestProcessPartialInvalidProtobufData() {
     ProcessorParseFromPBNative processor;
     this->prepareValidProcessor(processor);
 
     // Prepare event group with partial invalid protobuf data
     PipelineEventGroup eventGroup(std::make_shared<SourceBuffer>());
     this->generateValidSpanData(eventGroup);
     this->generateInvalidSpanData(eventGroup);
 
     // Process the event
     APSARA_TEST_EQUAL((size_t)2, eventGroup.GetEvents().size());
     processor.Process(eventGroup);
 
     // Validate output
     APSARA_TEST_EQUAL((size_t)2, eventGroup.GetEvents().size());
     this->assertValidSpanData(eventGroup.GetEvents(), 0);
     
     APSARA_TEST_EQUAL(uint64_t(1), processor.mOutSuccessfulEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(1), processor.mOutFailedEventGroupsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(0), processor.mDiscardedEventsTotal->GetValue());
     APSARA_TEST_EQUAL(uint64_t(2), processor.mOutSuccessfulEventsTotal->GetValue());
 }
 
 void ProcessorParseFromPBNativeUnittest::prepareValidProcessor(ProcessorParseFromPBNative& processor) {
     Json::Value config;
     config["Protocol"] = "LoongSuite";
     processor.SetContext(mContext);
     processor.CreateMetricsRecordRef(ProcessorParseFromPBNative::sName, "1");
     APSARA_TEST_TRUE(processor.Init(config));
 }
 
 void ProcessorParseFromPBNativeUnittest::generateValidSpanData(logtail::PipelineEventGroup& eventGroup) {
     logtail::models::PipelineEventGroup pbEventGroup;
     this->generateHttpServerValidSpanData(pbEventGroup);
     this->generateNoSQLValidSpanData(pbEventGroup);
 
     eventGroup.AddRawEvent()->SetContent(pbEventGroup.SerializeAsString());
 }
 
 void ProcessorParseFromPBNativeUnittest::generateInvalidSpanData(logtail::PipelineEventGroup& eventGroup) {
     eventGroup.AddRawEvent()->SetContent("invalid_protobuf_data");
 }

 void ProcessorParseFromPBNativeUnittest::generateNonRawEventData(logtail::PipelineEventGroup& eventGroup) {
    logtail::models::PipelineEventGroup pbEventGroup;
    this->generateHttpServerValidSpanData(pbEventGroup);
    std::string errMsg;
    ASSERT_TRUE(TransferPBToPipelineEventGroup(pbEventGroup, eventGroup, errMsg));
 }
 
 void ProcessorParseFromPBNativeUnittest::generateHttpServerValidSpanData(
     logtail::models::PipelineEventGroup& eventGroup) {
     models::SpanEvent pbSpan;
 
     pbSpan.set_traceid("cba78930fe0c2626bc60696a3453cc40");
     pbSpan.set_spanid("4083239a6a2e704e");
     pbSpan.set_parentspanid("0000000000000000");
     pbSpan.set_name("/components/api/v1/http/success");
     pbSpan.set_kind(models::SpanEvent::SERVER); // kind=2 is SERVER
     pbSpan.set_starttime(1748313835253000000ULL);
     pbSpan.set_endtime(1748313840262969241ULL);
     pbSpan.set_status(models::SpanEvent::Unset); // statusCode=0
 
     std::map<std::string, std::string> mergedTags1 = {{"http.path", "/components/api/v1/http/success"},
                                                       {"endpoint", "mall-user-service:9190"},
                                                       {"http.method", "POST"},
                                                       {"component.name", "http"},
                                                       {"http.status_code", "200"},
                                                       {"http.route", "/components/api/v1/http/success"}};
 
     for (const auto& tag : mergedTags1) {
         (*pbSpan.mutable_tags())[tag.first] = tag.second;
     }
 
     std::map<std::string, std::string> mergedScopeTags1
         = {{"otel.scope.version", "1.28.0-alpha"}, {"otel.scope.name", "io.opentelemetry.tomcat-8.0.15"}};
 
     for (const auto& tag : mergedScopeTags1) {
         (*pbSpan.mutable_scopetags())[tag.first] = tag.second;
     }
 
     auto* spanEvents = eventGroup.mutable_spans();
     *spanEvents->add_events() = pbSpan;
 }
 
 void ProcessorParseFromPBNativeUnittest::generateNoSQLValidSpanData(logtail::models::PipelineEventGroup& eventGroup) {
     models::SpanEvent pbSpan;
 
     pbSpan.set_traceid("cba78930fe0c2626bc60696a3453cc40");
     pbSpan.set_spanid("9a2c1a8a371d6798");
     pbSpan.set_parentspanid("4083239a6a2e704e");
     pbSpan.set_name("LLEN");
     pbSpan.set_kind(models::SpanEvent::CLIENT); // kind=3 is SERVER
     pbSpan.set_starttime(1748313840259486017ULL);
     pbSpan.set_endtime(1748313840259765375ULL);
     pbSpan.set_status(models::SpanEvent::Unset); // statusCode=0
 
     std::map<std::string, std::string> mergedTags2 = {{"db.system", "redis"},
                                                       {"endpoint", "redis:6379"},
                                                       {"component.name", "redis"},
                                                       {"db.name", "redis:6379"},
                                                       {"net.peer.name", "redis:6379"},
                                                       {"redis.args", "key<big_key>"},
                                                       {"db.statement.id", "2191aada7df3c872"}};
 
     for (const auto& tag : mergedTags2) {
         (*pbSpan.mutable_tags())[tag.first] = tag.second;
     }
 
     std::map<std::string, std::string> mergedScopeTags2
         = {{"otel.scope.version", "1.28.0-alpha"}, {"otel.scope.name", "io.opentelemetry.lettuce-5.1"}};
 
     for (const auto& tag : mergedScopeTags2) {
         (*pbSpan.mutable_scopetags())[tag.first] = tag.second;
     }
 
     auto* spanEvents = eventGroup.mutable_spans();
     *spanEvents->add_events() = pbSpan;
 }
 
 void ProcessorParseFromPBNativeUnittest::assertValidSpanData(const EventsContainer& events, int32_t offset) {
     assertHttpServerValidSpanData(events[0 + offset]);
     assertNoSQLValidSpanData(events[1 + offset]);
 }
 
 
 void ProcessorParseFromPBNativeUnittest::assertHttpServerValidSpanData(const PipelineEventPtr& event) {
     APSARA_TEST_TRUE(event.Is<SpanEvent>());
     const auto& spanEvent = event.Cast<SpanEvent>();
 
     APSARA_TEST_EQUAL("cba78930fe0c2626bc60696a3453cc40", spanEvent.GetTraceId());
     APSARA_TEST_EQUAL("4083239a6a2e704e", spanEvent.GetSpanId());
     APSARA_TEST_EQUAL("0000000000000000", spanEvent.GetParentSpanId());
     APSARA_TEST_EQUAL("/components/api/v1/http/success", spanEvent.GetName());
     APSARA_TEST_EQUAL(SpanEvent::Kind::Server, spanEvent.GetKind());
     APSARA_TEST_EQUAL(1748313835253000000ULL, spanEvent.GetStartTimeNs());
     APSARA_TEST_EQUAL(1748313840262969241ULL, spanEvent.GetEndTimeNs());
     APSARA_TEST_EQUAL(SpanEvent::StatusCode::Unset, spanEvent.GetStatus());
 
     // Assert tags
     APSARA_TEST_EQUAL(6, spanEvent.TagsSize());
     APSARA_TEST_EQUAL("/components/api/v1/http/success", spanEvent.GetTag("http.path"));
     APSARA_TEST_EQUAL("mall-user-service:9190", spanEvent.GetTag("endpoint"));
     APSARA_TEST_EQUAL("POST", spanEvent.GetTag("http.method"));
     APSARA_TEST_EQUAL("http", spanEvent.GetTag("component.name"));
     APSARA_TEST_EQUAL("200", spanEvent.GetTag("http.status_code"));
     APSARA_TEST_EQUAL("/components/api/v1/http/success", spanEvent.GetTag("http.route"));
 
     // Assert scope tags
     APSARA_TEST_EQUAL(2, spanEvent.ScopeTagsSize());
     APSARA_TEST_EQUAL("1.28.0-alpha", spanEvent.GetScopeTag("otel.scope.version"));
     APSARA_TEST_EQUAL("io.opentelemetry.tomcat-8.0.15", spanEvent.GetScopeTag("otel.scope.name"));
 }
 
 void ProcessorParseFromPBNativeUnittest::assertNoSQLValidSpanData(const PipelineEventPtr& event) {
     APSARA_TEST_TRUE(event.Is<SpanEvent>());
     const auto& spanEvent = event.Cast<SpanEvent>();
 
     APSARA_TEST_EQUAL("cba78930fe0c2626bc60696a3453cc40", spanEvent.GetTraceId());
     APSARA_TEST_EQUAL("9a2c1a8a371d6798", spanEvent.GetSpanId());
     APSARA_TEST_EQUAL("4083239a6a2e704e", spanEvent.GetParentSpanId());
     APSARA_TEST_EQUAL("LLEN", spanEvent.GetName());
     APSARA_TEST_EQUAL(SpanEvent::Kind::Client, spanEvent.GetKind());
     APSARA_TEST_EQUAL(1748313840259486017ULL, spanEvent.GetStartTimeNs());
     APSARA_TEST_EQUAL(1748313840259765375ULL, spanEvent.GetEndTimeNs());
     APSARA_TEST_EQUAL(SpanEvent::StatusCode::Unset, spanEvent.GetStatus());
 
     // Assert tags
     APSARA_TEST_EQUAL(7, spanEvent.TagsSize());
     APSARA_TEST_EQUAL("redis", spanEvent.GetTag("db.system"));
     APSARA_TEST_EQUAL("redis:6379", spanEvent.GetTag("endpoint"));
     APSARA_TEST_EQUAL("redis", spanEvent.GetTag("component.name"));
     APSARA_TEST_EQUAL("redis:6379", spanEvent.GetTag("db.name"));
     APSARA_TEST_EQUAL("redis:6379", spanEvent.GetTag("net.peer.name"));
     APSARA_TEST_EQUAL("key<big_key>", spanEvent.GetTag("redis.args"));
     APSARA_TEST_EQUAL("2191aada7df3c872", spanEvent.GetTag("db.statement.id"));
 
     // Assert scope tags
     APSARA_TEST_EQUAL(2, spanEvent.ScopeTagsSize());
     APSARA_TEST_EQUAL("1.28.0-alpha", spanEvent.GetScopeTag("otel.scope.version"));
     APSARA_TEST_EQUAL("io.opentelemetry.lettuce-5.1", spanEvent.GetScopeTag("otel.scope.name"));
 }
 
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestInit)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessValidSpanData)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessEmptyEventGroup)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessNonRawEventGroup)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessInvalidProtobufData)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessMultiValidSpanData)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessMultiInvalidProtobufData)
 UNIT_TEST_CASE(ProcessorParseFromPBNativeUnittest, TestProcessPartialInvalidProtobufData)
 
 } // namespace logtail
 
 UNIT_TEST_MAIN
 