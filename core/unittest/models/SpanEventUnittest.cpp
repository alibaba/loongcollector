// Copyright 2024 iLogtail Authors
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

#include <memory>

#include "common/JsonUtil.h"
#include "models/SpanEvent.h"
#include "unittest/Unittest.h"

using namespace std;

namespace logtail {

class SpanEventUnittest : public ::testing::Test {
public:
    void TestSimpleFields();
    void TestTag();
    void TestLink();
    void TestEvent();
    void TestScopeTag();
    void TestSize();
    void TestToJson();
    void TestFromJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mSpanEvent = mEventGroup->CreateSpanEvent();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<SpanEvent> mSpanEvent;
};

void SpanEventUnittest::TestSimpleFields() {
    mSpanEvent->SetTraceId("test_trace_id");
    APSARA_TEST_EQUAL("test_trace_id", mSpanEvent->GetTraceId().to_string());

    mSpanEvent->SetSpanId("test_span_id");
    APSARA_TEST_EQUAL("test_span_id", mSpanEvent->GetSpanId().to_string());

    mSpanEvent->SetTraceState("normal");
    APSARA_TEST_EQUAL("normal", mSpanEvent->GetTraceState().to_string());

    mSpanEvent->SetParentSpanId("test_parent_span_id");
    APSARA_TEST_EQUAL("test_parent_span_id", mSpanEvent->GetParentSpanId().to_string());

    mSpanEvent->SetName("test_name");
    APSARA_TEST_EQUAL("test_name", mSpanEvent->GetName().to_string());

    mSpanEvent->SetKind(SpanEvent::Kind::Client);
    APSARA_TEST_EQUAL(SpanEvent::Kind::Client, mSpanEvent->GetKind());

    mSpanEvent->SetStartTimeNs(1715826723000000000);
    APSARA_TEST_EQUAL(1715826723000000000U, mSpanEvent->GetStartTimeNs());

    mSpanEvent->SetEndTimeNs(1715826725000000000);
    APSARA_TEST_EQUAL(1715826725000000000U, mSpanEvent->GetEndTimeNs());

    mSpanEvent->SetStatus(SpanEvent::StatusCode::Ok);
    APSARA_TEST_EQUAL(SpanEvent::StatusCode::Ok, mSpanEvent->GetStatus());
}

void SpanEventUnittest::TestTag() {
    {
        string key = "key1";
        string value = "value1";
        mSpanEvent->SetTag(key, value);
        APSARA_TEST_TRUE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetTag(key).to_string());
    }
    {
        string key = "key2";
        string value = "value2";
        mSpanEvent->SetTag(StringView(key.data(), key.size()), StringView(value.data(), value.size()));
        APSARA_TEST_TRUE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetTag(key).to_string());
    }
    {
        string key = "key3";
        string value = "value3";
        mSpanEvent->SetTagNoCopy(mSpanEvent->GetSourceBuffer()->CopyString(key),
                                 mSpanEvent->GetSourceBuffer()->CopyString(value));
        APSARA_TEST_TRUE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetTag(key).to_string());
    }
    {
        string key = "key4";
        string value = "value4";
        const StringBuffer& kb = mSpanEvent->GetSourceBuffer()->CopyString(key);
        const StringBuffer& vb = mSpanEvent->GetSourceBuffer()->CopyString(value);
        mSpanEvent->SetTagNoCopy(StringView(kb.data, kb.size), StringView(vb.data, vb.size));
        APSARA_TEST_TRUE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetTag(key).to_string());
    }
    {
        string key = "key5";
        APSARA_TEST_FALSE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mSpanEvent->GetTag(key).to_string());
    }
    {
        string key = "key1";
        mSpanEvent->DelTag(key);
        APSARA_TEST_FALSE(mSpanEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mSpanEvent->GetTag(key).to_string());
    }
}

void SpanEventUnittest::TestLink() {
    SpanEvent::SpanLink* l = mSpanEvent->AddLink();
    APSARA_TEST_EQUAL(1U, mSpanEvent->GetLinks().size());
    APSARA_TEST_EQUAL(l, &mSpanEvent->GetLinks()[0]);
}

