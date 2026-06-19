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

#pragma once

#include <cstdint>

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "json/json.h"

#include "file_server/checkpoint/InputStaticFileCheckpoint.h"

namespace logtail {

class InputStaticFileCheckpointManager {
public:
    InputStaticFileCheckpointManager(const InputStaticFileCheckpointManager&) = delete;
    InputStaticFileCheckpointManager& operator=(const InputStaticFileCheckpointManager&) = delete;

    static InputStaticFileCheckpointManager* GetInstance() {
        static InputStaticFileCheckpointManager instance;
        return &instance;
    }

    bool CreateCheckpoint(const std::string& configName,
                          size_t idx,
                          const std::optional<std::vector<std::filesystem::path>>& files = std::nullopt,
                          uint32_t startTime = 0,
                          uint32_t expireTime = 0);
    bool DeleteCheckpoint(const std::string& configName, size_t idx);
    bool UpdateCurrentFileCheckpoint(const std::string& configName, size_t idx, uint64_t offset);
    bool InvalidateCurrentFileCheckpoint(const std::string& configName, size_t idx);
    bool GetCurrentFileFingerprint(const std::string& configName, size_t idx, FileFingerprint* cpt);

    // Deferred commit support: the file offset is advanced only after the data has been
    // successfully sent by the flusher.
    // RegisterInflight is called at read time and returns a monotonically increasing
    // sequence number for the read unit ending at offset. ConfirmSent is called by the
    // flusher on send success and advances the committed offset in strict read order
    // (contiguous-ack), tolerating out-of-order acknowledgements (e.g. Kafka).
    uint64_t RegisterInflight(const std::string& configName, size_t idx, uint64_t endOffset);
    void ConfirmSent(const std::string& configName, size_t idx, uint64_t seq);
    // Called by a flusher when a read unit could not be sent (final/terminal failure). Requests
    // a runtime rollback: the reader rewinds to the last committed offset and re-reads the
    // "read-but-uncommitted" range, so the data is recovered without restarting the process.
    void ReportSendFailure(const std::string& configName, size_t idx, uint64_t seq);
    // Called by the reader thread. If a rollback was requested for this input, drops all
    // in-flight units (so their offsets are re-read) and returns true. The committed offset is
    // left untouched, so a rebuilt reader resumes exactly from the last confirmed position.
    bool ConsumeRollback(const std::string& configName, size_t idx);
    // Returns the committed (persisted) file index, used by the reader to decide when it
    // may advance to the next file after deferring offset commits.
    bool GetCommittedFileIndex(const std::string& configName, size_t idx, size_t* fileIndex);


    void DumpAllCheckpointFiles() const;
    void GetAllCheckpointFileNames();
    void ClearUnusedCheckpoints();
    // std::vector<Json::Value> ExportAllCheckpoints();

private:
    InputStaticFileCheckpointManager();
    ~InputStaticFileCheckpointManager() = default;

    bool RetrieveCheckpointFromFile(const std::string& configName, size_t idx, InputStaticFileCheckpoint* cpt);
    // Assumes mUpdateMux is held by the caller.
    bool UpdateCurrentFileCheckpointLocked(const std::string& configName, size_t idx, uint64_t offset);
    // bool DeleteCheckpoint(const std::string& configName, size_t idx);
    bool DumpCheckpointFile(const InputStaticFileCheckpoint& cpt) const;
    bool LoadCheckpointFile(const std::filesystem::path& filepath, InputStaticFileCheckpoint* cpt);

    std::filesystem::path mCheckpointRootPath;

    // accessed by main thread, input runner thread and observabaility thread
    mutable std::mutex mUpdateMux;
    std::map<std::pair<std::string, size_t>, InputStaticFileCheckpoint> mInputCheckpointMap;
    // std::multimap<std::string, size_t> mDeletedInputs;

    // Tracks in-flight read units per input for deferred (send-confirmed) offset commit.
    // pending maps seq -> end offset; commitSeq is the next seq expected to be committed.
    struct InflightState {
        uint64_t nextSeq = 0;
        uint64_t commitSeq = 0;
        // seq -> (endOffset, confirmed)
        std::map<uint64_t, std::pair<uint64_t, bool>> pending;
        // set by ReportSendFailure, consumed by the reader thread to trigger a rewind
        bool rollbackRequested = false;
    };
    std::map<std::pair<std::string, size_t>, InflightState> mInflightMap;

    // only accessed by main thread
    std::set<std::pair<std::string, size_t>> mCheckpointFileNamesOnInit;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputStaticFileCheckpointManagerUnittest;
    friend class StaticFileServerUnittest;
    friend class InputStaticFileUnittest;
#endif
};

} // namespace logtail
