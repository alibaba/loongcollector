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

#include "pipeline/serializer/SLSSerializer.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "unittest/Unittest.h"

DECLARE_FLAG_INT32(max_send_log_group_size);

using namespace std;

namespace logtail {

class SLSSerializerUnittest : public ::testing::Test {
public:
    void TestSerializeEventGroup();
    void TestSerializeEventGroupList();

protected:
    static void SetUpTestCase() { sFlusher = make_unique<FlusherSLS>(); }

    void SetUp() override {
        mCtx.SetConfigName("test_config");
        sFlusher->SetContext(mCtx);
        sFlusher->SetMetricsRecordRef(FlusherSLS::sName, "1");
    }

private:
    BatchedEvents CreateBatchedLogEvents(bool enableNanosecond, bool emptyContent);
    BatchedEvents
    CreateBatchedMetricEvents(bool enableNanosecond, uint32_t nanoTimestamp, bool emptyValue, bool onlyOneTag);
    BatchedEvents CreateBatchedRawEvents(bool enableNanosecond, bool emptyContent);

    static unique_ptr<FlusherSLS> sFlusher;

    PipelineContext mCtx;
};

unique_ptr<FlusherSLS> SLSSerializerUnittest::sFlusher;

void SLSSerializerUnittest::TestSerializeEventGroup() {
    SLSEventGroupSerializer serializer(sFlusher.get());
    {
        // log
        {
            // nano second disabled, and set
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedLogEvents(false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_STREQ("key", logGroup.logs(0).contents(0).key().c_str());
            APSARA_TEST_STREQ("value", logGroup.logs(0).contents(0).value().c_str());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // nano second enabled, and set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedLogEvents(true, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1U, logGroup.logs(0).time_ns());
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = false;
        }
        {
            // nano second enabled, not set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedLogEvents(false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = false;
        }
        {
            // empty log content
            string res, errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(CreateBatchedLogEvents(false, true), res, errorMsg));
        }
    }
    {
        // metric
        {
            // only 1 tag
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedMetricEvents(false, 0, false, true), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));

            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(logGroup.logs(0).contents_size(), 4);
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).key(), "__labels__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).value(), "key1#$#value1");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).key(), "__time_nano__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).value(), "1234567890");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).key(), "__value__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).value(), "0.100000");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).key(), "__name__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).value(), "test_gauge");
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // nano second disabled
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedMetricEvents(false, 0, false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));

            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(logGroup.logs(0).contents_size(), 4);
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).key(), "__labels__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).value(), "key1#$#value1|key2#$#value2");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).key(), "__time_nano__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).value(), "1234567890");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).key(), "__value__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).value(), "0.100000");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).key(), "__name__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).value(), "test_gauge");
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // nano second enabled, less than 9 digits
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedMetricEvents(true, 1, false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));

            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(logGroup.logs(0).contents_size(), 4);
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).key(), "__labels__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).value(), "key1#$#value1|key2#$#value2");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).key(), "__time_nano__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).value(), "1234567890000000001");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).key(), "__value__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).value(), "0.100000");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).key(), "__name__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).value(), "test_gauge");
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // nano second enabled, exactly 9 digits
            string res, errorMsg;
            APSARA_TEST_TRUE(
                serializer.DoSerialize(CreateBatchedMetricEvents(true, 999999999, false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));

            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(logGroup.logs(0).contents_size(), 4);
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).key(), "__labels__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(0).value(), "key1#$#value1|key2#$#value2");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).key(), "__time_nano__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(1).value(), "1234567890999999999");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).key(), "__value__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(2).value(), "0.100000");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).key(), "__name__");
            APSARA_TEST_EQUAL(logGroup.logs(0).contents(3).value(), "test_gauge");
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // empty metric value
            string res, errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(CreateBatchedMetricEvents(false, 0, true, false), res, errorMsg));
        }
    }
    {
        // span
    }
    {
        // raw
        {
            // nano second disabled, and set
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedRawEvents(false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1, logGroup.logs_size());
            APSARA_TEST_EQUAL(1, logGroup.logs(0).contents_size());
            APSARA_TEST_STREQ("content", logGroup.logs(0).contents(0).key().c_str());
            APSARA_TEST_STREQ("value", logGroup.logs(0).contents(0).value().c_str());
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            APSARA_TEST_EQUAL(1, logGroup.logtags_size());
            APSARA_TEST_STREQ("__pack_id__", logGroup.logtags(0).key().c_str());
            APSARA_TEST_STREQ("pack_id", logGroup.logtags(0).value().c_str());
            APSARA_TEST_STREQ("machine_uuid", logGroup.machineuuid().c_str());
            APSARA_TEST_STREQ("source", logGroup.source().c_str());
            APSARA_TEST_STREQ("topic", logGroup.topic().c_str());
        }
        {
            // nano second enabled, and set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedRawEvents(true, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_EQUAL(1U, logGroup.logs(0).time_ns());
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = false;
        }
        {
            // nano second enabled, not set
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = true;
            string res, errorMsg;
            APSARA_TEST_TRUE(serializer.DoSerialize(CreateBatchedRawEvents(false, false), res, errorMsg));
            sls_logs::LogGroup logGroup;
            APSARA_TEST_TRUE(logGroup.ParseFromString(res));
            APSARA_TEST_EQUAL(1234567890U, logGroup.logs(0).time());
            APSARA_TEST_FALSE(logGroup.logs(0).has_time_ns());
            const_cast<GlobalConfig&>(mCtx.GetGlobalConfig()).mEnableTimestampNanosecond = false;
        }
        {
            // empty log content
            string res, errorMsg;
            APSARA_TEST_FALSE(serializer.DoSerialize(CreateBatchedRawEvents(false, true), res, errorMsg));
        }
    }
    {
        // log group exceed size limit
        INT32_FLAG(max_send_log_group_size) = 0;
        string res, errorMsg;
        APSARA_TEST_FALSE(serializer.DoSerialize(CreateBatchedLogEvents(true, false), res, errorMsg));
        INT32_FLAG(max_send_log_group_size) = 10 * 1024 * 1024;
    }
    {
        // empty log group
        PipelineEventGroup group(make_shared<SourceBuffer>());
        BatchedEvents batch(std::move(group.MutableEvents()),
                            std::move(group.GetSizedTags()),
                            std::move(group.GetSourceBuffer()),
                            group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                            std::move(group.GetExactlyOnceCheckpoint()));
        string res, errorMsg;
        APSARA_TEST_FALSE(serializer.DoSerialize(std::move(batch), res, errorMsg));
    }
}