void SpanEventUnittest::TestEvent() {
    SpanEvent::InnerEvent* e = mSpanEvent->AddEvent();
    APSARA_TEST_EQUAL(1U, mSpanEvent->GetEvents().size());
    APSARA_TEST_EQUAL(e, &mSpanEvent->GetEvents()[0]);
}

void SpanEventUnittest::TestScopeTag() {
    {
        string key = "key1";
        string value = "value1";
        mSpanEvent->SetScopeTag(key, value);
        APSARA_TEST_TRUE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetScopeTag(key).to_string());
    }
    {
        string key = "key2";
        string value = "value2";
        mSpanEvent->SetScopeTag(StringView(key.data(), key.size()), StringView(value.data(), value.size()));
        APSARA_TEST_TRUE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetScopeTag(key).to_string());
    }
    {
        string key = "key3";
        string value = "value3";
        mSpanEvent->SetScopeTagNoCopy(mSpanEvent->GetSourceBuffer()->CopyString(key),
                                      mSpanEvent->GetSourceBuffer()->CopyString(value));
        APSARA_TEST_TRUE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetScopeTag(key).to_string());
    }
    {
        string key = "key4";
        string value = "value4";
        const StringBuffer& kb = mSpanEvent->GetSourceBuffer()->CopyString(key);
        const StringBuffer& vb = mSpanEvent->GetSourceBuffer()->CopyString(value);
        mSpanEvent->SetScopeTagNoCopy(StringView(kb.data, kb.size), StringView(vb.data, vb.size));
        APSARA_TEST_TRUE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL(value, mSpanEvent->GetScopeTag(key).to_string());
    }
    {
        string key = "key5";
        APSARA_TEST_FALSE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL("", mSpanEvent->GetScopeTag(key).to_string());
    }
    {
        string key = "key1";
        mSpanEvent->DelScopeTag(key);
        APSARA_TEST_FALSE(mSpanEvent->HasScopeTag(key));
        APSARA_TEST_EQUAL("", mSpanEvent->GetScopeTag(key).to_string());
    }
}

void SpanEventUnittest::TestSize() {
    size_t basicSize = sizeof(time_t) + sizeof(long) + sizeof(SpanEvent::Kind) + sizeof(uint64_t) + sizeof(uint64_t)
        + sizeof(SpanEvent::StatusCode) + sizeof(vector<SpanEvent::InnerEvent>) + sizeof(vector<SpanEvent::SpanLink>)
        + sizeof(map<StringView, StringView>) + sizeof(map<StringView, StringView>);

    mSpanEvent->SetTraceId("test_trace_id");
    mSpanEvent->SetSpanId("test_span_id");
    mSpanEvent->SetTraceState("normal");
    mSpanEvent->SetParentSpanId("test_parent_span_id");
    mSpanEvent->SetName("test_name");
    mSpanEvent->SetKind(SpanEvent::Kind::Client);
    mSpanEvent->SetStatus(SpanEvent::StatusCode::Ok);
    basicSize += strlen("test_trace_id") + strlen("test_span_id") + strlen("normal") + strlen("test_parent_span_id")
        + strlen("test_name");

    {
        // add tag, key not existed
        mSpanEvent->SetTag(string("key1"), string("a"));
        APSARA_TEST_EQUAL(basicSize + 5U, mSpanEvent->DataSize());

        // add tag, key existed
        mSpanEvent->SetTag(string("key1"), string("bb"));
        APSARA_TEST_EQUAL(basicSize + 6U, mSpanEvent->DataSize());

        // delete tag
        mSpanEvent->DelTag(string("key1"));
        APSARA_TEST_EQUAL(basicSize, mSpanEvent->DataSize());
    }
    {
        // add scope tag, key not existed
        mSpanEvent->SetScopeTag(string("key1"), string("a"));
        APSARA_TEST_EQUAL(basicSize + 5U, mSpanEvent->DataSize());

        // add scope tag, key existed
        mSpanEvent->SetScopeTag(string("key1"), string("bb"));
        APSARA_TEST_EQUAL(basicSize + 6U, mSpanEvent->DataSize());

        // delete scope tag
        mSpanEvent->DelScopeTag(string("key1"));
        APSARA_TEST_EQUAL(basicSize, mSpanEvent->DataSize());
    }
    {
        SpanEvent::InnerEvent* e = mSpanEvent->AddEvent();
        size_t newBasicSize = basicSize + sizeof(uint64_t) + sizeof(map<StringView, StringView>);

        e->SetName("test_event");
        newBasicSize += strlen("test_event");

        e->SetTag(string("key1"), string("a"));
        APSARA_TEST_EQUAL(newBasicSize + 5U, mSpanEvent->DataSize());

        mSpanEvent->mEvents.clear();
    }
    {
        SpanEvent::SpanLink* l = mSpanEvent->AddLink();
        size_t newBasicSize = basicSize + sizeof(map<StringView, StringView>);

        l->SetTraceId("other_trace_id");
        l->SetSpanId("other_span_id");
        l->SetTraceState("normal");
        newBasicSize += strlen("other_trace_id") + strlen("other_span_id") + strlen("normal");

        l->SetTag(string("key1"), string("a"));
        APSARA_TEST_EQUAL(newBasicSize + 5U, mSpanEvent->DataSize());

        mSpanEvent->mLinks.clear();
    }
}

