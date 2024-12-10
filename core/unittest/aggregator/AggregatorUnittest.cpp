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
#include "unittest/Unittest.h"
#include "Aggregator.h"
#include "flusher/FlusherSLS.h"
#include "app_config/AppConfig.h"


DECLARE_FLAG_INT32(batch_send_interval);
DECLARE_FLAG_INT32(merge_log_count_limit);
DECLARE_FLAG_DOUBLE(loggroup_bytes_inflation);

namespace logtail {
class AggregatorUnittest : public ::testing::Test {
public:

    

    void SetUp() override {
        AppConfig::GetInstance()->mMaxHoldedDataSize = 20 * 1024 * 1024;
    }

    void TearDown() override {
        Aggregator* aggregator = Aggregator::GetInstance();
        aggregator->mPackageListMergeMap.clear();
        aggregator->mMergeMap.clear();
        aggregator->mSendVectorSize = 0;
    }
    void TestLogstoreMergeTypeAdd();
    void TestLogstoreMergeTypeAddLargeGroup();
    void TestTopicMergeTypeAdd();
private:
};

APSARA_UNIT_TEST_CASE(AggregatorUnittest, TestLogstoreMergeTypeAdd, 0);
APSARA_UNIT_TEST_CASE(AggregatorUnittest, TestLogstoreMergeTypeAddLargeGroup, 1);
APSARA_UNIT_TEST_CASE(AggregatorUnittest, TestTopicMergeTypeAdd, 2);


void AggregatorUnittest::TestLogstoreMergeTypeAdd() {

    std::string projectName = "testProject";
    std::string sourceId = "test";
    std::string logstore = "testLogstore";
    
    INT32_FLAG(batch_send_interval) = 2;
    DOUBLE_FLAG(loggroup_bytes_inflation) = 1.2;

    int64_t logGroupKey = 123;
    
    std::unique_ptr<FlusherSLS> flusher;
    PipelineContext ctx;
    ctx.SetConfigName("test_config");
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");

    FlusherSLS::Batch::MergeType mergeType = FlusherSLS::Batch::MergeType::LOGSTORE;
    std::string defaultRegion = "testRegion";
    std::string filename = "testFile";
    LogGroupContext context = LogGroupContext();

    Aggregator* aggregator = Aggregator::GetInstance();
    int count = 10;
    for (int i = 0; i < count; i ++) {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);

        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
        APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 0);
        APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 1);
        APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 0);
    }

    // mPackageListMergeMap key
    int64_t logstoreKey = HashString(projectName + "_" + logstore);
    std::unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter;
    pIter = aggregator->mPackageListMergeMap.find(logstoreKey); 
    APSARA_TEST_NOT_EQUAL(pIter, aggregator->mPackageListMergeMap.end());
    if (pIter != aggregator->mPackageListMergeMap.end()) {
        APSARA_TEST_EQUAL((pIter->second)->mMergeItems.size(), 10);
    }

    APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 1);
    APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 0);

    // sleep until PackageListMergeBuffer::IsReady
    sleep(5);

    {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);
        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
    }
    // the 10 old logs and 1 new log will be added to sendDataVec
    APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 11);
    APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 0);
    APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 0);

    for (int i = 0; i < count; i ++) {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);
        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
        APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 0);
        APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 1);
        APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 0);
    }

    pIter = aggregator->mPackageListMergeMap.find(logstoreKey); 
    APSARA_TEST_NOT_EQUAL(pIter, aggregator->mPackageListMergeMap.end());
    if (pIter != aggregator->mPackageListMergeMap.end()) {
        APSARA_TEST_EQUAL((pIter->second)->mMergeItems.size(), 10);
    }
}

void AggregatorUnittest::TestLogstoreMergeTypeAddLargeGroup() {
    std::string projectName = "testProject";
    std::string sourceId = "test";
    std::string logstore = "testLogstore";
    
    INT32_FLAG(batch_send_interval) = 2;
    DOUBLE_FLAG(loggroup_bytes_inflation) = 1.2;

    int64_t logGroupKey = 123;
    
    std::unique_ptr<FlusherSLS> flusher;
    PipelineContext ctx;
    ctx.SetConfigName("test_config");
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");

    FlusherSLS::Batch::MergeType mergeType = FlusherSLS::Batch::MergeType::LOGSTORE;
    std::string defaultRegion = "testRegion";
    std::string filename = "testFile";
    LogGroupContext context = LogGroupContext();

    // mPackageListMergeMap key
    int64_t logstoreKey = HashString(projectName + "_" + logstore);

    Aggregator* aggregator = Aggregator::GetInstance();

    {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        
        std::string key = "testKey";
        std::string value = "testValue";
        // 7 + 9 + 5 = 21
        uint32_t logByteSize = key.size() + value.size() + 5;

        AppConfig::GetInstance()->mMaxHoldedDataSize = 1024;

        int count = 1024/logByteSize + 1;

        uint32_t logGroupSize = 0;

        // int32_t logSize = (int32_t)logGroup.logs_size();
        // int32_t logByteSize = logGroupSize / logSize;
        // value->mRawBytes > logGroupByteMin
        // int32_t logGroupByteMin = AppConfig::GetInstance()->GetMaxHoldedDataSize() > logByteSize
        //    ? (AppConfig::GetInstance()->GetMaxHoldedDataSize() - logByteSize)
        //    : 0;
        for (int i = 0; i < count; i ++) {
            sls_logs::Log* log = logGroup.add_logs();
            log->set_time(time(NULL));
            sls_logs::Log_Content* content = nullptr;
            content = log->add_contents();
            
            content->set_key(key);
            content->set_value(value);

            logGroupSize += logByteSize;
        }        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
        
        std::unordered_map<int64_t, PackageListMergeBuffer*>::iterator pIter;
        pIter = aggregator->mPackageListMergeMap.find(logstoreKey); 
        APSARA_TEST_NOT_EQUAL(pIter, aggregator->mPackageListMergeMap.end());
        if (pIter != aggregator->mPackageListMergeMap.end()) {
            APSARA_TEST_EQUAL((pIter->second)->mMergeItems.size(), 2);
        }

        int logCount = 0;
        for (auto item : (pIter->second)->mMergeItems) {
            logCount += (item->mLogGroup).logs_size();
        }
        APSARA_TEST_EQUAL(logCount, count);
    }

}


