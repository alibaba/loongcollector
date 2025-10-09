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

#include "JournalConnectionInstance.h"
#include "../filter/JournalFilter.h"
#include "logger/Logger.h"

namespace logtail {

//==============================================================================
// JournalConnectionInstance 实现
//==============================================================================

JournalConnectionInstance::JournalConnectionInstance(const std::string& configName, 
                                           size_t idx, 
                                           const JournalConfig& config)
    : mConfigName(configName)
    , mIndex(idx)
    , mConfig(config)
    , mCreateTime(std::chrono::steady_clock::now())
    , mLastResetTime(std::chrono::steady_clock::now())
    , mIsValid(false) {
    
    // 延迟初始化连接，避免在构造函数中调用可能失败的操作
    // initializeConnection(); // 注释掉，改为在GetReader()中延迟初始化
}

JournalConnectionInstance::~JournalConnectionInstance() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mReader) {
        mReader->Close();
    }
    // 连接已销毁
}

std::shared_ptr<SystemdJournalReader> JournalConnectionInstance::GetReader() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 如果连接无效、reader为空，或者reader已关闭，尝试重新初始化
    if (!mIsValid || !mReader || !mReader->IsOpen()) {
        // 延迟初始化连接
        initializeConnection();
    }
    
    return (mIsValid && mReader && mReader->IsOpen()) ? mReader : nullptr;
}

bool JournalConnectionInstance::ShouldReset(int /* resetIntervalSec */) const {
    // 移除自动重置逻辑，连接永远不重建
    // 只检查是否已标记为待重置
    return mPendingReset.load();
}

bool JournalConnectionInstance::ResetConnection() {
    std::lock_guard<std::mutex> lock(mMutex);
    
    // 关闭旧连接
    if (mReader) {
        mReader->Close();
        mReader.reset();
    }
    
    // 重新初始化
    bool success = initializeConnection();
    mLastResetTime = std::chrono::steady_clock::now();
    
    // 重置完成后清除标记
    if (success) {
        ClearResetFlag();
        LOG_INFO(sLogger, ("journal connection reset successfully", "")("config", mConfigName)("idx", mIndex));
    } else {
        LOG_ERROR(sLogger, ("journal connection reset failed", "")("config", mConfigName)("idx", mIndex));
    }
    
    return success;
}

bool JournalConnectionInstance::IsValid() const {
    std::lock_guard<std::mutex> lock(mMutex);
    bool valid = mIsValid && mReader && mReader->IsOpen();
    // 连接有效性检查
    return valid;
}

bool JournalConnectionInstance::initializeConnection() {
    // 此方法在锁内调用，不需要再加锁
    mIsValid = false;
    
    try {
        mReader = std::make_shared<SystemdJournalReader>();
        
        // 设置超时
        mReader->SetTimeout(std::chrono::milliseconds(5000));
        
        // 设置自定义journal路径（如果指定）
        if (!mConfig.journalPaths.empty()) {
            mReader->SetJournalPaths(mConfig.journalPaths);
        }
        
        // 打开journal连接
        if (!mReader->Open()) {
            LOG_ERROR(sLogger, ("failed to open journal", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        // 验证连接是否打开
        if (!mReader->IsOpen()) {
            LOG_ERROR(sLogger, ("journal reader not open after Open() call", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        
        // 应用过滤器
        JournalFilter::FilterConfig filterConfig;
        filterConfig.units = mConfig.units;
        filterConfig.identifiers = mConfig.identifiers;
        filterConfig.matchPatterns = mConfig.matchPatterns;
        filterConfig.enableKernel = mConfig.kernel;
        filterConfig.configName = mConfigName;
        filterConfig.configIndex = mIndex;
        
        if (!JournalFilter::ApplyAllFilters(mReader.get(), filterConfig)) {
            LOG_ERROR(sLogger, ("failed to apply journal filters", "")("config", mConfigName)("idx", mIndex));
            mReader.reset();
            return false;
        }
        
        mIsValid = true;
        LOG_INFO(sLogger, ("journal connection initialized successfully", "")("config", mConfigName)("idx", mIndex));
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("exception during journal connection initialization", e.what())("config", mConfigName)("idx", mIndex));
        mReader.reset();
        return false;
    }
}

//==============================================================================
// 强制重置管理
//==============================================================================

void JournalConnectionInstance::MarkForReset() const {
    mPendingReset.store(true);
}

bool JournalConnectionInstance::IsPendingReset() const {
    return mPendingReset.load();
}

void JournalConnectionInstance::ClearResetFlag() {
    mPendingReset.store(false);
}

} // namespace logtail 