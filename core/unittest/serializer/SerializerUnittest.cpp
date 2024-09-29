// Copyright 2024 iLogtail Authors
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

#include "monitor/metric_constants/MetricConstants.h"
#include "pipeline/plugin/interface/Flusher.h"
#include "pipeline/serializer/Serializer.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class SerializerMock : public Serializer<BatchedEvents> {
public:
    SerializerMock(Flusher* f) : Serializer<BatchedEvents>(f) {};

private:
    bool Serialize(BatchedEvents&& p, std::string& res, std::string& errorMsg) override {
        if (p.mEvents.empty()) {
            return false;
        }
        res = "result";
        return true;
    }
};

class SerializerUnittest : public ::testing::Test {
public:
    void TestMetric();

protected:
    static void SetUpTestCase() { sFlusher = make_unique<FlusherMock>(); }

    void SetUp() override {
        mCtx.SetConfigName("test_config");
        sFlusher->SetContext(mCtx);
        sFlusher->SetMetricsRecordRef(FlusherMock::sName, "1");
    }

private:
    static unique_ptr<Flusher> sFlusher;

    BatchedEvents CreateBatchedMetricEvents(bool withEvents = true);

    PipelineContext mCtx;
};

unique_ptr<Flusher> SerializerUnittest::sFlusher;

void SerializerUnittest::TestMetric() {
    {
        SerializerMock serializer(sFlusher.get());
        auto input = CreateBatchedMetricEvents();
        auto inputSize = input.mSizeBytes;
        string output;
        string errorMsg;
        serializer.DoSerialize(std::move(input), output, errorMsg);
        APSARA_TEST_EQUAL(1U, serializer.mInItemsTotal->GetValue());
        APSARA_TEST_EQUAL(inputSize, serializer.mInItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(1U, serializer.mOutItemsTotal->GetValue());
        APSARA_TEST_EQUAL(output.size(), serializer.mOutItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(0U, serializer.mDiscardedItemsTotal->GetValue());
        APSARA_TEST_EQUAL(0U, serializer.mDiscardedItemSizeBytes->GetValue());
    }
    {
        SerializerMock serializer(sFlusher.get());
        auto input = CreateBatchedMetricEvents(false);
        auto inputSize = input.mSizeBytes;
        string output;
        string errorMsg;
        serializer.DoSerialize(std::move(input), output, errorMsg);
        APSARA_TEST_EQUAL(1U, serializer.mInItemsTotal->GetValue());
        APSARA_TEST_EQUAL(inputSize, serializer.mInItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(0U, serializer.mOutItemsTotal->GetValue());
        APSARA_TEST_EQUAL(0U, serializer.mOutItemSizeBytes->GetValue());
        APSARA_TEST_EQUAL(1U, serializer.mDiscardedItemsTotal->GetValue());
        APSARA_TEST_EQUAL(inputSize, serializer.mDiscardedItemSizeBytes->GetValue());
    }
}

BatchedEvents SerializerUnittest::CreateBatchedMetricEvents(bool withEvents) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(string("key"), string("value"));
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    if (withEvents) {
        group.AddLogEvent();
    }
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

UNIT_TEST_CASE(SerializerUnittest, TestMetric)

} // namespace logtail

UNIT_TEST_MAIN
