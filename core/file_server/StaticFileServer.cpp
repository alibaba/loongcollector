// Copyright 2025 iLogtail Authors
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

#include "file_server/StaticFileServer.h"

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "file_server/checkpoint/CheckPointManager.h"
#include "file_server/checkpoint/FileSendCheckpoint.h"
#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"
#include "runner/ProcessorRunner.h"


DEFINE_FLAG_INT32(input_static_file_checkpoint_dump_interval_sec, "", 5);

using namespace std;

namespace logtail {

StaticFileServer::StaticFileServer() {
    WriteMetrics::GetInstance()->CreateMetricsRecordRef(
        mMetricsRecordRef,
        MetricCategory::METRIC_CATEGORY_RUNNER,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_STATIC_FILE_SERVER}});

    mLastRunTimeGauge = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    mActiveInputsTotalGauge = mMetricsRecordRef.CreateIntGauge(METRIC_RUNNER_STATIC_FILE_SERVER_ACTIVE_INPUTS_COUNT);

    WriteMetrics::GetInstance()->CommitMetricsRecordRef(mMetricsRecordRef);
}

void StaticFileServer::Init() {
    InputStaticFileCheckpointManager::GetInstance()->GetAllCheckpointFileNames();
    mIsThreadRunning = true;
    mThreadRes = async(launch::async, &StaticFileServer::Run, this);
    mStartTime = time(nullptr);
}

void StaticFileServer::Stop() {
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        if (!mIsThreadRunning || !mThreadRes.valid()) {
            return;
        }
        mIsThreadRunning = false;
    }
    mStopCV.notify_all();

    future_status s = mThreadRes.wait_for(chrono::seconds(1));
    if (s == future_status::ready) {
        LOG_INFO(sLogger, ("static file server", "stopped successfully"));
    } else {
        LOG_WARNING(sLogger, ("static file server", "forced to stopped"));
    }
}

bool StaticFileServer::HasRegisteredPlugins() const {
    lock_guard<mutex> lock(mUpdateMux);
    return !mInputFileReaderConfigsMap.empty();
}

void StaticFileServer::ClearUnusedCheckpoints() {
    if (mIsUnusedCheckpointsCleared || time(nullptr) - mStartTime < INT32_FLAG(unused_checkpoints_clear_interval_sec)) {
        return;
    }
    InputStaticFileCheckpointManager::GetInstance()->ClearUnusedCheckpoints();
    mIsUnusedCheckpointsCleared = true;
}

void StaticFileServer::RemoveInput(const string& configName, size_t idx, bool keepingCheckpoint) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        mInputFileDiscoveryConfigsMap.erase(make_pair(configName, idx));
        mInputFileReaderConfigsMap.erase(make_pair(configName, idx));
        mInputMultilineConfigsMap.erase(make_pair(configName, idx));
        mInputFileTagConfigsMap.erase(make_pair(configName, idx));
        mDeletedInputs.emplace(configName, idx);
    }
    if (!keepingCheckpoint) {
        InputStaticFileCheckpointManager::GetInstance()->DeleteCheckpoint(configName, idx);
    }
}

void StaticFileServer::AddInput(const string& configName,
                                size_t idx,
                                const optional<vector<filesystem::path>>& files,
                                FileDiscoveryOptions* fileDiscoveryOpts,
                                const FileReaderOptions* fileReaderOpts,
                                const MultilineOptions* multilineOpts,
                                const FileTagOptions* fileTagOpts,
                                const CollectionPipelineContext* ctx) {
    // check if the server is started, if not, start it
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        if (!mIsThreadRunning || !mThreadRes.valid()) {
            Init();
        }
    }

    // 安全获取Pipeline的时间值，避免空指针解引用
    uint32_t startTime = 0;
    uint32_t expireTime = 0;

    if (ctx != nullptr && ctx->HasValidPipeline()) {
        const auto& pipeline = ctx->GetPipeline();
        startTime = pipeline.GetOnetimeStartTime().value_or(0);
        expireTime = pipeline.GetOnetimeExpireTime().value_or(0);
    }

    InputStaticFileCheckpointManager::GetInstance()->CreateCheckpoint(configName, idx, files, startTime, expireTime);
    {
        lock_guard<mutex> lock(mUpdateMux);
        mInputFileDiscoveryConfigsMap.try_emplace(make_pair(configName, idx), make_pair(fileDiscoveryOpts, ctx));
        mInputFileReaderConfigsMap.try_emplace(make_pair(configName, idx), make_pair(fileReaderOpts, ctx));
        mInputMultilineConfigsMap.try_emplace(make_pair(configName, idx), make_pair(multilineOpts, ctx));
        mInputFileTagConfigsMap.try_emplace(make_pair(configName, idx), make_pair(fileTagOpts, ctx));
        mAddedInputs.emplace(configName, idx);
    }
}

