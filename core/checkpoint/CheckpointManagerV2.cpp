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

#include "CheckpointManagerV2.h"
#include <leveldb/write_batch.h>
#include <boost/filesystem.hpp>
#include "common/Flags.h"
#include "common/ScopeInvoker.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "profiler/LogtailAlarm.h"
#include "app_config/AppConfig.h"
#include "config/Config.h"
#include "config_manager/ConfigManager.h"
#include "checkpoint/CheckPointManager.h"

DEFINE_FLAG_INT32(logtail_checkpoint_check_gc_interval_sec, "60 seconds", 60);
DEFINE_FLAG_INT32(logtail_checkpoint_gc_threshold_sec, "30 minutes", 30 * 60);
DEFINE_FLAG_DOUBLE(logtail_checkpoint_max_gc_count_ratio_per_round, "10%", 0.1);
DEFINE_FLAG_INT64(logtail_checkpoint_max_used_time_per_round_in_msec, "500ms", 500);
DEFINE_FLAG_INT32(logtail_checkpoint_expired_threshold_sec, "6 hours", 6 * 60 * 60);

namespace logtail {

namespace detail {

    std::string getDatabasePath() {
        auto fp = boost::filesystem::path(AppConfig::GetInstance()->GetLogtailSysConfDir());
        return (fp / "checkpoint_v2").string();
    }

    // Log error locally and send alarm.
    void logDatabaseError(const std::string& op, const std::string& key, const leveldb::Status& s) {
        const std::string title = "error when access checkpoint database";
        std::string msg;
        msg.append("op:").append(op).append(", key:").append(key).append(", status:").append(s.ToString());
        LOG_ERROR(sLogger, (title, msg));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_V2_ALARM, title + ", " + msg);
    }

    // Range key is represented by data pointer and size to avoid copy.
    //
    // @return empty if failed to extract.
    std::string extractPrimaryKeyFromRangeKey(const char* data, size_t size) {
        const int32_t kExepectedCount = 2;
        int32_t underlineCount = 0;
        for (; size > 0; --size) {
            if (data[size - 1] != '_') {
                continue;
            }
            if (++underlineCount == kExepectedCount) {
                break;
            }
        }
        if (size > 2) {
            return std::string(data, size - 1);
        }
        return "";
    }

    void makeRangeKey(std::string& key, uint32_t idx) { key.append("_").append(std::to_string(idx)).append("_r"); }

    bool isRangeKey(const char* data, size_t len) { return len > 0 && data[len - 1] == 'r'; }

} // namespace detail

#define ASSERT_LEVELDB_STATUS \
    do { \
        if (nullptr == mDatabase) { \
            return false; \
        } \
    } while (0)

std::string CheckpointManagerV2::MakeRangeKey(const std::string& primaryKey, uint32_t idx) {
    std::string key = primaryKey;
    detail::makeRangeKey(key, idx);
    return key;
}

CheckpointManagerV2::CheckpointManagerV2() {
    mDefaultWriteOption.sync = AppConfig::GetInstance()->EnableCheckpointSyncWrite();

    if (open()) {
        mGCThreadPtr.reset(new std::thread([&]() { runGCLoop(); }));
    }
}

CheckpointManagerV2::~CheckpointManagerV2() {
    mStopGCThread = true;
    if (mGCThreadPtr) {
        mGCThreadPtr->join();
        mGCThreadPtr.reset();
    }

    close();
}

void CheckpointManagerV2::AppendRangeKeys(const std::string& primaryKey,
                                          uint32_t rangeCptCount,
                                          std::vector<std::string>& keys) {
    std::string rangeKey = primaryKey;
    for (uint32_t idx = 0; idx < rangeCptCount; ++idx) {
        rangeKey.resize(primaryKey.length());
        detail::makeRangeKey(rangeKey, idx);
        keys.push_back(rangeKey);
    }
}

