/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apm/ApmInjectRunner.h"

#include <json/json.h>

#include <filesystem>
#include <fstream>

#include "MachineInfoUtil.h"
#include "apm/Types.h"
#include "common/timer/Timer.h"
#include "logger/Logger.h"

namespace logtail::apm {

namespace fs = std::filesystem;

bool CheckUpdateEvent::Execute() {
    ApmInjectRunner::GetInstance()->CheckUpdates();
    // 重新注册下一次
    ApmInjectRunner::GetInstance()->ScheduleCheckUpdates();
    return true;
}

void ApmInjectRunner::Init() {
    if (mStarted) {
        return;
    }
    // 获取regionId
#ifndef APSARA_UNIT_TEST_MAIN
    if (!FetchECSMeta(mEcsMeta)) {
        LOG_WARNING(sLogger, ("failed to fetch ecs meta", ""));
        mRegionId = "";
    } else {
        mRegionId = mEcsMeta.GetRegionID().to_string();
    }
#else
    mRegionId = "cn-hangzhou";
#endif
    // 先安装 exec hook
    mPackageMgr.InstallExecHook(mRegionId);
    mThreadPool = std::make_unique<ThreadPool>(1);
    mThreadPool->Start();
    Timer::GetInstance()->Init();
    ScheduleCheckUpdates();
    mStarted = true;
}

void ApmInjectRunner::Stop() {
    if (!mStarted) {
        return;
    }
    // 卸载 exec hook
    // TODO @qianlu.kk do we need to un-install??
    mPackageMgr.UninstallExecHook();
    // if (mCheckUpdateTimerEvent) {
    //     mCheckUpdateTimerEvent->SetExecTime(std::chrono::steady_clock::now()); // 让其失效
    //     mCheckUpdateTimerEvent.reset();
    // }
    if (mThreadPool) {
        mThreadPool->Stop();
    }
    mStarted = false;
}

void ApmInjectRunner::ScheduleCheckUpdates() {
    auto interval = mUpdateCheckInterval;
    auto event = std::make_unique<CheckUpdateEvent>(interval);
    Timer::GetInstance()->PushEvent(std::move(event));
}

bool ApmInjectRunner::InjectApmAgent(const TaskPipelineContext* ctx, std::unique_ptr<AttachConfig>&& config) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    std::string configName = ctx->GetConfigName();
    auto commandType = config->mCommandType;
    AttachContextWithRetry ctxWithRetry;
    ctxWithRetry.context = std::make_unique<AttachContext>(std::move(config));
    ctxWithRetry.retryCount = 0;
    ctxWithRetry.lastStatus = ApmAttachStatus::kInProgress;
    if (commandType == CommandType::kInstall) {
        // 只处理当前config，成功后插入mAttachConfigs
        mThreadPool->Add([&]() mutable { injectApmAgentInner(configName, ctxWithRetry, false); });
    } else if (commandType == CommandType::kUpdate) {
        // 只更新当前config
        mThreadPool->Add([&]() mutable { injectApmAgentInner(configName, ctxWithRetry, true); });
    }
    return true;
}

bool ApmInjectRunner::RemoveApmAgent(const TaskPipelineContext* ctx) {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    std::string configName = ctx->GetConfigName();
    auto it = mAttachConfigs.find(configName);
    if (it != mAttachConfigs.end()) {
        auto& ctxWithRetry = it->second;
        mThreadPool->Add([&]() mutable { removeApmAgentInner(configName, ctxWithRetry); });
        mAttachConfigs.erase(it);
    }
    return true;
}

void ApmInjectRunner::injectApmAgentInner(const std::string& configName,
                                          AttachContextWithRetry& ctxWithRetry,
                                          bool isUpdate) {
    auto& attachConfig = ctxWithRetry.context->mAttachConfig;
    fs::path agentPath;
    bool ok = mPackageMgr.PrepareAPMAgent(
        attachConfig->mLanguage, attachConfig->mAppId, mRegionId, attachConfig->mAgentVersion, agentPath);
    if (!ok) {
        ctxWithRetry.lastStatus = ApmAttachStatus::kFailed;
        ctxWithRetry.retryCount++;
        return;
    }
    // 写入rc文件
    for (auto& rule : attachConfig->mMatchRules) {
        if (!mAttachMgr.DoAttach(rule, agentPath, ctxWithRetry.context)) {
            ctxWithRetry.lastStatus = ApmAttachStatus::kFailed;
            ctxWithRetry.retryCount++;
            return;
        }
    }
    ctxWithRetry.lastStatus = ApmAttachStatus::kSucceed;
    ctxWithRetry.retryCount = 0;
    if (!isUpdate) {
        std::lock_guard<std::mutex> lock(mConfigMutex);
        mAttachConfigs[configName] = std::move(ctxWithRetry);
    }
}

void ApmInjectRunner::removeApmAgentInner(const std::string& configName, AttachContextWithRetry& ctxWithRetry) {
    auto& attachConfig = ctxWithRetry.context->mAttachConfig;
    if (attachConfig == nullptr) {
        LOG_WARNING(sLogger, ("failed to get attachConfig", ""));
        return;
    }
    // 删除rc文件
    for (auto& rule : attachConfig->mMatchRules) {
        mAttachMgr.DoDetach(rule);
    }
    // 删除专属agent包
    fs::path dummy;
    mPackageMgr.RemoveAPMAgent(attachConfig->mLanguage, attachConfig->mAppId);
}

void ApmInjectRunner::CheckUpdates() {
    std::lock_guard<std::mutex> lock(mConfigMutex);
    for (auto& it : mAttachConfigs) {
        // [configName, ctxWithRetry]
        const auto& configName = it.first;
        auto& ctxWithRetry = it.second;
        if (ctxWithRetry.lastStatus == ApmAttachStatus::kSucceed) {
            continue;
        }
        if (ctxWithRetry.retryCount >= mMaxRetry) {
            // 超过最大重试次数，停止重试
            continue;
        }
        mThreadPool->Add(
            [this, configName, &ctxWithRetry]() mutable { injectApmAgentInner(configName, ctxWithRetry, false); });
    }
}

} // namespace logtail::apm