void StaticFileServer::Run() {
    LOG_INFO(sLogger, ("static file server", "started"));
    unique_lock<mutex> lock(mThreadRunningMux);
    time_t lastDumpCheckpointTime = time(nullptr);
    while (mIsThreadRunning) {
        lock.unlock();
        UpdateInputs();
        ReadFiles();

        auto cur = time(nullptr);
        if (cur - lastDumpCheckpointTime >= INT32_FLAG(input_static_file_checkpoint_dump_interval_sec)) {
            InputStaticFileCheckpointManager::GetInstance()->DumpAllCheckpointFiles();
            lastDumpCheckpointTime = cur;
        }
        SET_GAUGE(mLastRunTimeGauge, time(nullptr));
        lock.lock();
        if (mStopCV.wait_for(lock, chrono::milliseconds(10), [this]() { return !mIsThreadRunning; })) {
            return;
        }
    }
}

void StaticFileServer::ReadFiles() {
    for (auto& item : mPipelineNameReadersMap) {
        {
            lock_guard<mutex> lock(mUpdateMux);
            const auto& configName = item.first;
            auto inputIdx = item.second.first;
            if (mDeletedInputs.find(make_pair(configName, inputIdx)) != mDeletedInputs.end()) {
                continue;
            }

            auto& reader = item.second.second;
            const auto key = make_pair(configName, inputIdx);
            auto* checkpointMgr = InputStaticFileCheckpointManager::GetInstance();
            // Deferred commit (advance the persisted offset only after a successful send) is only
            // safe when the data is sent by a confirm-capable native flusher (SLS/Kafka/File). If
            // the pipeline flushes through the Go pipeline, the C++ send-checkpoint token is dropped
            // at the C++/Go protobuf boundary (see ProcessorRunner) and would never be confirmed,
            // permanently stalling this input. In that case fall back to committing the offset at
            // read time (the original at-least-once behavior).
            const auto readerCfg = GetFileReaderConfig(configName, inputIdx);
            const bool deferCommit = readerCfg.second != nullptr && !readerCfg.second->IsFlushingThroughGoPipeline();
            auto cur = chrono::system_clock::now();
            while (chrono::system_clock::now() - cur < chrono::milliseconds(50)) {
                // Runtime rollback: a flusher reported that a previously read unit could not be
                // sent. Drop the current reader and rewind to the last committed offset so the
                // "read-but-uncommitted" range is re-read, recovering the data without a restart.
                if (deferCommit && checkpointMgr->ConsumeRollback(configName, inputIdx)) {
                    reader = nullptr;
                    mPendingCommitFileIndexMap.erase(key);
                    size_t committedIdx = 0;
                    if (checkpointMgr->GetCommittedFileIndex(configName, inputIdx, &committedIdx)) {
                        mReadingFileIndexMap[key] = committedIdx;
                    }
                    LOG_WARNING(sLogger,
                                ("rewind reader to last committed offset after send failure",
                                 "")("config", configName)("input idx", inputIdx));
                }
                if (!reader) {
                    // gating: if reading of a file reached EOF but its data has not been fully
                    // committed yet, do not start the next file, otherwise GetNextAvailableReader
                    // would re-create a reader for the still-uncommitted current file.
                    if (deferCommit) {
                        auto pendingIt = mPendingCommitFileIndexMap.find(key);
                        if (pendingIt != mPendingCommitFileIndexMap.end()) {
                            size_t committedIdx = 0;
                            if (checkpointMgr->GetCommittedFileIndex(configName, inputIdx, &committedIdx)
                                && committedIdx > pendingIt->second) {
                                mPendingCommitFileIndexMap.erase(pendingIt);
                            } else {
                                break; // wait for the committed offset to catch up
                            }
                        }
                    }
                    reader = GetNextAvailableReader(configName, inputIdx);
                    if (!reader) {
                        break;
                    }
                    if (deferCommit) {
                        size_t committedIdx = 0;
                        if (checkpointMgr->GetCommittedFileIndex(configName, inputIdx, &committedIdx)) {
                            mReadingFileIndexMap[key] = committedIdx;
                        }
                    }
                }

                bool skip = false;
                while (chrono::system_clock::now() - cur < chrono::milliseconds(50)) {
                    if (!ProcessQueueManager::GetInstance()->IsValidToPush(reader->GetQueueKey())) {
                        skip = true;
                        break;
                    }

                    auto logBuffer = make_unique<LogBuffer>();
                    bool moreData = reader->ReadLog(*logBuffer, nullptr, true);
                    auto group = LogFileReader::GenerateEventGroup(reader, logBuffer.get());
                    const uint64_t endOffset = reader->GetLastFilePos();
                    // In deferred mode, register this read unit as in-flight and attach a send
                    // checkpoint to the group; the committed file offset is advanced only after the
                    // flusher confirms the send (see InputStaticFileCheckpointManager::ConfirmSent).
                    // Skip empty groups: they may be dropped before reaching a flusher, which would
                    // never confirm and would stall the committed offset forever. Empty reads also
                    // carry no new offset, so there is nothing to commit.
                    const bool hasEvents = !group.GetEvents().empty();
                    uint64_t seq = 0;
                    if (deferCommit && hasEvents) {
                        seq = checkpointMgr->RegisterInflight(configName, inputIdx, endOffset);
                        group.SetFileSendCheckpoint(
                            make_shared<FileSendCheckpoint>(configName, inputIdx, seq, endOffset));
                    }
                    if (!ProcessorRunner::GetInstance()->PushQueue(reader->GetQueueKey(), inputIdx, std::move(group))) {
                        // should not happend, since only one thread is pushing to the queue
                        LOG_ERROR(sLogger,
                                  ("failed to push to process queue", "discard data")("config", configName)(
                                      "input idx", inputIdx)("filepath", reader->GetHostLogPath()));
                        // data is discarded; in deferred mode confirm immediately so the committed
                        // offset is not permanently blocked behind a unit that never reaches a flusher
                        if (deferCommit && hasEvents) {
                            checkpointMgr->ConfirmSent(configName, inputIdx, seq);
                        }
                    }
                    if (!deferCommit) {
                        // legacy behavior: commit the read offset immediately
                        checkpointMgr->UpdateCurrentFileCheckpoint(configName, inputIdx, endOffset);
                    }
                    if (!moreData) {
                        if (deferCommit) {
                            // finished reading the current file; defer advancing to the next file
                            // until all of this file's data has been committed.
                            // If the final read produced no events, register and immediately confirm a
                            // sentinel unit at the EOF offset. ConfirmSent only advances through
                            // consecutively confirmed units, so this safely finalizes the file after
                            // any still-pending data units commit (and immediately for an empty file).
                            if (!hasEvents) {
                                const uint64_t eofSeq
                                    = checkpointMgr->RegisterInflight(configName, inputIdx, endOffset);
                                checkpointMgr->ConfirmSent(configName, inputIdx, eofSeq);
                            }
                            auto ridIt = mReadingFileIndexMap.find(key);
                            if (ridIt != mReadingFileIndexMap.end()) {
                                mPendingCommitFileIndexMap[key] = ridIt->second;
                            }
                        }
                        reader = nullptr;
                        skip = true;
                        break;
                    }
                }
                if (skip) {
                    break;
                }
            }
        }
        {
            lock_guard<mutex> lock(mThreadRunningMux);
            if (!mIsThreadRunning) {
                return;
            }
        }
    }
}

