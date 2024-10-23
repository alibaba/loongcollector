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

#include "file_server/FileServer.h"

#include "checkpoint/CheckPointManager.h"
#include "common/Flags.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "file_server/ConfigManager.h"
#include "file_server/EventDispatcher.h"
#include "file_server/event_handler/LogInput.h"
#include "file_server/polling/PollingDirFile.h"
#include "file_server/polling/PollingModify.h"
#include "plugin/input/InputFile.h"

DEFINE_FLAG_BOOL(enable_polling_discovery, "", true);

using namespace std;

namespace logtail {

FileServer::FileServer() {
    WriteMetrics::GetInstance()->PrepareMetricsRecordRef(
        mMetricsRecordRef,
        {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_FILE_SERVER},
         {METRIC_LABEL_KEY_METRIC_CATEGORY, METRIC_LABEL_KEY_METRIC_CATEGORY_RUNNER}});
}

// 启动文件服务，包括加载配置、处理检查点、注册事件等
void FileServer::Start() {
    ConfigManager::GetInstance()->LoadDockerConfig();
    CheckPointManager::Instance()->LoadCheckPoint();
    ConfigManager::GetInstance()->RegisterHandlers();
    LOG_INFO(sLogger, ("watch dirs", "succeeded"));
    EventDispatcher::GetInstance()->AddExistedCheckPointFileEvents();
    // the dump time must be reset after dir registration, since it may take long on NFS.
    CheckPointManager::Instance()->ResetLastDumpTime();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingModify::GetInstance()->Start();
        PollingDirFile::GetInstance()->Start();
    }
    LogInput::GetInstance()->Start();
    LOG_INFO(sLogger, ("file server", "started"));
}

// 暂停文件服务，根据配置更新标志来决定是否要执行相关的清理和保存操作
void FileServer::Pause(bool isConfigUpdate) {
    PauseInner();
    if (isConfigUpdate) {
        EventDispatcher::GetInstance()->DumpAllHandlersMeta(true);
        CheckPointManager::Instance()->DumpCheckPointToLocal();
        EventDispatcher::GetInstance()->ClearBrokenLinkSet();
        PollingDirFile::GetInstance()->ClearCache();
        ConfigManager::GetInstance()->ClearFilePipelineMatchCache();
    }
}

// 暂停文件服务的内部实现，记录日志并处理暂停逻辑
void FileServer::PauseInner() {
    LOG_INFO(sLogger, ("file server pause", "starts"));
    // cache must be cleared at last, since logFileReader dump still requires the cache
    auto holdOnStart = GetCurrentTimeInMilliSeconds();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingDirFile::GetInstance()->HoldOn();
        PollingModify::GetInstance()->HoldOn();
    }
    LogInput::GetInstance()->HoldOn();
    auto holdOnCost = GetCurrentTimeInMilliSeconds() - holdOnStart;
    if (holdOnCost >= 60 * 1000) {
        LogtailAlarm::GetInstance()->SendAlarm(HOLD_ON_TOO_SLOW_ALARM,
                                               "Pausing file server took " + ToString(holdOnCost) + "ms");
    }
    LOG_INFO(sLogger, ("file server pause", "succeeded")("cost", ToString(holdOnCost) + "ms"));
}

// 恢复文件服务，重新注册事件处理程序和恢复日志输入
void FileServer::Resume(bool isConfigUpdate) {
    if (isConfigUpdate) {
        ClearContainerInfo();
        ConfigManager::GetInstance()->DoUpdateContainerPaths();
        ConfigManager::GetInstance()->SaveDockerConfig();
    }

    LOG_INFO(sLogger, ("file server resume", "starts"));
    ConfigManager::GetInstance()->RegisterHandlers();
    LOG_INFO(sLogger, ("watch dirs", "succeeded"));
    if (isConfigUpdate) {
        EventDispatcher::GetInstance()->AddExistedCheckPointFileEvents();
    }
    LogInput::GetInstance()->Resume();
    if (BOOL_FLAG(enable_polling_discovery)) {
        PollingModify::GetInstance()->Resume();
        PollingDirFile::GetInstance()->Resume();
    }
    LOG_INFO(sLogger, ("file server resume", "succeeded"));
}

// 停止文件服务，将事件处理程序的元数据以及检查点数据保存到本地
void FileServer::Stop() {
    PauseInner();
    EventDispatcher::GetInstance()->DumpAllHandlersMeta(false);
    CheckPointManager::Instance()->DumpCheckPointToLocal();
}

// 获取给定名称的文件发现配置
FileDiscoveryConfig FileServer::GetFileDiscoveryConfig(const string& name) const {
    ReadLock lock(mReadWriteLock);
    auto itr = mPipelineNameFileDiscoveryConfigsMap.find(name);
    if (itr != mPipelineNameFileDiscoveryConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

// 添加文件发现配置
void FileServer::AddFileDiscoveryConfig(const string& name, FileDiscoveryOptions* opts, const PipelineContext* ctx) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameFileDiscoveryConfigsMap[name] = make_pair(opts, ctx);
}

// 移除给定名称的文件发现配置
void FileServer::RemoveFileDiscoveryConfig(const string& name) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameFileDiscoveryConfigsMap.erase(name);
}