void CheckpointManagerV2::appendCheckpointKeys(const std::string& primaryKey,
                                               uint32_t rgCptCount,
                                               std::vector<std::string>& keys) {
    keys.push_back(primaryKey);
    AppendRangeKeys(primaryKey, rgCptCount, keys);
}

int64_t CheckpointManagerV2::scanCheckpoints(const std::vector<Config*>& exactlyOnceConfigs,
                                             std::vector<std::pair<std::string, PrimaryCheckpointPB>>* checkpoints,
                                             std::vector<std::string>& shouldDeleteCptKeys,
                                             uint64_t limitScanTimeInMs) {
    bool doFullScan = !exactlyOnceConfigs.empty();
    if (doFullScan) {
        limitScanTimeInMs = 0;
    }
    shouldDeleteCptKeys.clear();

    std::set<std::string> configNameSet;
    for (auto& cfg : exactlyOnceConfigs) {
        configNameSet.insert(cfg->mConfigName);
    }

    leveldb::ReadOptions options;
    options.snapshot = mDatabase->GetSnapshot();
    auto iter = mDatabase->NewIterator(options);
    ScopeInvoker invoker([&]() {
        delete iter;
        mDatabase->ReleaseSnapshot(options.snapshot);
    });

    static std::string sLastScannedKey;
    auto initIterator = [&]() {
        if (doFullScan || sLastScannedKey.empty()) {
            iter->SeekToFirst();
        } else {
            iter->Seek(sLastScannedKey);
            if (!iter->Valid()) {
                iter->SeekToFirst();
            }
        }
    };
    auto const scanStartTime = GetCurrentTimeInMilliSeconds();
    int64_t scannedCount = 0;
    auto canAccessIterator = [&]() {
        if (doFullScan || limitScanTimeInMs == 0) {
            return iter->Valid();
        }
        auto r = (GetCurrentTimeInMilliSeconds() - scanStartTime > limitScanTimeInMs) ? false : iter->Valid();
        if (!r) {
            LOG_DEBUG(sLogger, ("scanned count", scannedCount));
        }
        return r;
    };
    auto forwardIterator = [&]() {
        if (!doFullScan) {
            sLastScannedKey.assign(iter->key().data(), iter->key().size());
        }
        ++scannedCount;
        iter->Next();
    };

    std::unordered_set<std::string> missingPrimaryKeysCache;
    for (initIterator(); canAccessIterator(); forwardIterator()) {
        const auto& key = iter->key();

        // Range key: check if the primary key still alives, if not, delete.
        if (detail::isRangeKey(key.data(), key.size())) {
            auto primaryKey = detail::extractPrimaryKeyFromRangeKey(key.data(), key.size());
            if (primaryKey.empty()) {
                LOG_ERROR(sLogger, ("invalid range checkpoint, delete", key.ToString()));
                shouldDeleteCptKeys.push_back(key.ToString());
                continue;
            }
            bool missing = false;
            auto iter = missingPrimaryKeysCache.find(primaryKey);
            std::string ignoreVal;
            if (iter != missingPrimaryKeysCache.end()) {
                missing = true;
            } else if (!readDatabase(primaryKey, ignoreVal)) {
                const size_t kMissingCacheSize = 100;
                missing = true;
                if (missingPrimaryKeysCache.size() >= kMissingCacheSize) {
                    missingPrimaryKeysCache.erase(missingPrimaryKeysCache.begin());
                }
                missingPrimaryKeysCache.insert(primaryKey);
            }
            if (missing) {
                LOG_ERROR(sLogger, ("primary key is missing, delete", key.ToString()));
                shouldDeleteCptKeys.push_back(key.ToString());
            }
            continue;
        }

        // Read primary checkpoint and validate it.
        PrimaryCheckpointPB cpt;
        if (!cpt.ParseFromArray(iter->value().data(), iter->value().size())) {
            LOG_ERROR(sLogger, ("parse primary checkpoint error, delete", key.ToString()));
            appendCheckpointKeys(key.ToString(), Config::kExactlyOnceMaxConcurrency, shouldDeleteCptKeys);
            continue;
        }

        // Only full scan should validate config and v1 checkpoint.
        if (doFullScan) {
            if (configNameSet.find(cpt.config_name()) == configNameSet.end()) {
                LOG_INFO(sLogger, ("config name not found, delete", key.ToString())("checkpoint", cpt.DebugString()));
                appendCheckpointKeys(key.ToString(), cpt.concurrency(), shouldDeleteCptKeys);
                continue;
            }
            static auto sV1CptM = CheckPointManager::Instance();
            {
                CheckPointPtr v1Cpt;
                if (sV1CptM->GetCheckPoint(DevInode(cpt.dev(), cpt.inode()), cpt.config_name(), v1Cpt)) {
                    LOG_DEBUG(sLogger, ("existing v1 checkpoint, skip", key.ToString()));
                    continue;
                }
            }
        }

        // Validate range checkpoints to confirm if the checkpoint should load.
        // When primary checkpoint and all of its range checkpoint expire, delete them.
        auto curTime = time(NULL);
        if (curTime - cpt.update_time() >= INT32_FLAG(logtail_checkpoint_expired_threshold_sec)) {
            int32_t aliveRangeCptCount = 0;
            std::vector<std::string> rangeCptKeys;
            AppendRangeKeys(key.ToString(), cpt.concurrency(), rangeCptKeys);
            RangeCheckpointPB rgCpt;
            for (auto& rangeKey : rangeCptKeys) {
                if (!GetPB(rangeKey, rgCpt)) {
                    continue;
                }
                if (curTime - rgCpt.update_time() >= INT32_FLAG(logtail_checkpoint_expired_threshold_sec)) {
                    LOG_DEBUG(sLogger, ("range checkpoint expired", rangeKey)("update time", rgCpt.update_time()));
                    continue;
                }
                ++aliveRangeCptCount;
            }
            if (0 == aliveRangeCptCount) {
                LOG_INFO(sLogger, ("no more alive range checkpoint, delete", key.ToString()));
                appendCheckpointKeys(key.ToString(), cpt.concurrency(), shouldDeleteCptKeys);
                continue;
            }
        }

        // Valid primary checkpoint.
        if (checkpoints) {
            checkpoints->resize(checkpoints->size() + 1);
            checkpoints->back().first.assign(key.data(), key.size());
            checkpoints->back().second.Swap(&cpt);
        }
    }

    return GetCurrentTimeInMilliSeconds() - scanStartTime;
}