// Caller must hold mUpdateMux (ReadFiles, or TestGetNextAvailableReader for unit tests).
LogFileReaderPtr StaticFileServer::GetNextAvailableReader(const string& configName, size_t idx) {
    FileFingerprint fingerprint;
    while (InputStaticFileCheckpointManager::GetInstance()->GetCurrentFileFingerprint(configName, idx, &fingerprint)) {
        filesystem::path filePath = fingerprint.mFilePath.lexically_normal();
        string errMsg;

        // 检查文件是否存在，如果不存在或路径变了但 devinode 没变，尝试查找轮转后的文件
        auto currentDevInode = GetFileDevInode(filePath.string());
        if (!currentDevInode.IsValid() || currentDevInode != fingerprint.mDevInode) {
            // 文件不存在或 devinode 不匹配，尝试在目录中查找轮转后的文件
            auto dirPath = filePath.parent_path();
            const auto searchResult
                = SearchFilePathByDevInodeInDirectory(dirPath.string(), 0, fingerprint.mDevInode, nullptr);
            if (searchResult) {
                filePath = filesystem::path(ConvertAndNormalizeNativePath(searchResult.value())).lexically_normal();
                LOG_INFO(sLogger,
                         ("file rotated, found new path", "")("config", configName)("input idx", idx)(
                             "old path", fingerprint.mFilePath.string())("new path", filePath.string()));
            } else {
                errMsg = "file not found and cannot find rotated file";
            }
        }

        if (!errMsg.empty()) {
            LOG_WARNING(sLogger,
                        ("failed to get reader",
                         errMsg)("config", configName)("input idx", idx)("filepath", fingerprint.mFilePath.string()));
            InputStaticFileCheckpointManager::GetInstance()->InvalidateCurrentFileCheckpoint(configName, idx);
            continue;
        }

        LogFileReaderPtr reader(LogFileReader::CreateLogFileReader(filePath.parent_path().string(),
                                                                   filePath.filename().string(),
                                                                   fingerprint.mDevInode,
                                                                   GetFileReaderConfig(configName, idx),
                                                                   GetMultilineConfig(configName, idx),
                                                                   GetFileDiscoveryConfig(configName, idx),
                                                                   GetFileTagConfig(configName, idx),
                                                                   0,
                                                                   true));
        if (!reader) {
            errMsg = "failed to create reader";
        } else {
            // 设置预期文件大小限制（仅对 StaticFileServer reader 生效）
            if (fingerprint.mSize > 0) {
                reader->SetExpectedFileSize(static_cast<int64_t>(fingerprint.mSize));
            }
            if (!reader->UpdateFilePtr()) {
                errMsg = "failed to open file";
            } else if (!reader->CheckFileSignatureAndOffset(false)
                       || reader->GetSignature() != make_pair(fingerprint.mSignatureHash, fingerprint.mSignatureSize)) {
                errMsg = "file signature check failed";
            }
        }
        if (!errMsg.empty()) {
            LOG_WARNING(sLogger,
                        ("failed to get reader", errMsg)("config", configName)("input idx", idx)("filepath",
                                                                                                 filePath.string()));
            InputStaticFileCheckpointManager::GetInstance()->InvalidateCurrentFileCheckpoint(configName, idx);
            continue;
        }
        if (fingerprint.mOffset > 0) {
            LOG_INFO(sLogger,
                     ("set last file pos", fingerprint.mOffset)("config", configName)("input idx", idx)(
                         "filepath", filePath.string())("dev", fingerprint.mDevInode.dev)("inode",
                                                                                          fingerprint.mDevInode.inode));
            reader->SetLastFilePos(fingerprint.mOffset);
        }
        return reader;
    }
    // all files have been read (mDeletedInputs under mUpdateMux; see GetNextAvailableReader precondition)
    mDeletedInputs.emplace(configName, idx);
    return LogFileReaderPtr();
}

