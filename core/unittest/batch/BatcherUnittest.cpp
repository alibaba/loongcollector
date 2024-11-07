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

#include "common/JsonUtil.h"
#include "pipeline/batch/Batcher.h"
#include "unittest/Unittest.h"
#include "unittest/plugin/PluginMock.h"

using namespace std;

namespace logtail {

class BatcherUnittest : public ::testing::Test {
public:
    void TestParamInit();
    void TestInitWithoutGroupBatch();
    void TestInitWithGroupBatch();
    void TestAddWithoutGroupBatch();
    void TestAddWithGroupBatch();
    void TestAddWithOversizedGroup();
    void TestFlushEventQueueWithoutGroupBatch();
    void TestFlushEventQueueWithGroupBatch();
    void TestFlushGroupQueue();
    void TestFlushAllWithoutGroupBatch();
    void TestFlushAllWithGroupBatch();
    void TestMetric();

protected:
    static void SetUpTestCase() { sFlusher = make_unique<FlusherMock>(); }

    void SetUp() override {
        mCtx.SetConfigName("test_config");
        sFlusher->SetContext(mCtx);
        sFlusher->SetMetricsRecordRef(FlusherMock::sName, "1");
        sFlusher->SetPluginID("1");
    }

    void TearDown() override { TimeoutFlushManager::GetInstance()->mTimeoutRecords.clear(); }

private:
    PipelineEventGroup CreateEventGroup(size_t cnt);

    static unique_ptr<FlusherMock> sFlusher;

    PipelineContext mCtx;
};

unique_ptr<FlusherMock> BatcherUnittest::sFlusher;

void BatcherUnittest::TestParamInit() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 1;
    strategy.mMaxSizeBytes = 300;
    strategy.mMinSizeBytes = 100;
    strategy.mTimeoutSecs = 3;
    {
        // empty config
        Batcher<> batch;
        batch.Init(Json::Value(), sFlusher.get(), strategy);
        APSARA_TEST_EQUAL(1U, batch.mEventFlushStrategy.GetMinCnt());
        APSARA_TEST_EQUAL(100U, batch.mEventFlushStrategy.GetMinSizeBytes());
        APSARA_TEST_EQUAL(3U, batch.mEventFlushStrategy.GetTimeoutSecs());
        APSARA_TEST_EQUAL(300U, batch.mEventFlushStrategy.GetMaxSizeBytes());
    }
    {
        // invalid param
        Json::Value configJson;
        string configStr, errorMsg;
        configStr = R"(
            {
                "MinSizeBytes": "1000",
                "MinCnt": "10",
                "TimeoutSecs": "5"
            }
        )";
        APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));

        Batcher<> batch;
        batch.Init(configJson, sFlusher.get(), strategy);
        APSARA_TEST_EQUAL(1U, batch.mEventFlushStrategy.GetMinCnt());
        APSARA_TEST_EQUAL(100U, batch.mEventFlushStrategy.GetMinSizeBytes());
        APSARA_TEST_EQUAL(3U, batch.mEventFlushStrategy.GetTimeoutSecs());
        APSARA_TEST_EQUAL(300U, batch.mEventFlushStrategy.GetMaxSizeBytes());
    }
}

void BatcherUnittest::TestInitWithoutGroupBatch() {
    Json::Value configJson;
    string configStr, errorMsg;
    configStr = R"(
        {
            "MinSizeBytes": 1000,
            "MinCnt": 10,
            "TimeoutSecs": 5
        }
    )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));

    Batcher<> batch;
    batch.Init(configJson, sFlusher.get(), DefaultFlushStrategyOptions());
    APSARA_TEST_EQUAL(10U, batch.mEventFlushStrategy.GetMinCnt());
    APSARA_TEST_EQUAL(1000U, batch.mEventFlushStrategy.GetMinSizeBytes());
    APSARA_TEST_EQUAL(5U, batch.mEventFlushStrategy.GetTimeoutSecs());
    APSARA_TEST_EQUAL(numeric_limits<uint32_t>::max(), batch.mEventFlushStrategy.GetMaxSizeBytes());
    APSARA_TEST_EQUAL(sFlusher.get(), batch.mFlusher);
}