void SpanEventUnittest::TestToJson() {
    mSpanEvent->SetTimestamp(12345678901, 0);
    mSpanEvent->SetTraceId("test_trace_id");
    mSpanEvent->SetSpanId("test_span_id");
    mSpanEvent->SetTraceState("normal");
    mSpanEvent->SetParentSpanId("test_parent_span_id");
    mSpanEvent->SetName("test_name");
    mSpanEvent->SetKind(SpanEvent::Kind::Client);
    mSpanEvent->SetStartTimeNs(1715826723000000000);
    mSpanEvent->SetEndTimeNs(1715826725000000000);
    mSpanEvent->SetTag(string("key1"), string("value1"));
    SpanEvent::InnerEvent* e = mSpanEvent->AddEvent();
    e->SetName("test_event");
    e->SetTimestampNs(1715826724000000000);
    SpanEvent::SpanLink* l = mSpanEvent->AddLink();
    l->SetTraceId("other_trace_id");
    l->SetSpanId("other_span_id");
    mSpanEvent->SetStatus(SpanEvent::StatusCode::Ok);
    mSpanEvent->SetScopeTag(string("key2"), string("value2"));
    Json::Value res = mSpanEvent->ToJson();

    Json::Value eventJson;
    string eventStr = R"({
        "type": 3,
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "traceId": "test_trace_id",
        "spanId": "test_span_id",
        "traceState": "normal",
        "parentSpanId": "test_parent_span_id",
        "name": "test_name",
        "kind": 3,
        "startTimeNs": 1715826723000000000,
        "endTimeNs": 1715826725000000000,
        "tags": {
            "key1": "value1"
        },
        "events": [
            {
                "name": "test_event",
                "timestampNs": 1715826724000000000
            }
        ],
        "links": [
            {
                "traceId": "other_trace_id",
                "spanId": "other_span_id"
            }
        ],
        "status": 1,
        "scopeTags": {
            "key2": "value2"
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);

    APSARA_TEST_TRUE(eventJson == res);
}

