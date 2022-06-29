/*
 * Copyright 2022 iLogtail Authors
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

#pragma once
#include <string>
#include <unordered_map>
#include <thread>
#include <memory>
#include <mutex>
#include <vector>
#include <leveldb/db.h>
#include "log_pb/checkpoint.pb.h"

namespace logtail {

class Config;

// CheckpointManagerV2 for exactly once feature
//
// It supports realtime read/write operation so that logtail can persist redo logs,
//  which can be used to replay file content from unexpected exit.
//
// There are two kinds of checkpoints in v2 manager:
// - Primary checkpoint: each one of it represents a file, file meta (such as dev inode)
//  is stored in it.
// - Range checkpoints: each primary checkpoint can have several range checkpoints,
//  which are used to record read progress of the file.
// Each exactly once file can be represented by 1 primary checkpoint plus N range
//  checkpoints.
//
// Exactly once means the file content must be sent through determined blocks.
// - If out of order is allowed, these blocks can be sent concurrently by different
//  range checkpoints, that is why we call N concurrency.
// - If order is import, the 1 primary checkpoint + N range checkpoints model downgrades
//  to 1 primary + 1 range, ie. there is only one concurrency for the file.
class CheckpointManagerV2 {
public:
    static std::string MakeRangeKey(const std::string& primaryKey, uint32_t idx);

    // Append key of range checkpoints to keys.
    static void AppendRangeKeys(const std::string& primaryKey, uint32_t rangeCptCount, std::vector<std::string>& keys);

    static CheckpointManagerV2* GetInstance() {
        static auto singleton = new CheckpointManagerV2;
        return singleton;
    }

    // ScanCheckpoints scans checkpoint database, return all alive checkpoints.
    //
    // Checkpoints in database will be deleted if matched anyone of following rules
    //  while scanning:
    //  - The corresponding config is not excatly once any more, delete it.
    //  - No more range checkpoint.
    //  - All range checkpoints are outdated.
    //
    // Called by EventDispatcher::AddExistedCheckPointFileEvents when program start or
    //  updating config, so it is lock free to access managers, such as v1 checkpoint
    //  manager, config manager, etc.
    //
    // @return checkpoints.
    std::vector<std::pair<std::string, /* the key of primary checkpoint */
                          PrimaryCheckpointPB>>
    ScanCheckpoints(const std::vector<Config*>& exactlyOnceConfigs);

    template <class PBType>
    bool GetPB(const std::string& key, PBType& value) {
        std::string data;
        if (!read(key, data)) {
            return false;
        }

        return value.ParseFromString(data);
    }

    template <class PBType>
    bool SetPB(const std::string& key, const PBType& value) {
        std::string data;
        if (!value.SerializeToString(&data)) {
            return false;
        }

        return write(key, data);
    }

    // Add primaryKey to GC list, called in destructor of LogFileReader.
    //
    // GetPB will remove primaryKey from GC list, so for config update case, primary
    //  keys of alive readers will be marked, and these keys will be brought back when
    //  recovering readers by MODIFY events.
    //
    // If the primaryKey stays in GC list for a while, GC thread will do real deletion.
    void MarkGC(const std::string& primaryKey);

    // Batch delete checkpoints by keys.
    //
    // @return used time in milliseconds.
    uint64_t DeleteCheckpoints(const std::vector<std::string>& keys);

    // Batch update primary checkpoints.
    //
    // @return used time in milliseconds.
    uint64_t UpdatePrimaryCheckpoints(const std::vector<std::pair<std::string, PrimaryCheckpointPB>*>& checkpoints);

    // Batch delete primary checkpoints.
    //
    // @return used time in milliseconds.
    uint64_t DeletePrimaryCheckpoints(const std::vector<std::pair<std::string, PrimaryCheckpointPB>*>& checkpoints);

private:
    CheckpointManagerV2();
    ~CheckpointManagerV2();

    // Open database, return true if succeed.
    bool open();
    // Close database, return true if the database is opened before.
    bool close();

    bool readDatabase(const std::string& key, std::string& value);

    // Read/Write checkpoints by key.
    // @return true if succeed.
    bool read(const std::string& key, std::string& value);
    bool write(const std::string& key, const std::string& value);

    // Routine of GC thread.
    void runGCLoop();

    void checkGCItems();

    // Scan whole database according to mode.
    //
    // Scan mode: full or partial.
    // - Full: for loading checkpoints when startup or config update, unlimited scan
    //  time and scan from start to end.
    // - Partial: for regular checkpoint GC, scan time is limited and will scan from
    //  the last position.
    //
    // Except for scan time limit and scan range, full scan will do extra config name
    //  validation, add mismatched checkpoints to shouldDeleteCptKeys.
    //
    // @exactlyOnceConfigs: if it is not empty, do full scan.
    // @checkpoints: valid primary checkpoints found by this scan, if nullptr, ignore.
    // @shouldDeleteCptKeys: keys of checkpoints that should be deleted.
    // @limitScanTimeInMs: 0 means unlimited.
    //
    // @return used time in milliseconds.
    int64_t scanCheckpoints(const std::vector<Config*>& exactlyOnceConfigs,
                            std::vector<std::pair<std::string, PrimaryCheckpointPB>>* checkpoints,
                            std::vector<std::string>& shouldDeleteCptKeys,
                            uint64_t limitScanTimeInMs = 0);

    // Append the key of primary checkpoint and range checkpoints to keys.
    void appendCheckpointKeys(const std::string& primaryKey, uint32_t rgCptCount, std::vector<std::string>& keys);

private:
    std::string mDatabasePath;
    leveldb::DB* mDatabase = nullptr;
    leveldb::WriteOptions mDefaultWriteOption;

    volatile bool mStopGCThread = false;
    std::unique_ptr<std::thread> mGCThreadPtr;
    std::mutex mMutex;
    std::unordered_map<std::string, /* primary key */
                       time_t /* create time */>
        mGCItems;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CheckpointManagerV2Unittest;
    friend class ExactlyOnceReaderUnittest;
    friend class SenderUnittest;

    void rebuild();
#endif
};

} // namespace logtail