void StaticFileServer::UpdateInputs() {
    unique_lock<mutex> lock(mUpdateMux);
    for (const auto& item : mDeletedInputs) {
        mPipelineNameReadersMap.erase(item.first);
        mReadingFileIndexMap.erase(item);
        mPendingCommitFileIndexMap.erase(item);
    }
    mDeletedInputs.clear();

    for (const auto& item : mAddedInputs) {
        mPipelineNameReadersMap.emplace(item.first, make_pair(item.second, LogFileReaderPtr()));
    }
    mAddedInputs.clear();

    SET_GAUGE(mActiveInputsTotalGauge, mPipelineNameReadersMap.size());
}

FileDiscoveryConfig StaticFileServer::GetFileDiscoveryConfig(const std::string& name, size_t idx) const {
    auto it = mInputFileDiscoveryConfigsMap.find(make_pair(name, idx));
    if (it == mInputFileDiscoveryConfigsMap.end()) {
        // should not happen
        return make_pair(nullptr, nullptr);
    }
    return it->second;
}

FileReaderConfig StaticFileServer::GetFileReaderConfig(const std::string& name, size_t idx) const {
    auto it = mInputFileReaderConfigsMap.find(make_pair(name, idx));
    if (it == mInputFileReaderConfigsMap.end()) {
        // should not happen
        return make_pair(nullptr, nullptr);
    }
    return it->second;
}

MultilineConfig StaticFileServer::GetMultilineConfig(const std::string& name, size_t idx) const {
    auto it = mInputMultilineConfigsMap.find(make_pair(name, idx));
    if (it == mInputMultilineConfigsMap.end()) {
        // should not happen
        return make_pair(nullptr, nullptr);
    }
    return it->second;
}

FileTagConfig StaticFileServer::GetFileTagConfig(const std::string& name, size_t idx) const {
    auto it = mInputFileTagConfigsMap.find(make_pair(name, idx));
    if (it == mInputFileTagConfigsMap.end()) {
        // should not happen
        return make_pair(nullptr, nullptr);
    }
    return it->second;
}

#ifdef APSARA_UNIT_TEST_MAIN
void StaticFileServer::Clear() {
    Stop();
    lock_guard<mutex> lock(mUpdateMux);
    mInputFileDiscoveryConfigsMap.clear();
    mInputFileReaderConfigsMap.clear();
    mInputMultilineConfigsMap.clear();
    mInputFileTagConfigsMap.clear();
    mPipelineNameReadersMap.clear();
    mAddedInputs.clear();
    mDeletedInputs.clear();
}

LogFileReaderPtr StaticFileServer::TestGetNextAvailableReader(const string& configName, size_t idx) {
    lock_guard<mutex> lock(mUpdateMux);
    return GetNextAvailableReader(configName, idx);
}
#endif

} // namespace logtail
