// Copyright 2022 iLogtail Authors
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

#include "unittest/Unittest.h"
#include "common/Flags.h"
#include "app_config/AppConfig.h"
#include "log_pb/sls_logs.pb.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "config/Config.h"

DECLARE_FLAG_INT32(logtail_checkpoint_check_gc_interval_sec);
DECLARE_FLAG_INT32(logtail_checkpoint_expired_threshold_sec);
DECLARE_FLAG_INT32(logtail_checkpoint_gc_threshold_sec);

namespace logtail {

std::string kTestRootDir;
const uint32_t kConcurrency = 3;
const std::string kPrimaryKey = "primary";
const std::string kConfigName = "config";
const std::string kLogPath = "/var/log/test.log";
const auto kDevInode = DevInode(100, 1000);

class CheckpointManagerV2Unittest : public ::testing::Test {
public:
    static void SetUpTestCase() {
        kTestRootDir = (bfs::path(GetProcessExecutionDir()) / "CheckpointManagerV2Unittest").string();
        if (bfs::exists(kTestRootDir)) {
            bfs::remove_all(kTestRootDir);
        }
        bfs::create_directories(kTestRootDir);
        AppConfig::GetInstance()->SetLogtailSysConfDir(kTestRootDir);
        INT32_FLAG(logtail_checkpoint_check_gc_interval_sec) = 1;
    }

    static void TearDownTestCase() { bfs::remove_all(kTestRootDir); }

    void TestBaseMethod();

    void TestProtobufMethod();

    void TestScanCheckpoints();

    void TestExtractPrimaryKeyFromRangeKey();

    void TestMarkGC();
};

UNIT_TEST_CASE(CheckpointManagerV2Unittest, TestBaseMethod);
UNIT_TEST_CASE(CheckpointManagerV2Unittest, TestProtobufMethod);
UNIT_TEST_CASE(CheckpointManagerV2Unittest, TestScanCheckpoints);
UNIT_TEST_CASE(CheckpointManagerV2Unittest, TestExtractPrimaryKeyFromRangeKey);
UNIT_TEST_CASE(CheckpointManagerV2Unittest, TestMarkGC);

void CheckpointManagerV2Unittest::TestBaseMethod() {
    CheckpointManagerV2 m;

    const std::string key = "test";
    const std::string value = "anyvalue";

    EXPECT_TRUE(m.write(key, value));
    std::string rValue;
    EXPECT_TRUE(m.read(key, rValue));
    EXPECT_EQ(value, rValue);
    m.DeleteCheckpoints(std::vector<std::string>{key, key + "not_exist"});
    EXPECT_FALSE(m.read(key, rValue));
}

void CheckpointManagerV2Unittest::TestProtobufMethod() {
    CheckpointManagerV2 m;

    const std::string key = "test";
    sls_logs::Log log;
    log.set_time(time(NULL));
    for (int idx = 0; idx < 10; ++idx) {
        auto content = log.add_contents();
        content->set_key(std::to_string(idx));
        content->set_value(std::to_string(idx) + "value");
    }

    EXPECT_TRUE(m.SetPB(key, log));
    sls_logs::Log rLog;
    EXPECT_TRUE(m.GetPB(key, rLog));
    EXPECT_EQ(log.time(), rLog.time());
    EXPECT_EQ(log.contents_size(), rLog.contents_size());
    for (int idx = 0; idx < log.contents_size(); ++idx) {
        EXPECT_EQ(log.contents(idx).key(), rLog.contents(idx).key());
        EXPECT_EQ(log.contents(idx).value(), rLog.contents(idx).value());
    }
}

// Test checkpoints returned by ScanCheckpoints.
void CheckpointManagerV2Unittest::TestScanCheckpoints() {
    Config config;
    config.mConfigName = kConfigName;

    CheckpointManagerV2 m;
    std::string ignoreCptValue;

    // Valid checkpoints.
    {
        m.rebuild();

        PrimaryCheckpointPB cpt;
        cpt.set_concurrency(kConcurrency);
        cpt.set_config_name(kConfigName);
        cpt.set_sig_hash(0);
        cpt.set_sig_size(0);
        cpt.set_log_path(kLogPath);
        cpt.set_dev(kDevInode.dev);
        cpt.set_inode(kDevInode.inode);
        cpt.set_update_time(time(NULL));
        EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            RangeCheckpointPB rgCpt;
            rgCpt.set_hash_key(kPrimaryKey + std::to_string(idx));
            rgCpt.set_sequence_id(0);
            rgCpt.set_read_offset(0);
            rgCpt.set_read_length(0);
            rgCpt.set_update_time(time(NULL));
            rgCpt.set_committed(false);
            EXPECT_TRUE(m.SetPB(m.MakeRangeKey(kPrimaryKey, idx), rgCpt));
        }

        auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
        EXPECT_EQ(checkpoints.size(), 1);
        EXPECT_EQ(checkpoints[0].second.DebugString(), cpt.DebugString());
    }