void AggregatorUnittest::TestTopicMergeTypeAdd() {

    std::string projectName = "testProject";
    std::string sourceId = "test";
    std::string logstore = "testLogstore";


    int64_t logGroupKey = 123;
    INT32_FLAG(merge_log_count_limit) = 10;
    
    std::unique_ptr<FlusherSLS> flusher;
    PipelineContext ctx;
    ctx.SetConfigName("test_config");
    flusher.reset(new FlusherSLS());
    flusher->SetContext(ctx);
    flusher->SetMetricsRecordRef(FlusherSLS::sName, "1");

    FlusherSLS::Batch::MergeType mergeType = FlusherSLS::Batch::MergeType::TOPIC;
    std::string defaultRegion = "testRegion";
    std::string filename = "testFile";
    LogGroupContext context = LogGroupContext();

    Aggregator* aggregator = Aggregator::GetInstance();
    int count = 10;

    int32_t curTime = time(NULL);
    for (int i = 0; i < count; i ++) {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(curTime);
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);
        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
        APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 0);
        APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 0);
        APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 1);
    }
    std::unordered_map<int64_t, MergeItem*>::iterator itr = aggregator->mMergeMap.find(logGroupKey);
    APSARA_TEST_NOT_EQUAL(itr, aggregator->mMergeMap.end());
    if (itr != aggregator->mMergeMap.end()) {
        APSARA_TEST_EQUAL((itr->second)->mLogGroup.logs_size(), 10);
    }

    {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(time(NULL));
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);
        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
    }
    // the 10 old logs will merge to 1, and will be added to sendDataVec
    APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 1);
    APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 0);

    // the 1 new log will keey in mMergeMap
    APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 1);
    itr = aggregator->mMergeMap.find(logGroupKey);
    APSARA_TEST_NOT_EQUAL(itr, aggregator->mMergeMap.end());
    if (itr != aggregator->mMergeMap.end()) {
        APSARA_TEST_EQUAL((itr->second)->mLogGroup.logs_size(), 1);
    }

    // add 9 more log 
    curTime = time(NULL);
    for (int i = 0; i < 9; i ++) {
        sls_logs::LogGroup logGroup;
        logGroup.set_category(logstore);
        
        sls_logs::LogTag* logTag = logGroup.add_logtags();
        logTag->set_key("testKey");
        logTag->set_value("testValue");
        sls_logs::Log* log = logGroup.add_logs();
        log->set_time(curTime);
        sls_logs::Log_Content* content = nullptr;
        content = log->add_contents();
        std::string key = "testKey";
        std::string value = "testValue";
        content->set_key(key);
        content->set_value(value);
        uint32_t logGroupSize = (key.size() + value.size() + 5)*DOUBLE_FLAG(loggroup_bytes_inflation);
        
        aggregator->Add(
            projectName, sourceId, logGroup, logGroupKey, flusher.get(), mergeType, logGroupSize, defaultRegion, filename, context);
        APSARA_TEST_EQUAL(aggregator->mSendVectorSize, 0);
        APSARA_TEST_EQUAL(aggregator->mPackageListMergeMap.size(), 0);
        APSARA_TEST_EQUAL(aggregator->mMergeMap.size(), 1);
    }
    itr = aggregator->mMergeMap.find(logGroupKey);
    APSARA_TEST_NOT_EQUAL(itr, aggregator->mMergeMap.end());

    // mMergeMap should keep 10 logs
    if (itr != aggregator->mMergeMap.end()) {
        APSARA_TEST_EQUAL((itr->second)->mLogGroup.logs_size(), 10);
    }
}
}

int main(int argc, char** argv) {
    logtail::Logger::Instance().InitGlobalLoggers();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}