std::vector<std::pair<std::string, PrimaryCheckpointPB>>
CheckpointManagerV2::ScanCheckpoints(const std::vector<Config*>& exactlyOnceConfigs) {
    std::vector<std::pair<std::string, PrimaryCheckpointPB>> checkpoints;

    LOG_INFO(sLogger,
             ("begin to scan checkpoints for exactly once configs", "")("config count", exactlyOnceConfigs.size()));
    if (nullptr == mDatabase) {
        LOG_ERROR(sLogger, ("scan exacly once checkpoints error", "uninitialized"));
        return checkpoints;
    }

    std::vector<std::string> toDeleteKeys;
    auto scanUsedTimeInMs = scanCheckpoints(exactlyOnceConfigs, &checkpoints, toDeleteKeys);
    auto deleteUsedTimeInMs = DeleteCheckpoints(toDeleteKeys);
    LOG_INFO(sLogger,
             ("finish scanning checkpoints for exactly once configs", "")("checkpoint count", checkpoints.size())(
                 "scan used time", scanUsedTimeInMs)("delete count", toDeleteKeys.size())("delete used time",
                                                                                          deleteUsedTimeInMs));
    return checkpoints;
}

uint64_t CheckpointManagerV2::DeleteCheckpoints(const std::vector<std::string>& keys) {
    if (keys.empty()) {
        return 0;
    }

    auto const startTimeInMs = GetCurrentTimeInMilliSeconds();
    leveldb::WriteBatch batch;
    for (auto& k : keys) {
        batch.Delete(k);
    }
    auto status = mDatabase->Write(mDefaultWriteOption, &batch);
    auto const usedTimeInMs = GetCurrentTimeInMilliSeconds() - startTimeInMs;
    if (status.ok()) {
        LOG_DEBUG(sLogger, ("delete checkpoints, count", keys.size()));
    } else {
        detail::logDatabaseError("batch_delete", std::to_string(keys.size()), status);
    }
    return usedTimeInMs;
}

