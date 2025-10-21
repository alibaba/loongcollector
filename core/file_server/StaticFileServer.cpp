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
#include "common/LogtailCommonFlags.h"
#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"
#include "runner/ProcessorRunner.h"


DEFINE_FLAG_INT32(input_static_file_checkpoint_dump_interval_sec, "", 5);

using namespace std;

namespace logtail {

void StaticFileServer::Init() {
    InputStaticFileCheckpointManager::GetInstance()->GetAllCheckpointFileNames();
    mThreadRes = async(launch::async, &StaticFileServer::Run, this);
    mStartTime = time(nullptr);
}

void StaticFileServer::Stop() {
    if (!mThreadRes.valid()) {
        return;
    }
    {
        lock_guard<mutex> lock(mThreadRunningMux);
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

void StaticFileServer::RemoveInput(const string& configName, size_t idx) {
    {
        lock_guard<mutex> lock(mUpdateMux);
        mInputFileDiscoveryConfigsMap.erase(make_pair(configName, idx));
        mInputFileReaderConfigsMap.erase(make_pair(configName, idx));
        mInputMultilineConfigsMap.erase(make_pair(configName, idx));
        mInputFileTagConfigsMap.erase(make_pair(configName, idx));
        mDeletedInputs.emplace(configName, idx);
    }
    InputStaticFileCheckpointManager::GetInstance()->DeleteCheckpoint(configName, idx);
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
    if (!mThreadRes.valid()) {
        Init();
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
            auto cur = chrono::system_clock::now();
            while (chrono::system_clock::now() - cur < chrono::milliseconds(50)) {
                if (!reader) {
                    reader = GetNextAvailableReader(configName, inputIdx);
                    if (!reader) {
                        break;
                    }
                }

                bool skip = false;
                while (chrono::system_clock::now() - cur < chrono::milliseconds(50)) {
                    if (!ProcessQueueManager::GetInstance()->IsValidToPush(reader->GetQueueKey())) {
                        skip = true;
                        break;
                    }

                    auto logBuffer = make_unique<LogBuffer>();
                    bool moreData = reader->ReadLog(*logBuffer, nullptr);
                    auto group = LogFileReader::GenerateEventGroup(reader, logBuffer.get());
                    if (!ProcessorRunner::GetInstance()->PushQueue(reader->GetQueueKey(), inputIdx, std::move(group))) {
                        // should not happend, since only one thread is pushing to the queue
                        LOG_ERROR(sLogger,
                                  ("failed to push to process queue", "discard data")("config", configName)(
                                      "input idx", inputIdx)("filepath", reader->GetHostLogPath()));
                    }
                    InputStaticFileCheckpointManager::GetInstance()->UpdateCurrentFileCheckpoint(
                        configName, inputIdx, reader->GetLastFilePos(), reader->GetFileSize());
                    if (!moreData) {
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

LogFileReaderPtr StaticFileServer::GetNextAvailableReader(const string& configName, size_t idx) {
    FileFingerprint fingerprint;
    while (InputStaticFileCheckpointManager::GetInstance()->GetCurrentFileFingerprint(configName, idx, &fingerprint)) {
        LogFileReaderPtr reader(LogFileReader::CreateLogFileReader(fingerprint.mFilePath.parent_path().string(),
                                                                   fingerprint.mFilePath.filename().string(),
                                                                   fingerprint.mDevInode,
                                                                   GetFileReaderConfig(configName, idx),
                                                                   GetMultilineConfig(configName, idx),
                                                                   GetFileDiscoveryConfig(configName, idx),
                                                                   GetFileTagConfig(configName, idx),
                                                                   0,
                                                                   true));
        string errMsg;
        if (!reader) {
            errMsg = "failed to create reader";
        } else if (!reader->UpdateFilePtr()) {
            errMsg = "failed to open file";
        } else if (!reader->CheckFileSignatureAndOffset(false)
                   || reader->GetSignature() != make_pair(fingerprint.mSignatureHash, fingerprint.mSignatureSize)) {
            errMsg = "file signature check failed";
        }
        if (!errMsg.empty()) {
            LOG_WARNING(sLogger,
                        ("failed to get reader",
                         errMsg)("config", configName)("input idx", idx)("filepath", fingerprint.mFilePath.string()));
            InputStaticFileCheckpointManager::GetInstance()->InvalidateCurrentFileCheckpoint(configName, idx);
            continue;
        }
        return reader;
    }
    // all files have been read
    mDeletedInputs.emplace(configName, idx);
    return LogFileReaderPtr();
}

void StaticFileServer::UpdateInputs() {
    unique_lock<mutex> lock(mUpdateMux);
    for (const auto& item : mDeletedInputs) {
        mPipelineNameReadersMap.erase(item.first);
    }
    mDeletedInputs.clear();

    for (const auto& item : mAddedInputs) {
        mPipelineNameReadersMap.emplace(item.first, make_pair(item.second, LogFileReaderPtr()));
    }
    mAddedInputs.clear();
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
#endif

} // namespace logtail