    // Missing primary checkpoint.
    {
        m.rebuild();

        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            RangeCheckpointPB rgCpt;
            rgCpt.set_hash_key(kPrimaryKey + std::to_string(idx));
            rgCpt.set_sequence_id(0);
            rgCpt.set_read_offset(0);
            rgCpt.set_read_length(0);
            rgCpt.set_update_time(time(NULL));
            rgCpt.set_committed(false);
            EXPECT_TRUE(m.SetPB(m.MakeRangeKey(kPrimaryKey, idx), rgCpt));
        }

        auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
        EXPECT_EQ(checkpoints.size(), 0);
        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            EXPECT_FALSE(m.read(m.MakeRangeKey(kPrimaryKey, idx), ignoreCptValue));
        }
    }

    // Broken primary checkpoint.
    {
        m.rebuild();

        EXPECT_TRUE(m.write(kPrimaryKey, "broken value"));

        auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
        EXPECT_EQ(checkpoints.size(), 0);
        EXPECT_FALSE(m.read(kPrimaryKey, ignoreCptValue));
    }

    // Unmatched config name.
    {
        m.rebuild();

        PrimaryCheckpointPB cpt;
        cpt.set_concurrency(kConcurrency);
        cpt.set_config_name(kConfigName + "_unmatched");
        cpt.set_sig_hash(0);
        cpt.set_sig_size(0);
        cpt.set_log_path(kLogPath);
        cpt.set_dev(kDevInode.dev);
        cpt.set_inode(kDevInode.inode);
        cpt.set_update_time(time(NULL));
        EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));

        auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
        EXPECT_EQ(checkpoints.size(), 0);
        EXPECT_FALSE(m.read(kPrimaryKey, ignoreCptValue));
    }

    // No range checkpoint.
    {
        m.rebuild();

        PrimaryCheckpointPB cpt;
        cpt.set_concurrency(kConcurrency);
        cpt.set_config_name(kConfigName);
        cpt.set_sig_hash(0);
        cpt.set_sig_size(0);
        cpt.set_log_path(kLogPath);
        cpt.set_dev(kDevInode.dev);
        cpt.set_inode(kDevInode.inode);

        // Primary checkpoint has not been expired.
        {
            cpt.set_update_time(time(NULL));
            EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
            auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
            EXPECT_EQ(checkpoints.size(), 1);
            EXPECT_TRUE(m.read(kPrimaryKey, ignoreCptValue));
        }

        // Primary checkpoint has been expired.
        {
            cpt.set_update_time(time(NULL) - 100 - INT32_FLAG(logtail_checkpoint_expired_threshold_sec));
            EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
            auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
            EXPECT_EQ(checkpoints.size(), 0);
            EXPECT_FALSE(m.read(kPrimaryKey, ignoreCptValue));
        }
    }

    // Outdated range checkpoints.
    {
        m.rebuild();

        PrimaryCheckpointPB cpt;
        cpt.set_concurrency(kConcurrency);
        cpt.set_config_name(kConfigName);
        cpt.set_sig_hash(0);
        cpt.set_sig_size(0);
        cpt.set_log_path(kLogPath);
        cpt.set_dev(kDevInode.dev);
        cpt.set_inode(kDevInode.inode);
        const auto outdatedUpdateTime = time(NULL) - 100 - INT32_FLAG(logtail_checkpoint_expired_threshold_sec);
        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            RangeCheckpointPB rgCpt;
            rgCpt.set_hash_key(kPrimaryKey + std::to_string(idx));
            rgCpt.set_sequence_id(0);
            rgCpt.set_read_offset(0);
            rgCpt.set_read_length(0);
            rgCpt.set_update_time(outdatedUpdateTime);
            rgCpt.set_committed(false);
            EXPECT_TRUE(m.SetPB(m.MakeRangeKey(kPrimaryKey, idx), rgCpt));
        }

        // Primary checkpoint has not been expired.
        {
            cpt.set_update_time(time(NULL));
            EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
            auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
            EXPECT_EQ(checkpoints.size(), 1);
            EXPECT_TRUE(m.read(kPrimaryKey, ignoreCptValue));
            for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
                EXPECT_TRUE(m.read(m.MakeRangeKey(kPrimaryKey, idx), ignoreCptValue));
            }
        }

        // Primary checkpoint has been expired.
        {
            cpt.set_update_time(outdatedUpdateTime);
            EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
            auto checkpoints = m.ScanCheckpoints(std::vector<Config*>{&config});
            EXPECT_EQ(checkpoints.size(), 0);
            EXPECT_FALSE(m.read(kPrimaryKey, ignoreCptValue));
            for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
                EXPECT_FALSE(m.read(m.MakeRangeKey(kPrimaryKey, idx), ignoreCptValue));
            }
        }
    }
}