uint64_t CheckpointManagerV2::UpdatePrimaryCheckpoints(
    const std::vector<std::pair<std::string, PrimaryCheckpointPB>*>& checkpoints) {
#define METHOD_LOG_PATTERN ("method", "UpdatePrimaryCheckpoints")("count", checkpoints.size())
    auto const startTimeInMs = GetCurrentTimeInMilliSeconds();
    leveldb::WriteBatch batch;
    for (auto& cptPair : checkpoints) {
        auto& key = cptPair->first;
        auto& cpt = cptPair->second;
        std::string data;
        if (!cpt.SerializeToString(&data)) {
            LOG_ERROR(sLogger, METHOD_LOG_PATTERN("serialize error", key)("checkpoint", cpt.DebugString()));
            continue;
        }
        batch.Put(key, data);
    }
    auto status = mDatabase->Write(mDefaultWriteOption, &batch);
    if (status.ok()) {
        return GetCurrentTimeInMilliSeconds() - startTimeInMs;
    } else {
        detail::logDatabaseError("batch_update", std::to_string(checkpoints.size()), status);
        return 0;
    }
#undef METHOD_LOG_PATTERN
}

uint64_t CheckpointManagerV2::DeletePrimaryCheckpoints(
    const std::vector<std::pair<std::string, PrimaryCheckpointPB>*>& checkpoints) {
    std::vector<std::string> keys;
    for (auto& cptPair : checkpoints) {
        appendCheckpointKeys(cptPair->first, cptPair->second.concurrency(), keys);
    }
    return DeleteCheckpoints(keys);
}

bool CheckpointManagerV2::open() {
    const auto databasePath = detail::getDatabasePath();
#define METHOD_LOG_PATTERN ("path", databasePath)("method", "open")
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, databasePath, &mDatabase);
    if (!status.ok()) {
        detail::logDatabaseError("open", databasePath, status);
        mDatabase = nullptr;
        return false;
    }
    LOG_DEBUG(sLogger, METHOD_LOG_PATTERN("checkpoint database opened", ""));
    return true;
#undef METHOD_LOG_PATTERN
}

bool CheckpointManagerV2::close() {
    bool opened = mDatabase != nullptr;
    if (opened) {
        delete mDatabase;
        mDatabase = nullptr;
    }
    return opened;
}

bool CheckpointManagerV2::readDatabase(const std::string& key, std::string& value) {
    ASSERT_LEVELDB_STATUS;

    leveldb::Status s = mDatabase->Get(leveldb::ReadOptions(), key, &value);
    if (s.ok()) {
        return true;
    }
    LOG_DEBUG(sLogger, ("not ok when read checkpoint", key)("status", s.ToString()));
    if (!s.IsNotFound()) {
        detail::logDatabaseError("read", key, s);
    }
    return false;
}

bool CheckpointManagerV2::read(const std::string& key, std::string& value) {
    if (!readDatabase(key, value)) {
        return false;
    }

    // Check if the primary key is marked, bring it back.
    if (detail::isRangeKey(key.data(), key.size())) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    auto iter = mGCItems.find(key);
    if (iter != mGCItems.end()) {
        mGCItems.erase(iter);
        LOG_DEBUG(sLogger, ("bring checkpoint back from GC", key));
    }
    return true;
}

