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

#include <chrono>

#if defined(__linux__)
#include <fnmatch.h>
#else
#include "common/StringTools.h"
#endif

#include "collection_pipeline/queue/ProcessQueueManager.h"
#include "common/FileSystemUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/StringTools.h"
#include "container_manager/ContainerManager.h"
#include "file_server/ContainerInfo.h"
#include "file_server/checkpoint/CheckPointManager.h"
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
    ContainerManager::GetInstance()->LoadContainerInfo();
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
        std::pair<std::string, size_t> configInfo = make_pair(configName, idx);
        mInputFileDiscoveryConfigsMap.erase(configInfo);
        mFileContainerMetaMap.erase(configInfo);
        mInputFileReaderConfigsMap.erase(configInfo);
        mInputMultilineConfigsMap.erase(configInfo);
        mInputFileTagConfigsMap.erase(configInfo);
        mDeletedInputs.emplace(configInfo);
    }
    if (!keepingCheckpoint) {
        InputStaticFileCheckpointManager::GetInstance()->DeleteCheckpoint(configName, idx);
    }
}

void StaticFileServer::AddInput(const string& configName,
                                size_t idx,
                                FileDiscoveryOptions* fileDiscoveryOpts,
                                const FileReaderOptions* fileReaderOpts,
                                const MultilineOptions* multilineOpts,
                                const FileTagOptions* fileTagOpts,
                                unordered_map<string, FileCheckpoint::ContainerMeta>& fileContainerMetas,
                                const CollectionPipelineContext* ctx) {
    // check if the server is started, if not, start it
    {
        lock_guard<mutex> lock(mThreadRunningMux);
        if (!mIsThreadRunning || !mThreadRes.valid()) {
            Init();
        }
    }

    if (fileDiscoveryOpts->IsContainerDiscoveryEnabled()) {
        ContainerManager::GetInstance()->Init();
    }

    {
        lock_guard<mutex> lock(mUpdateMux);
        std::pair<std::string, size_t> configInfo = make_pair(configName, idx);
        mInputFileDiscoveryConfigsMap.try_emplace(configInfo, make_pair(fileDiscoveryOpts, ctx));
        mFileContainerMetaMap.try_emplace(configInfo, fileContainerMetas);
        mInputFileReaderConfigsMap.try_emplace(configInfo, make_pair(fileReaderOpts, ctx));
        mInputMultilineConfigsMap.try_emplace(configInfo, make_pair(multilineOpts, ctx));
        mInputFileTagConfigsMap.try_emplace(configInfo, make_pair(fileTagOpts, ctx));
        mAddedInputs.emplace(configInfo, ctx);
    }
}