void SpanEventUnittest::TestFromJson() {
    Json::Value eventJson;
    string eventStr = R"({
        "timestamp" : 12345678901,
        "timestampNanosecond" : 0,
        "traceId": "test_trace_id",
        "spanId": "test_span_id",
        "traceState": "normal",
        "parentSpanId": "test_parent_span_id",
        "name": "test_name",
        "kind": 3,
        "startTimeNs": 1715826723000000000,
        "endTimeNs": 1715826725000000000,
        "tags": {
            "key1": "value1"
        },
        "events": [
            {
                "name": "test_event",
                "timestampNs": 1715826724000000000
            }
        ],
        "links": [
            {
                "traceId": "other_trace_id",
                "spanId": "other_span_id"
            }
        ],
        "status": 1,
        "scopeTags": {
            "key2": "value2"
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);
    mSpanEvent->FromJson(eventJson);

    APSARA_TEST_EQUAL(12345678901, mSpanEvent->GetTimestamp());
    APSARA_TEST_EQUAL(0L, mSpanEvent->GetTimestampNanosecond().value());
    APSARA_TEST_EQUAL("test_trace_id", mSpanEvent->GetTraceId().to_string());
    APSARA_TEST_EQUAL("test_span_id", mSpanEvent->GetSpanId().to_string());
    APSARA_TEST_EQUAL("normal", mSpanEvent->GetTraceState().to_string());
    APSARA_TEST_EQUAL("test_parent_span_id", mSpanEvent->GetParentSpanId().to_string());
    APSARA_TEST_EQUAL("test_name", mSpanEvent->GetName().to_string());
    APSARA_TEST_EQUAL(SpanEvent::Kind::Client, mSpanEvent->GetKind());
    APSARA_TEST_EQUAL(1715826723000000000U, mSpanEvent->GetStartTimeNs());
    APSARA_TEST_EQUAL(1715826725000000000U, mSpanEvent->GetEndTimeNs());
    APSARA_TEST_EQUAL("value1", mSpanEvent->GetTag("key1"));
    APSARA_TEST_EQUAL(1U, mSpanEvent->GetEvents().size());
    APSARA_TEST_EQUAL(1U, mSpanEvent->GetLinks().size());
    APSARA_TEST_EQUAL(SpanEvent::StatusCode::Ok, mSpanEvent->GetStatus());
    APSARA_TEST_EQUAL("value2", mSpanEvent->GetScopeTag("key2"));
}

UNIT_TEST_CASE(SpanEventUnittest, TestSimpleFields)
UNIT_TEST_CASE(SpanEventUnittest, TestTag)
UNIT_TEST_CASE(SpanEventUnittest, TestLink)
UNIT_TEST_CASE(SpanEventUnittest, TestEvent)
UNIT_TEST_CASE(SpanEventUnittest, TestScopeTag)
UNIT_TEST_CASE(SpanEventUnittest, TestSize)
UNIT_TEST_CASE(SpanEventUnittest, TestToJson)
UNIT_TEST_CASE(SpanEventUnittest, TestFromJson)

class InnerEventUnittest : public ::testing::Test {
public:
    void TestSimpleFields();
    void TestTag();
    void TestSize();
    void TestToJson();
    void TestFromJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mSpanEvent = mEventGroup->CreateSpanEvent();
        mInnerEvent = mSpanEvent->AddEvent();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<SpanEvent> mSpanEvent;
    SpanEvent::InnerEvent* mInnerEvent;
};

void InnerEventUnittest::TestSimpleFields() {
    mInnerEvent->SetTimestampNs(1715826723000000000);
    APSARA_TEST_EQUAL(1715826723000000000U, mInnerEvent->GetTimestampNs());

    mInnerEvent->SetName("test_name");
    APSARA_TEST_EQUAL("test_name", mInnerEvent->GetName().to_string());
}

void InnerEventUnittest::TestTag() {
    {
        string key = "key1";
        string value = "value1";
        mInnerEvent->SetTag(key, value);
        APSARA_TEST_TRUE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mInnerEvent->GetTag(key).to_string());
    }
    {
        string key = "key2";
        string value = "value2";
        mInnerEvent->SetTag(StringView(key.data(), key.size()), StringView(value.data(), value.size()));
        APSARA_TEST_TRUE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mInnerEvent->GetTag(key).to_string());
    }
    {
        string key = "key3";
        string value = "value3";
        mInnerEvent->SetTagNoCopy(mInnerEvent->GetSourceBuffer()->CopyString(key),
                                  mInnerEvent->GetSourceBuffer()->CopyString(value));
        APSARA_TEST_TRUE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mInnerEvent->GetTag(key).to_string());
    }
    {
        string key = "key4";
        string value = "value4";
        const StringBuffer& kb = mInnerEvent->GetSourceBuffer()->CopyString(key);
        const StringBuffer& vb = mInnerEvent->GetSourceBuffer()->CopyString(value);
        mInnerEvent->SetTagNoCopy(StringView(kb.data, kb.size), StringView(vb.data, vb.size));
        APSARA_TEST_TRUE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL(value, mInnerEvent->GetTag(key).to_string());
    }
    {
        string key = "key5";
        APSARA_TEST_FALSE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mInnerEvent->GetTag(key).to_string());
    }
    {
        string key = "key1";
        mInnerEvent->DelTag(key);
        APSARA_TEST_FALSE(mInnerEvent->HasTag(key));
        APSARA_TEST_EQUAL("", mInnerEvent->GetTag(key).to_string());
    }
}