bool CheckpointManagerV2::write(const std::string& key, const std::string& value) {
    ASSERT_LEVELDB_STATUS;

    leveldb::Status s = mDatabase->Put(mDefaultWriteOption, key, value);
    if (s.ok()) {
        return true;
    }
    detail::logDatabaseError("write", key, s);
    return false;
}

void CheckpointManagerV2::MarkGC(const std::string& primaryKey) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mGCItems.find(primaryKey) != mGCItems.end()) {
            return;
        }
        mGCItems[primaryKey] = time(NULL);
    }
    LOG_DEBUG(sLogger, ("checkpoint", "mark gc")("key", primaryKey));
}

void CheckpointManagerV2::checkGCItems() {
    const auto curTime = time(NULL);
    const auto startTimeInMs = GetCurrentTimeInMilliSeconds();
    int32_t deletedCount = 0;

    PrimaryCheckpointPB cpt;
    std::vector<std::string> keys;
    std::lock_guard<std::mutex> lock(mMutex);
    const int32_t maxDeleteCount
        = 1 + static_cast<int32_t>(mGCItems.size() * DOUBLE_FLAG(logtail_checkpoint_max_gc_count_ratio_per_round));
    auto iter = mGCItems.begin();
    while (iter != mGCItems.end()) {
        if (mStopGCThread
            || (GetCurrentTimeInMilliSeconds() - startTimeInMs
                >= static_cast<uint64_t>(INT64_FLAG(logtail_checkpoint_max_used_time_per_round_in_msec)))
            || (deletedCount >= maxDeleteCount)) {
            break;
        }

        auto& key = iter->first;
        auto& createTime = iter->second;
        if (!(curTime >= createTime && curTime - createTime >= INT32_FLAG(logtail_checkpoint_gc_threshold_sec))) {
            ++iter;
            continue;
        }

        // GC primary checkpiont and corresponding range checkpoints.
        do {
            std::string value;
            if (!readDatabase(key, value) || !cpt.ParseFromArray(value.data(), value.size())) {
                LOG_ERROR(sLogger, ("read or parse error when GC checkpoint", key));
                break;
            }

            keys.clear();
            keys.reserve(1 + cpt.concurrency());
            appendCheckpointKeys(key, cpt.concurrency(), keys);
            DeleteCheckpoints(keys);
            LOG_INFO(sLogger, ("GC checkpoint", key)("time", curTime - createTime)("checkpoint", cpt.DebugString()));
            ++deletedCount;
        } while (0);
        iter = mGCItems.erase(iter);
    }
}

void CheckpointManagerV2::runGCLoop() {
    if (nullptr == mDatabase) {
        LOG_ERROR(sLogger, ("runGCLoop exit", "checkpoint database is closed"));
        return;
    }

    while (!mStopGCThread) {
        std::this_thread::sleep_for(std::chrono::seconds(INT32_FLAG(logtail_checkpoint_check_gc_interval_sec)));

        checkGCItems();

        std::vector<std::string> toDeleteCptKeys;
        auto scanUsedTimeInMs = scanCheckpoints(std::vector<Config*>(), nullptr, toDeleteCptKeys, 100);
        auto deleteUsedTimeInMs = DeleteCheckpoints(toDeleteCptKeys);
        if (!toDeleteCptKeys.empty()) {
            LOG_INFO(sLogger,
                     ("delete checkpoints", toDeleteCptKeys.size())("scan used time", scanUsedTimeInMs)(
                         "delete used time", deleteUsedTimeInMs));
        }
    }
    LOG_INFO(sLogger, ("runGCLoop exit", "done"));
}

#ifdef APSARA_UNIT_TEST_MAIN
void CheckpointManagerV2::rebuild() {
    bool opened = close();
    leveldb::DestroyDB(detail::getDatabasePath(), leveldb::Options());
    if (opened) {
        open();
    }
}
#endif

} // namespace logtail