void BatcherUnittest::TestInitWithGroupBatch() {
    Json::Value configJson;
    string configStr, errorMsg;
    configStr = R"(
            {
                "MinSizeBytes": 1000,
                "MinCnt": 10,
                "TimeoutSecs": 5
            }
        )";
    APSARA_TEST_TRUE(ParseJsonTable(configStr, configJson, errorMsg));

    Batcher<> batch;
    batch.Init(configJson, sFlusher.get(), DefaultFlushStrategyOptions(), true);
    APSARA_TEST_EQUAL(10U, batch.mEventFlushStrategy.GetMinCnt());
    APSARA_TEST_EQUAL(1000U, batch.mEventFlushStrategy.GetMinSizeBytes());
    APSARA_TEST_EQUAL(3U, batch.mEventFlushStrategy.GetTimeoutSecs());
    APSARA_TEST_EQUAL(numeric_limits<uint32_t>::max(), batch.mEventFlushStrategy.GetMaxSizeBytes());
    APSARA_TEST_TRUE(batch.mGroupFlushStrategy);
    APSARA_TEST_EQUAL(1000U, batch.mGroupFlushStrategy->GetMinSizeBytes());
    APSARA_TEST_EQUAL(2U, batch.mGroupFlushStrategy->GetTimeoutSecs());
    APSARA_TEST_TRUE(batch.mGroupQueue);
    APSARA_TEST_EQUAL(sFlusher.get(), batch.mFlusher);
}