void SLSSerializerUnittest::TestSerializeEventGroupList() {
    vector<CompressedLogGroup> v;
    v.emplace_back("data1", 10);

    SLSEventGroupListSerializer serializer(sFlusher.get());
    string res, errorMsg;
    APSARA_TEST_TRUE(serializer.DoSerialize(std::move(v), res, errorMsg));
    sls_logs::SlsLogPackageList logPackageList;
    APSARA_TEST_TRUE(logPackageList.ParseFromString(res));
    APSARA_TEST_EQUAL(1, logPackageList.packages_size());
    APSARA_TEST_STREQ("data1", logPackageList.packages(0).data().c_str());
    APSARA_TEST_EQUAL(10, logPackageList.packages(0).uncompress_size());
    APSARA_TEST_EQUAL(sls_logs::SlsCompressType::SLS_CMP_NONE, logPackageList.packages(0).compress_type());
}


BatchedEvents SLSSerializerUnittest::CreateBatchedLogEvents(bool enableNanosecond, bool emptyContent) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "machine_uuid");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "pack_id");
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    LogEvent* e = group.AddLogEvent();
    if (!emptyContent) {
        e->SetContent(string("key"), string("value"));
    }
    if (enableNanosecond) {
        e->SetTimestamp(1234567890, 1);
    } else {
        e->SetTimestamp(1234567890);
    }
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}


BatchedEvents SLSSerializerUnittest::CreateBatchedMetricEvents(bool enableNanosecond,
                                                               uint32_t nanoTimestamp,
                                                               bool emptyValue,
                                                               bool onlyOneTag) {
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
        double value = 0.1;
        e->SetValue<UntypedSingleValue>(value);
    }
    e->SetName("test_gauge");
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

BatchedEvents SLSSerializerUnittest::CreateBatchedRawEvents(bool enableNanosecond, bool emptyContent) {
    PipelineEventGroup group(make_shared<SourceBuffer>());
    group.SetTag(LOG_RESERVED_KEY_TOPIC, "topic");
    group.SetTag(LOG_RESERVED_KEY_SOURCE, "source");
    group.SetTag(LOG_RESERVED_KEY_MACHINE_UUID, "machine_uuid");
    group.SetTag(LOG_RESERVED_KEY_PACKAGE_ID, "pack_id");
    StringBuffer b = group.GetSourceBuffer()->CopyString(string("pack_id"));
    group.SetMetadataNoCopy(EventGroupMetaKey::SOURCE_ID, StringView(b.data, b.size));
    group.SetExactlyOnceCheckpoint(RangeCheckpointPtr(new RangeCheckpoint));
    RawEvent* e = group.AddRawEvent();
    if (!emptyContent) {
        e->SetContent(string("value"));
    }
    if (enableNanosecond) {
        e->SetTimestamp(1234567890, 1);
    } else {
        e->SetTimestamp(1234567890);
    }
    BatchedEvents batch(std::move(group.MutableEvents()),
                        std::move(group.GetSizedTags()),
                        std::move(group.GetSourceBuffer()),
                        group.GetMetadata(EventGroupMetaKey::SOURCE_ID),
                        std::move(group.GetExactlyOnceCheckpoint()));
    return batch;
}

UNIT_TEST_CASE(SLSSerializerUnittest, TestSerializeEventGroup)
UNIT_TEST_CASE(SLSSerializerUnittest, TestSerializeEventGroupList)

} // namespace logtail

UNIT_TEST_MAIN