void InnerEventUnittest::TestSize() {
    size_t basicSize = sizeof(uint64_t) + sizeof(map<StringView, StringView>);

    mInnerEvent->SetName("test");
    basicSize += strlen("test");

    // add tag, and key not existed
    mInnerEvent->SetTag(string("key1"), string("a"));
    APSARA_TEST_EQUAL(basicSize + 5U, mInnerEvent->DataSize());

    // add tag, and key existed
    mInnerEvent->SetTag(string("key1"), string("bb"));
    APSARA_TEST_EQUAL(basicSize + 6U, mInnerEvent->DataSize());

    // delete tag
    mInnerEvent->DelTag(string("key1"));
    APSARA_TEST_EQUAL(basicSize, mInnerEvent->DataSize());
}

void InnerEventUnittest::TestToJson() {
    mInnerEvent->SetName("test");
    mInnerEvent->SetTimestampNs(1715826723000000000);
    mInnerEvent->SetTag(string("key1"), string("value1"));
    Json::Value res = mInnerEvent->ToJson();

    Json::Value eventJson;
    string eventStr = R"({
        "name": "test",
        "timestampNs": 1715826723000000000,
        "tags": {
            "key1": "value1"
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);

    APSARA_TEST_TRUE(eventJson == res);
}

void InnerEventUnittest::TestFromJson() {
    Json::Value eventJson;
    string eventStr = R"({
        "name": "test",
        "timestampNs": 1715826723000000000,
        "tags": {
            "key1": "value1"
        }
    })";
    string errorMsg;
    ParseJsonTable(eventStr, eventJson, errorMsg);
    mInnerEvent->FromJson(eventJson);

    APSARA_TEST_EQUAL("test", mInnerEvent->GetName().to_string());
    APSARA_TEST_EQUAL(1715826723000000000U, mInnerEvent->GetTimestampNs());
    APSARA_TEST_EQUAL("value1", mInnerEvent->GetTag("key1").to_string());
}

UNIT_TEST_CASE(InnerEventUnittest, TestSimpleFields)
UNIT_TEST_CASE(InnerEventUnittest, TestTag)
UNIT_TEST_CASE(InnerEventUnittest, TestSize)
UNIT_TEST_CASE(InnerEventUnittest, TestToJson)
UNIT_TEST_CASE(InnerEventUnittest, TestFromJson)

class SpanLinkUnittest : public ::testing::Test {
public:
    void TestSimpleFields();
    void TestTag();
    void TestSize();
    void TestToJson();
    void TestFromJson();

protected:
    void SetUp() override {
        mSourceBuffer.reset(new SourceBuffer);
        mEventGroup.reset(new PipelineEventGroup(mSourceBuffer));
        mSpanEvent = mEventGroup->CreateSpanEvent();
        mLink = mSpanEvent->AddLink();
    }

private:
    shared_ptr<SourceBuffer> mSourceBuffer;
    unique_ptr<PipelineEventGroup> mEventGroup;
    unique_ptr<SpanEvent> mSpanEvent;
    SpanEvent::SpanLink* mLink;
};

void SpanLinkUnittest::TestSimpleFields() {
    mLink->SetTraceId("test_trace_id");
    APSARA_TEST_EQUAL("test_trace_id", mLink->GetTraceId().to_string());

    mLink->SetSpanId("test_span_id");
    APSARA_TEST_EQUAL("test_span_id", mLink->GetSpanId().to_string());

    mLink->SetTraceState("normal");
    APSARA_TEST_EQUAL("normal", mLink->GetTraceState().to_string());
}