void BatcherUnittest::TestAddWithoutGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy);

    // add to empty batch item
    vector<BatchedEventsList> res;
    PipelineEventGroup group1 = CreateEventGroup(2);
    size_t key = group1.GetTagsHash();
    SourceBuffer* buffer1 = group1.GetSourceBuffer().get();
    RangeCheckpoint* eoo1 = group1.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group1), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(2U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(0U, res.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].size());
    TimeoutRecord& record = TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key));
    time_t updateTime = record.mUpdateTime;
    APSARA_TEST_EQUAL(3U, record.mTimeoutSecs);
    APSARA_TEST_EQUAL(sFlusher.get(), record.mFlusher);
    APSARA_TEST_EQUAL(key, record.mKey);
    APSARA_TEST_GT(updateTime, 0);

    // flush by cnt && one batch item contains more than 1 original event group
    PipelineEventGroup group2 = CreateEventGroup(2);
    SourceBuffer* buffer2 = group2.GetSourceBuffer().get();
    RangeCheckpoint* eoo2 = group2.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group2), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(3U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(2U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer1, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(buffer2, res[0][0].mSourceBuffers[1].get());
    APSARA_TEST_EQUAL(eoo1, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);

    // flush by time then by size
    res.clear();
    batch.mEventFlushStrategy.SetTimeoutSecs(0);
    batch.mEventFlushStrategy.SetMinSizeBytes(10);
    PipelineEventGroup group3 = CreateEventGroup(1);
    SourceBuffer* buffer3 = group3.GetSourceBuffer().get();
    RangeCheckpoint* eoo3 = group3.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group3), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(2U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(1U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer2, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo2, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);
    APSARA_TEST_EQUAL(1U, res[1].size());
    APSARA_TEST_EQUAL(1U, res[1][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[1][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[1][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[1][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer3, res[1][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo3, res[1][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[1][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);
}

void BatcherUnittest::TestAddWithGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy, true);

    // add to empty batch item
    vector<BatchedEventsList> res;
    PipelineEventGroup group1 = CreateEventGroup(2);
    size_t key = group1.GetTagsHash();
    SourceBuffer* buffer1 = group1.GetSourceBuffer().get();
    RangeCheckpoint* eoo1 = group1.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group1), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(2U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(0U, res.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].size());
    TimeoutRecord& record = TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key));
    time_t updateTime = record.mUpdateTime;
    APSARA_TEST_EQUAL(2U, record.mTimeoutSecs);
    APSARA_TEST_EQUAL(sFlusher.get(), record.mFlusher);
    APSARA_TEST_EQUAL(key, record.mKey);
    APSARA_TEST_GT(updateTime, 0);

    // flush by cnt && one batch item contains more than 1 original event group
    PipelineEventGroup group2 = CreateEventGroup(2);
    SourceBuffer* buffer2 = group2.GetSourceBuffer().get();
    RangeCheckpoint* eoo2 = group2.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group2), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(3U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(2U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer1, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(buffer2, res[0][0].mSourceBuffers[1].get());
    APSARA_TEST_EQUAL(eoo1, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);

    // flush by time to group batch
    res.clear();
    batch.mEventFlushStrategy.SetTimeoutSecs(0);
    PipelineEventGroup group3 = CreateEventGroup(1);
    SourceBuffer* buffer3 = group3.GetSourceBuffer().get();
    RangeCheckpoint* eoo3 = group3.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group3), res);
    APSARA_TEST_EQUAL(0U, res.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());

    // flush by time to group batch, and then group flush by time
    batch.mGroupFlushStrategy->SetTimeoutSecs(0);
    PipelineEventGroup group4 = CreateEventGroup(1);
    SourceBuffer* buffer4 = group4.GetSourceBuffer().get();
    RangeCheckpoint* eoo4 = group4.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group4), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(1U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer2, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo2, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);

    // flush by time to group batch, and then group flush by size
    res.clear();
    batch.mGroupFlushStrategy->SetTimeoutSecs(3);
    batch.mGroupFlushStrategy->SetMinSizeBytes(10);
    PipelineEventGroup group5 = CreateEventGroup(1);
    SourceBuffer* buffer5 = group5.GetSourceBuffer().get();
    RangeCheckpoint* eoo5 = group5.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group5), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(2U, res[0].size());
    APSARA_TEST_EQUAL(1U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer3, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo3, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);
    APSARA_TEST_EQUAL(1U, res[0][1].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][1].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][1].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][1].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer4, res[0][1].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo4, res[0][1].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][1].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);

    // flush by size
    res.clear();
    batch.mEventFlushStrategy.SetMinSizeBytes(159);
    batch.mEventFlushStrategy.SetTimeoutSecs(3);
    PipelineEventGroup group6 = CreateEventGroup(1);
    SourceBuffer* buffer6 = group6.GetSourceBuffer().get();
    batch.Add(std::move(group6), res);
    PipelineEventGroup group7 = CreateEventGroup(2);
    SourceBuffer* buffer7 = group7.GetSourceBuffer().get();
    batch.Add(std::move(group7), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(3U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(3U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer5, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(buffer6, res[0][0].mSourceBuffers[1].get());
    APSARA_TEST_EQUAL(buffer7, res[0][0].mSourceBuffers[2].get());
    APSARA_TEST_EQUAL(eoo5, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_GT(TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, key)).mUpdateTime,
                   updateTime - 1);
}

void BatcherUnittest::TestAddWithOversizedGroup() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMaxSizeBytes = 500;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 200;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy);

    vector<BatchedEventsList> res;
    PipelineEventGroup group1 = CreateEventGroup(2);
    size_t key = group1.GetTagsHash();
    batch.Add(std::move(group1), res);

    PipelineEventGroup group2 = CreateEventGroup(20);
    batch.Add(std::move(group2), res);
    APSARA_TEST_EQUAL(1U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap[key].mBatch.mEvents.size());
    APSARA_TEST_EQUAL(3U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(2U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[1].size());
    APSARA_TEST_EQUAL(13U, res[1][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[2].size());
    APSARA_TEST_EQUAL(7U, res[2][0].mEvents.size());
}

void BatcherUnittest::TestFlushEventQueueWithoutGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy);

    PipelineEventGroup group = CreateEventGroup(2);
    size_t key = group.GetTagsHash();
    SourceBuffer* buffer = group.GetSourceBuffer().get();
    RangeCheckpoint* eoo = group.GetExactlyOnceCheckpoint().get();
    vector<BatchedEventsList> tmp;
    batch.Add(std::move(group), tmp);

    BatchedEventsList res;
    // key not existed
    batch.FlushQueue(key + 1, res);
    APSARA_TEST_EQUAL(0U, res.size());

    // key existed
    batch.FlushQueue(key, res);
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(2U, res[0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer, res[0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo, res[0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0].mPackIdPrefix.data());
}

void BatcherUnittest::TestFlushEventQueueWithGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 10;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy, true);

    BatchedEventsList res;
    vector<BatchedEventsList> tmp;
    // flush to group item, but no actual output
    PipelineEventGroup group1 = CreateEventGroup(2);
    size_t key = group1.GetTagsHash();
    SourceBuffer* buffer1 = group1.GetSourceBuffer().get();
    RangeCheckpoint* eoo1 = group1.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group1), tmp);
    batch.FlushQueue(key, res);
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(0U, res.size());
    APSARA_TEST_EQUAL(1U, TimeoutFlushManager::GetInstance()->mTimeoutRecords.size());
    APSARA_TEST_EQUAL(2U, TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].size());
    TimeoutRecord& record = TimeoutFlushManager::GetInstance()->mTimeoutRecords["test_config"].at(make_pair(0, 0));
    time_t updateTime = record.mUpdateTime;
    APSARA_TEST_EQUAL(1U, record.mTimeoutSecs);
    APSARA_TEST_EQUAL(sFlusher.get(), record.mFlusher);
    APSARA_TEST_EQUAL(0U, record.mKey);
    APSARA_TEST_GT(updateTime, 0);
    APSARA_TEST_EQUAL(1U, batch.mGroupQueue->mGroups.size());

    // flush to group item, and group is flushed by time then by size
    batch.mGroupFlushStrategy->SetTimeoutSecs(0);
    batch.mGroupFlushStrategy->SetMinSizeBytes(10);
    PipelineEventGroup group2 = CreateEventGroup(2);
    SourceBuffer* buffer2 = group2.GetSourceBuffer().get();
    RangeCheckpoint* eoo2 = group2.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group2), tmp);
    batch.FlushQueue(key, res);
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(2U, res.size());
    APSARA_TEST_EQUAL(2U, res[0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer1, res[0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo1, res[0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0].mPackIdPrefix.data());
    APSARA_TEST_EQUAL(2U, res[1].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[1].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[1].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[1].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer2, res[1].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo2, res[1].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[1].mPackIdPrefix.data());
}

void BatcherUnittest::TestFlushGroupQueue() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;
    {
        // no group item
        Batcher<> batch;
        batch.Init(Json::Value(), sFlusher.get(), strategy);
        BatchedEventsList res;
        batch.FlushQueue(0, res);
        APSARA_TEST_TRUE(res.empty());
    }
    {
        // with group item
        Batcher<> batch;
        batch.Init(Json::Value(), sFlusher.get(), strategy, true);

        BatchedEventsList res;
        vector<BatchedEventsList> tmp;
        PipelineEventGroup group = CreateEventGroup(2);
        size_t key = group.GetTagsHash();
        SourceBuffer* buffer = group.GetSourceBuffer().get();
        RangeCheckpoint* eoo = group.GetExactlyOnceCheckpoint().get();
        batch.Add(std::move(group), tmp);
        batch.FlushQueue(key, res);
        APSARA_TEST_TRUE(res.empty());

        batch.FlushQueue(0, res);
        APSARA_TEST_EQUAL(1U, res.size());
        APSARA_TEST_EQUAL(2U, res[0].mEvents.size());
        APSARA_TEST_EQUAL(1U, res[0].mTags.mInner.size());
        APSARA_TEST_STREQ("val", res[0].mTags.mInner["key"].data());
        APSARA_TEST_EQUAL(1U, res[0].mSourceBuffers.size());
        APSARA_TEST_EQUAL(buffer, res[0].mSourceBuffers[0].get());
        APSARA_TEST_EQUAL(eoo, res[0].mExactlyOnceCheckpoint.get());
        APSARA_TEST_STREQ("pack_id", res[0].mPackIdPrefix.data());
    }
}