void StaticFileServer::Run() {
    LOG_INFO(sLogger, ("static file server", "started"));
    unique_lock<mutex> lock(mThreadRunningMux);
    time_t lastDumpCheckpointTime = time(nullptr);
    time_t lastContainerCheckTime = time(nullptr);
    while (mIsThreadRunning) {
        lock.unlock();
        UpdateInputs();
        ReadFiles();

        auto cur = time(nullptr);
        if (cur - lastContainerCheckTime >= 3) {
            if (ContainerManager::GetInstance()->CheckStaticFileServerContainerDiffs()) {
                ContainerManager::GetInstance()->ApplyStaticFileServerContainerDiffs();
            }
            ContainerManager::GetInstance()->GetStaticFileServerContainerStoppedEvents();
            lastContainerCheckTime = cur;
        }
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
                    if (reader) {
                        FileFingerprint fingerprint;
                        if (InputStaticFileCheckpointManager::GetInstance()->GetCurrentFileFingerprint(
                                configName, inputIdx, &fingerprint)
                            && !fingerprint.mContainerID.empty()) {
                            string currentID;
                            bool currentStopped = false;
                            if (GetCurrentContainerInfoForPath(
                                    configName, inputIdx, reader->GetHostLogPath(), &currentID, &currentStopped)) {
                                if (currentID != fingerprint.mContainerID || currentStopped) {
                                    LOG_INFO(sLogger,
                                             ("abort reading: container changed or stopped", "")("config", configName)(
                                                 "input idx", inputIdx)("filepath", reader->GetHostLogPath())(
                                                 "checkpoint container id", fingerprint.mContainerID)(
                                                 "current container id", currentID)("current stopped", currentStopped));
                                    InputStaticFileCheckpointManager::GetInstance()->InvalidateCurrentFileCheckpoint(
                                        configName, inputIdx);
                                    reader = nullptr;
                                    skip = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (!ProcessQueueManager::GetInstance()->IsValidToPush(reader->GetQueueKey())) {
                        skip = true;
                        break;
                    }

                    auto logBuffer = make_unique<LogBuffer>();
                    bool moreData = reader->ReadLog(*logBuffer, nullptr, true);
                    auto group = LogFileReader::GenerateEventGroup(reader, logBuffer.get());
                    if (!ProcessorRunner::GetInstance()->PushQueue(reader->GetQueueKey(), inputIdx, std::move(group))) {
                        // should not happend, since only one thread is pushing to the queue
                        LOG_ERROR(sLogger,
                                  ("failed to push to process queue", "discard data")("config", configName)(
                                      "input idx", inputIdx)("filepath", reader->GetHostLogPath()));
                    }
                    InputStaticFileCheckpointManager::GetInstance()->UpdateCurrentFileCheckpoint(
                        configName, inputIdx, reader->GetLastFilePos());
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

    for (auto it = mAddedInputs.begin(); it != mAddedInputs.end();) {
        const auto& configInfo = it->first;
        const auto& ctx = it->second;
        if (!ctx) {
            it = mAddedInputs.erase(it);
            continue;
        }

        // 获取Pipeline的时间值
        uint32_t startTime = 0;
        uint32_t expireTime = 0;
        if (ctx->HasValidPipeline()) {
            const auto& pipeline = ctx->GetPipeline();
            startTime = pipeline.GetOnetimeStartTime().value_or(0);
            expireTime = pipeline.GetOnetimeExpireTime().value_or(0);
        }

        // 获取文件列表
        optional<vector<filesystem::path>> files;
        auto fileDiscoveryOpts = mInputFileDiscoveryConfigsMap.find(configInfo)->second.first;
        if (fileDiscoveryOpts->IsContainerDiscoveryEnabled() && !ContainerManager::GetInstance()->IsReady()) {
            // 如果容器发现模块未准备好，跳过本次，等待下次重试
            ++it;
            continue;
        }
        auto fileContainerMetas = mFileContainerMetaMap.find(configInfo)->second;
        if (ctx->IsOnetimePipelineRunningBeforeStart()) {
            files = GetFiles(fileDiscoveryOpts, ctx, fileContainerMetas);
        }
        InputStaticFileCheckpointManager::GetInstance()->CreateCheckpoint(
            configInfo.first, configInfo.second, files, startTime, expireTime, fileContainerMetas);
        mPipelineNameReadersMap.emplace(configInfo.first, make_pair(configInfo.second, LogFileReaderPtr()));
        it = mAddedInputs.erase(it);
    }

    SET_GAUGE(mActiveInputsTotalGauge, mPipelineNameReadersMap.size());
}

bool StaticFileServer::GetCurrentContainerInfoForPath(
    const string& configName, size_t idx, const string& hostPath, string* outContainerID, bool* outStopped) const {
    if (!outContainerID || !outStopped) {
        return false;
    }
    *outContainerID = "";
    *outStopped = true;
    FileDiscoveryConfig config = GetFileDiscoveryConfig(configName, idx);
    if (!config.first) {
        return false;
    }
    if (!config.first->IsContainerDiscoveryEnabled()) {
        return false;
    }
    const auto& containerInfos = config.first->GetContainerInfo();
    if (!containerInfos || containerInfos->empty()) {
        return true;
    }
    for (const auto& info : *containerInfos) {
        if (!info.mRawContainerInfo) {
            continue;
        }
        for (const auto& realBaseDir : info.mRealBaseDirs) {
            if (realBaseDir.empty()) {
                continue;
            }
            if (hostPath.size() >= realBaseDir.size() && hostPath.compare(0, realBaseDir.size(), realBaseDir) == 0
                && (realBaseDir.size() == hostPath.size() || hostPath[realBaseDir.size()] == '/')) {
                *outContainerID = info.mRawContainerInfo->mID;
                *outStopped = info.mRawContainerInfo->mStopped;
                return true;
            }
        }
    }
    return true;
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

vector<filesystem::path>
StaticFileServer::GetFiles(const FileDiscoveryOptions* fileDiscoveryOpts,
                           const CollectionPipelineContext* ctx,
                           std::unordered_map<std::string, FileCheckpoint::ContainerMeta>& fileContainerMetas) {
    vector<filesystem::path> res;
    const auto& pathInfos = fileDiscoveryOpts->GetBasePathInfos();
    fileContainerMetas.clear();

    if (!fileDiscoveryOpts->IsContainerDiscoveryEnabled()) {
        set<DevInode> visitedDirs;
        // Process each path configuration
        for (const auto& pathInfo : pathInfos) {
            vector<filesystem::path> baseDirs;
            if (pathInfo.hasWildcard()) {
                GetValidBaseDirs(pathInfo, pathInfo.wildcardPaths[0], 0, baseDirs);
                if (baseDirs.empty()) {
                    LOG_WARNING(sLogger,
                                ("no files found", "base dir path invalid")("base dir", pathInfo.basePath)(
                                    "config", ctx->GetConfigName()));
                    continue;
                }
            } else {
                baseDirs.emplace_back(pathInfo.basePath);
            }

            for (const auto& dir : baseDirs) {
                if (IsValidDir(dir)) {
                    GetFiles(fileDiscoveryOpts,
                             dir,
                             fileDiscoveryOpts->mMaxDirSearchDepth,
                             &pathInfo,
                             nullptr,
                             visitedDirs,
                             res);
                }
            }
        }
        LOG_INFO(sLogger, ("total files cnt", res.size())("files", ToString(res))("config", ctx->GetConfigName()));
    } else {
        // TODO: support symlink in container
        set<DevInode> visitedDirs;
        for (const auto& item : *fileDiscoveryOpts->GetContainerInfo()) {
            const string& containerID = item.mRawContainerInfo->mID;
            // 遍历配置路径和对应的容器真实路径
            for (size_t idx = 0; idx < pathInfos.size(); ++idx) {
                if (idx >= item.mRealBaseDirs.size())
                    break;

                const auto& pathInfo = pathInfos[idx];
                const string& realBaseDir = item.mRealBaseDirs[idx];

                if (realBaseDir.empty())
                    continue;

                vector<filesystem::path> baseDirs;
                if (pathInfo.hasWildcard()) {
                    GetValidBaseDirs(pathInfo, realBaseDir, 0, baseDirs);
                    if (baseDirs.empty()) {
                        LOG_DEBUG(sLogger,
                                  ("no files found", "base dir path invalid")("container id", containerID)(
                                      "real base dir", realBaseDir)("config", ctx->GetConfigName()));
                        continue;
                    }
                } else {
                    baseDirs.emplace_back(realBaseDir);
                }

                auto prevCnt = res.size();
                for (const auto& dir : baseDirs) {
                    if (IsValidDir(dir)) {
                        GetFiles(fileDiscoveryOpts,
                                 dir,
                                 fileDiscoveryOpts->mMaxDirSearchDepth,
                                 &pathInfo,
                                 &realBaseDir,
                                 visitedDirs,
                                 res);
                    }
                }
                // 记录新发现的文件与容器元信息（FileCheckpoint::ContainerMeta，含容器内路径）
                const auto* raw = item.mRawContainerInfo.get();
                for (size_t i = prevCnt; i < res.size(); ++i) {
                    const string& hostPath = res[i].string();
                    FileCheckpoint::ContainerMeta meta;
                    meta.mContainerID = raw->mID;
                    meta.mContainerName = raw->mName;
                    meta.mPodName = raw->mK8sInfo.mPod;
                    meta.mNamespace = raw->mK8sInfo.mNamespace;
                    meta.mContainerPath = pathInfo.basePath + hostPath.substr(realBaseDir.size());
                    fileContainerMetas[hostPath] = std::move(meta);
                }
                if (res.size() > prevCnt) {
                    LOG_INFO(sLogger,
                             ("container files cnt", res.size() - prevCnt)("container id", containerID)(
                                 "real base dir", realBaseDir)("files", ToString(res))("config", ctx->GetConfigName()));
                } else {
                    LOG_DEBUG(sLogger,
                              ("no files found, container id",
                               containerID)("real base dir", realBaseDir)("config", ctx->GetConfigName()));
                }
            }
        }
        LOG_INFO(sLogger, ("total files cnt", res.size())("config", ctx->GetConfigName()));
    }
    return res;
}

void StaticFileServer::GetFiles(const FileDiscoveryOptions* fileDiscoveryOpts,
                                const filesystem::path& dir,
                                uint32_t depth,
                                const BasePathInfo* pathInfo,
                                const std::string* containerBaseDir,
                                std::set<DevInode>& visitedDir,
                                std::vector<std::filesystem::path>& files) {
    error_code ec;
    for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
        const auto& path = entry.path();
        auto pathStr = path.string();
        if (containerBaseDir && pathInfo) {
            // Map container path to config path
            pathStr = pathInfo->basePath + pathStr.substr(containerBaseDir->size());
        }
        const auto& status = entry.status();
        if (filesystem::is_regular_file(status)) {
            const auto& filename = path.filename().string();
            if (pathInfo) {
                // Use the specific pathInfo's filePattern if provided, otherwise check all patterns
                bool filenameMatched = fileDiscoveryOpts->IsFilenameMatched(filename, *pathInfo);

                if (filenameMatched && !fileDiscoveryOpts->IsFilenameInBlacklist(filename)
                    && !fileDiscoveryOpts->IsFilepathInBlacklist(pathStr)) {
                    files.emplace_back(path);
                }
            }
        } else if (filesystem::is_directory(status)) {
            auto devInode = GetFileDevInode(path.string());
            if (!devInode.IsValid() || visitedDir.find(devInode) != visitedDir.end()) {
                // avoid loop
                continue;
            }
            visitedDir.emplace(devInode);
            if (depth > 0 && !AppConfig::GetInstance()->IsHostPathMatchBlacklist(path.string())
                && !fileDiscoveryOpts->IsDirectoryInBlacklist(pathStr)) {
                GetFiles(fileDiscoveryOpts, path, depth - 1, pathInfo, containerBaseDir, visitedDir, files);
            }
        }
    }
}

void StaticFileServer::GetValidBaseDirs(const BasePathInfo& pathInfo,
                                        const filesystem::path& dir,
                                        uint32_t depth,
                                        vector<filesystem::path>& filepaths) {
    const auto& wildcardPaths = pathInfo.wildcardPaths;
    bool finish = false;
    if (depth + 2 == wildcardPaths.size()) {
        finish = true;
    }

    if (depth == 0 && !IsValidDir(wildcardPaths[depth])) {
        return;
    }

    const auto& subdir = pathInfo.constWildcardPaths[depth];
    if (!subdir.empty()) {
        auto path = dir / subdir;
        error_code ec;
        filesystem::file_status s = filesystem::status(path, ec);
        if (ec || !filesystem::exists(s) || !filesystem::is_directory(s)) {
            return;
        }
        if (finish) {
            filepaths.emplace_back(path);
        } else {
            GetValidBaseDirs(pathInfo, path, depth + 1, filepaths);
        }
    } else {
        auto pattern = filesystem::path(wildcardPaths[depth + 1]).filename();
        error_code ec;
        for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
            const auto& path = entry.path();
            const auto& status = entry.status();
            if (filesystem::is_directory(status)
                && (fnmatch(pattern.string().c_str(), path.filename().string().c_str(), FNM_PATHNAME) == 0)) {
                if (finish) {
                    filepaths.emplace_back(path);
                } else {
                    GetValidBaseDirs(pathInfo, path, depth + 1, filepaths);
                }
            }
        }
    }
}

bool StaticFileServer::IsValidDir(const filesystem::path& dir) {
    error_code ec;
    filesystem::file_status s = filesystem::status(dir, ec);
    if (ec) {
        LOG_WARNING(sLogger,
                    ("failed to get base dir path info",
                     "skip")("dir path", dir.string())("error code", ec.value())("error msg", ec.message()));
        return false;
    }
    if (!filesystem::exists(s)) {
        LOG_WARNING(sLogger, ("base dir path not existed", "skip")("dir path", dir.string()));
        return false;
    }
    if (!filesystem::is_directory(s)) {
        LOG_WARNING(sLogger, ("base dir path is not a directory", "skip")("dir path", dir.string()));
        return false;
    }
    return true;
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