void SpanLinkUnittest::TestTag() {
    {
        string key = "key1";
        string value = "value1";
        mLink->SetTag(key, value);
        APSARA_TEST_TRUE(mLink->HasTag(key));
        APSARA_TEST_EQUAL(value, mLink->GetTag(key).to_string());
    }
    {
        string key = "key2";
        string value = "value2";
        mLink->SetTag(StringView(key.data(), key.size()), StringView(value.data(), value.size()));
        APSARA_TEST_TRUE(mLink->HasTag(key));
        APSARA_TEST_EQUAL(value, mLink->GetTag(key).to_string());
    }
    {
        string key = "key3";
        string value = "value3";
        mLink->SetTagNoCopy(mLink->GetSourceBuffer()->CopyString(key), mLink->GetSourceBuffer()->CopyString(value));
        APSARA_TEST_TRUE(mLink->HasTag(key));
        APSARA_TEST_EQUAL(value, mLink->GetTag(key).to_string());
    }
    {
        string key = "key4";
        string value = "value4";
        const StringBuffer& kb = mLink->GetSourceBuffer()->CopyString(key);
        const StringBuffer& vb = mLink->GetSourceBuffer()->CopyString(value);
        mLink->SetTagNoCopy(StringView(kb.data, kb.size), StringView(vb.data, vb.size));
        APSARA_TEST_TRUE(mLink->HasTag(key));
        APSARA_TEST_EQUAL(value, mLink->GetTag(key).to_string());
    }
    {
        string key = "key5";
        APSARA_TEST_FALSE(mLink->HasTag(key));
        APSARA_TEST_EQUAL("", mLink->GetTag(key).to_string());
    }
    {
        string key = "key1";
        mLink->DelTag(key);
        APSARA_TEST_FALSE(mLink->HasTag(key));
        APSARA_TEST_EQUAL("", mLink->GetTag(key).to_string());
    }
}

void SpanLinkUnittest::TestSize() {
    size_t basicSize = sizeof(map<StringView, StringView>);

    mLink->SetTraceId("test_trace_id");
    mLink->SetSpanId("test_span_id");
    mLink->SetTraceState("normal");
    basicSize += strlen("test_trace_id") + strlen("test_span_id") + strlen("normal");

    // add tag, and key not existed
    mLink->SetTag(string("key1"), string("a"));
    APSARA_TEST_EQUAL(basicSize + 5U, mLink->DataSize());

    // add tag, and key existed
    mLink->SetTag(string("key1"), string("bb"));
    APSARA_TEST_EQUAL(basicSize + 6U, mLink->DataSize());

    // delete tag
    mLink->DelTag(string("key1"));
    APSARA_TEST_EQUAL(basicSize, mLink->DataSize());
}

void SpanLinkUnittest::TestToJson() {
    mLink->SetTraceId("test_trace_id");
    mLink->SetSpanId("test_span_id");
    mLink->SetTraceState("normal");
    mLink->SetTag(string("key1"), string("value1"));
    Json::Value res = mLink->ToJson();

    Json::Value linkJson;
    string linkStr = R"({
        "traceId": "test_trace_id",
        "spanId": "test_span_id",
        "traceState": "normal",
        "tags": {
            "key1": "value1"
        }
    })";
    string errorMsg;
    ParseJsonTable(linkStr, linkJson, errorMsg);

    APSARA_TEST_TRUE(linkJson == res);
}

void SpanLinkUnittest::TestFromJson() {
    Json::Value linkJson;
    string linkStr = R"({
        "traceId": "test_trace_id",
        "spanId": "test_span_id",
        "traceState": "normal",
        "tags": {
            "key1": "value1"
        }
    })";
    string errorMsg;
    ParseJsonTable(linkStr, linkJson, errorMsg);
    mLink->FromJson(linkJson);

    APSARA_TEST_EQUAL("test_trace_id", mLink->GetTraceId().to_string());
    APSARA_TEST_EQUAL("test_span_id", mLink->GetSpanId().to_string());
    APSARA_TEST_EQUAL("normal", mLink->GetTraceState().to_string());
    APSARA_TEST_EQUAL("value1", mLink->GetTag("key1").to_string());
}

UNIT_TEST_CASE(SpanLinkUnittest, TestSimpleFields)
UNIT_TEST_CASE(SpanLinkUnittest, TestTag)
UNIT_TEST_CASE(SpanLinkUnittest, TestSize)
UNIT_TEST_CASE(SpanLinkUnittest, TestToJson)
UNIT_TEST_CASE(SpanLinkUnittest, TestFromJson)

} // namespace logtail

UNIT_TEST_MAIN