// 获取给定名称的文件读取器配置
FileReaderConfig FileServer::GetFileReaderConfig(const string& name) const {
    ReadLock lock(mReadWriteLock);
    auto itr = mPipelineNameFileReaderConfigsMap.find(name);
    if (itr != mPipelineNameFileReaderConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

// 添加文件读取器配置
void FileServer::AddFileReaderConfig(const string& name, const FileReaderOptions* opts, const PipelineContext* ctx) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameFileReaderConfigsMap[name] = make_pair(opts, ctx);
}

// 移除给定名称的文件读取器配置
void FileServer::RemoveFileReaderConfig(const string& name) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameFileReaderConfigsMap.erase(name);
}

// 获取给定名称的多行配置
MultilineConfig FileServer::GetMultilineConfig(const string& name) const {
    ReadLock lock(mReadWriteLock);
    auto itr = mPipelineNameMultilineConfigsMap.find(name);
    if (itr != mPipelineNameMultilineConfigsMap.end()) {
        return itr->second;
    }
    return make_pair(nullptr, nullptr);
}

// 添加多行配置
void FileServer::AddMultilineConfig(const string& name, const MultilineOptions* opts, const PipelineContext* ctx) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameMultilineConfigsMap[name] = make_pair(opts, ctx);
}

// 移除给定名称的多行配置
void FileServer::RemoveMultilineConfig(const string& name) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameMultilineConfigsMap.erase(name);
}

// 保存容器信息
void FileServer::SaveContainerInfo(const string& pipeline, const shared_ptr<vector<ContainerInfo>>& info) {
    WriteLock lock(mReadWriteLock);
    mAllContainerInfoMap[pipeline] = info;
}

// 获取并移除给定管道的容器信息
shared_ptr<vector<ContainerInfo>> FileServer::GetAndRemoveContainerInfo(const string& pipeline) {
    WriteLock lock(mReadWriteLock);
    auto iter = mAllContainerInfoMap.find(pipeline);
    if (iter == mAllContainerInfoMap.end()) {
        return make_shared<vector<ContainerInfo>>();
    }
    auto res = iter->second;
    mAllContainerInfoMap.erase(iter);
    return res;
}

// 清除所有容器信息
void FileServer::ClearContainerInfo() {
    WriteLock lock(mReadWriteLock);
    mAllContainerInfoMap.clear();
}

// 获取插件的指标管理器
PluginMetricManagerPtr FileServer::GetPluginMetricManager(const std::string& name) const {
    ReadLock lock(mReadWriteLock);
    auto itr = mPipelineNamePluginMetricManagersMap.find(name);
    if (itr != mPipelineNamePluginMetricManagersMap.end()) {
        return itr->second;
    }
    return nullptr;
}

// 添加插件的指标管理器
void FileServer::AddPluginMetricManager(const std::string& name, PluginMetricManagerPtr PluginMetricManager) {
    WriteLock lock(mReadWriteLock);
    mPipelineNamePluginMetricManagersMap[name] = PluginMetricManager;
}

// 移除插件的指标管理器
void FileServer::RemovePluginMetricManager(const std::string& name) {
    WriteLock lock(mReadWriteLock);
    mPipelineNamePluginMetricManagersMap.erase(name);
}

// 获取“ReentrantMetricsRecordRef”指标记录对象
ReentrantMetricsRecordRef FileServer::GetOrCreateReentrantMetricsRecordRef(const std::string& name,
                                                                           MetricLabels& labels) {
    PluginMetricManagerPtr filePluginMetricManager = GetPluginMetricManager(name);
    if (filePluginMetricManager != nullptr) {
        return filePluginMetricManager->GetOrCreateReentrantMetricsRecordRef(labels);
    }
    return nullptr;
}

// 释放“ReentrantMetricsRecordRef”指标记录对象
void FileServer::ReleaseReentrantMetricsRecordRef(const std::string& name, MetricLabels& labels) {
    PluginMetricManagerPtr filePluginMetricManager = GetPluginMetricManager(name);
    if (filePluginMetricManager != nullptr) {
        filePluginMetricManager->ReleaseReentrantMetricsRecordRef(labels);
    }
}

// 获取给定名称的“ExactlyOnce”并发级别
uint32_t FileServer::GetExactlyOnceConcurrency(const string& name) const {
    ReadLock lock(mReadWriteLock);
    auto itr = mPipelineNameEOConcurrencyMap.find(name);
    if (itr != mPipelineNameEOConcurrencyMap.end()) {
        return itr->second;
    }
    return 0;
}

// 获取所有配置了“ExactlyOnce”选项的配置名列表
vector<string> FileServer::GetExactlyOnceConfigs() const {
    ReadLock lock(mReadWriteLock);
    vector<string> res;
    for (const auto& item : mPipelineNameEOConcurrencyMap) {
        if (item.second > 0) {
            res.push_back(item.first);
        }
    }
    return res;
}

// 添加“ExactlyOnce”并发配置
void FileServer::AddExactlyOnceConcurrency(const string& name, uint32_t concurrency) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameEOConcurrencyMap[name] = concurrency;
}

// 移除给定名称的“ExactlyOnce”并发配置
void FileServer::RemoveExactlyOnceConcurrency(const string& name) {
    WriteLock lock(mReadWriteLock);
    mPipelineNameEOConcurrencyMap.erase(name);
}

} // namespace logtail