void BatcherUnittest::TestFlushAllWithoutGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy);
    PipelineEventGroup group = CreateEventGroup(2);
    SourceBuffer* buffer = group.GetSourceBuffer().get();
    RangeCheckpoint* eoo = group.GetExactlyOnceCheckpoint().get();
    vector<BatchedEventsList> tmp;
    batch.Add(std::move(group), tmp);

    vector<BatchedEventsList> res;
    batch.FlushAll(res);
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(1U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(2U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
}

void BatcherUnittest::TestFlushAllWithGroupBatch() {
    DefaultFlushStrategyOptions strategy;
    strategy.mMinCnt = 3;
    strategy.mMinSizeBytes = 1000;
    strategy.mTimeoutSecs = 3;

    Batcher<> batch;
    batch.Init(Json::Value(), sFlusher.get(), strategy, true);

    BatchedEventsList tmp1;
    vector<BatchedEventsList> tmp2;
    // flush to group item, but no actual output
    PipelineEventGroup group1 = CreateEventGroup(2);
    size_t key = group1.GetTagsHash();
    SourceBuffer* buffer1 = group1.GetSourceBuffer().get();
    RangeCheckpoint* eoo1 = group1.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group1), tmp2);
    batch.FlushQueue(key, tmp1);
    APSARA_TEST_TRUE(tmp1.empty());
    APSARA_TEST_TRUE(tmp2.empty());

    // add another group to event batch
    PipelineEventGroup group2 = CreateEventGroup(2);
    SourceBuffer* buffer2 = group2.GetSourceBuffer().get();
    RangeCheckpoint* eoo2 = group2.GetExactlyOnceCheckpoint().get();
    batch.Add(std::move(group2), tmp2);
    APSARA_TEST_TRUE(tmp2.empty());

    // flush all by time then by size
    batch.mGroupFlushStrategy->SetTimeoutSecs(0);
    batch.mGroupFlushStrategy->SetMinSizeBytes(10);
    vector<BatchedEventsList> res;
    batch.FlushAll(res);
    APSARA_TEST_EQUAL(0U, batch.mEventQueueMap.size());
    APSARA_TEST_EQUAL(2U, res.size());
    APSARA_TEST_EQUAL(1U, res[0].size());
    APSARA_TEST_EQUAL(2U, res[0][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[0][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[0][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[0][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer1, res[0][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo1, res[0][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[0][0].mPackIdPrefix.data());
    APSARA_TEST_EQUAL(1U, res[1].size());
    APSARA_TEST_EQUAL(2U, res[1][0].mEvents.size());
    APSARA_TEST_EQUAL(1U, res[1][0].mTags.mInner.size());
    APSARA_TEST_STREQ("val", res[1][0].mTags.mInner["key"].data());
    APSARA_TEST_EQUAL(1U, res[1][0].mSourceBuffers.size());
    APSARA_TEST_EQUAL(buffer2, res[1][0].mSourceBuffers[0].get());
    APSARA_TEST_EQUAL(eoo2, res[1][0].mExactlyOnceCheckpoint.get());
    APSARA_TEST_STREQ("pack_id", res[1][0].mPackIdPrefix.data());
}

void BatcherUnittest::TestMetric() {
    {
        DefaultFlushStrategyOptions strategy;
        strategy.mMinCnt = 2;
        strategy.mMinSizeBytes = 1000;
        strategy.mTimeoutSecs = 3;

        Batcher<> batch;
        batch.Init(Json::Value(), sFlusher.get(), strategy, false);

        PipelineEventGroup g = CreateEventGroup(3);
        auto groupSize = g.DataSize();
        auto batchSize = groupSize - 2 * g.GetEvents()[0]->DataSize();

        vector<BatchedEventsList> res;
        batch.Add(std::move(g), res);
        APSARA_TEST_EQUAL(6U, batch.mMetricsRecordRef->GetLabels()->size());
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PROJECT, ""));
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_PIPELINE_NAME, "test_config"));
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_COMPONENT_NAME,
                                                          METRIC_LABEL_VALUE_COMPONENT_NAME_BATCHER));
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_FLUSHER_PLUGIN_ID, "1"));
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_GROUP_BATCH_ENABLED, "false"));
        APSARA_TEST_EQUAL(3U, batch.mInEventsTotal->GetValue());
        APSARA_TEST_EQUAL(groupSize, batch.mInGroupDataSizeBytes->GetValue());
        APSARA_TEST_EQUAL(2U, batch.mOutEventsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mEventBatchItemsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mBufferedGroupsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mBufferedEventsTotal->GetValue());
        APSARA_TEST_EQUAL(batchSize, batch.mBufferedDataSizeByte->GetValue());
    }
    {
        DefaultFlushStrategyOptions strategy;
        strategy.mMinCnt = 2;
        strategy.mMinSizeBytes = 1000;
        strategy.mTimeoutSecs = 3;

        Batcher<> batch;
        batch.Init(Json::Value(), sFlusher.get(), strategy, true);

        PipelineEventGroup g = CreateEventGroup(3);
        auto groupSize = g.DataSize();
        auto batchSize = groupSize - 2 * g.GetEvents()[0]->DataSize();

        vector<BatchedEventsList> res;
        batch.Add(std::move(g), res);
        batch.FlushQueue(0, res[0]);
        APSARA_TEST_TRUE(batch.mMetricsRecordRef.HasLabel(METRIC_LABEL_KEY_GROUP_BATCH_ENABLED, "true"));
        APSARA_TEST_EQUAL(3U, batch.mInEventsTotal->GetValue());
        APSARA_TEST_EQUAL(groupSize, batch.mInGroupDataSizeBytes->GetValue());
        APSARA_TEST_EQUAL(2U, batch.mOutEventsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mEventBatchItemsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mBufferedGroupsTotal->GetValue());
        APSARA_TEST_EQUAL(1U, batch.mBufferedEventsTotal->GetValue());
        APSARA_TEST_EQUAL(batchSize, batch.mBufferedDataSizeByte->GetValue());
    }
}

PipelineEventGroup BatcherUnittest::CreateEventGroup(size_t cnt) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(string("key"), string("val"));
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    for (size_t i = 0; i < cnt; ++i) {
        group.AddLogEvent();
    }
    return group;
}

UNIT_TEST_CASE(BatcherUnittest, TestParamInit)
UNIT_TEST_CASE(BatcherUnittest, TestInitWithoutGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestInitWithGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestAddWithOversizedGroup)
UNIT_TEST_CASE(BatcherUnittest, TestAddWithoutGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestAddWithGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestFlushEventQueueWithoutGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestFlushEventQueueWithGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestFlushGroupQueue)
UNIT_TEST_CASE(BatcherUnittest, TestFlushAllWithoutGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestFlushAllWithGroupBatch)
UNIT_TEST_CASE(BatcherUnittest, TestMetric)

} // namespace logtail

UNIT_TEST_MAIN