namespace detail {

    std::string extractPrimaryKeyFromRangeKey(const char* data, size_t size);

}

void CheckpointManagerV2Unittest::TestExtractPrimaryKeyFromRangeKey() {
    const std::string kPrimaryKey = "config-file_path-100-1000";
    const std::string kRangeKey = CheckpointManagerV2::MakeRangeKey(kPrimaryKey, 0);
    EXPECT_EQ(kPrimaryKey, detail::extractPrimaryKeyFromRangeKey(kRangeKey.data(), kRangeKey.size()));
}

void CheckpointManagerV2Unittest::TestMarkGC() {
    CheckpointManagerV2 m;
    auto initDatabase = [&]() {
        m.rebuild();

        PrimaryCheckpointPB cpt;
        cpt.set_concurrency(kConcurrency);
        cpt.set_sig_hash(0);
        cpt.set_sig_size(0);
        EXPECT_TRUE(m.SetPB(kPrimaryKey, cpt));
        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            RangeCheckpointPB rgCpt;
            rgCpt.set_hash_key(kPrimaryKey + std::to_string(idx));
            rgCpt.set_sequence_id(0);
            rgCpt.set_read_offset(0);
            rgCpt.set_read_length(0);
            rgCpt.set_update_time(time(NULL));
            rgCpt.set_committed(false);
            EXPECT_TRUE(m.SetPB(m.MakeRangeKey(kPrimaryKey, idx), rgCpt));
        }
    };

    {
        initDatabase();

        auto bakGCThreshold = INT32_FLAG(logtail_checkpoint_gc_threshold_sec);
        INT32_FLAG(logtail_checkpoint_gc_threshold_sec) = 1;

        m.MarkGC(kPrimaryKey);
        sleep(2);
        std::string ignoreValue;
        EXPECT_FALSE(m.read(kPrimaryKey, ignoreValue));
        for (uint32_t idx = 0; idx < kConcurrency; ++idx) {
            EXPECT_FALSE(m.read(m.MakeRangeKey(kPrimaryKey, idx), ignoreValue));
        }

        INT32_FLAG(logtail_checkpoint_gc_threshold_sec) = bakGCThreshold;
    }

    {
        initDatabase();

        m.MarkGC(kPrimaryKey);
        {
            std::lock_guard<std::mutex> lock(m.mMutex);
            EXPECT_TRUE(m.mGCItems.find(kPrimaryKey) != m.mGCItems.end());
        }
        sleep(3);

        std::string ignoreValue;
        EXPECT_TRUE(m.read(kPrimaryKey, ignoreValue));
        {
            std::lock_guard<std::mutex> lock(m.mMutex);
            EXPECT_FALSE(m.mGCItems.find(kPrimaryKey) != m.mGCItems.end());
        }
    }
}

} // namespace logtail

UNIT_TEST_MAIN
