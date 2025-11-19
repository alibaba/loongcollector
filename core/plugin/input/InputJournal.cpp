/*
 * Copyright 2024 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InputJournal.h"

#include "collection_pipeline/CollectionPipeline.h"
#include "journal_server/JournalServer.h"
#include "logger/Logger.h"

namespace logtail {

const std::string InputJournal::sName = "input_journal";

InputJournal::InputJournal() : mShutdown(false) {
}

InputJournal::~InputJournal() {
    Stop(true);
}

bool InputJournal::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    (void)optionalGoPipeline; // Suppress unused parameter warning

    mConfigJson = config;

    return true;
}

bool InputJournal::Start() {
    if (mShutdown) {
        LOG_WARNING(sLogger,
                    ("InputJournal already shutdown",
                     "cannot start")("config", mContext ? mContext->GetConfigName() : "unknown")("idx", mIndex));
        return false;
    }

    if (!mContext) {
        LOG_ERROR(sLogger, ("InputJournal Start called without context", "cannot start"));
        return false;
    }

    // 同一Config配置中只能有一个 InputJournal 输入
    if (mContext->HasValidPipeline()) {
        const auto& inputs = mContext->GetPipeline().GetInputs();
        int inputJournalCount = 0;
        for (const auto& input : inputs) {
            if (input && input->GetPlugin() && input->GetPlugin()->Name() == sName) {
                inputJournalCount++;
            }
        }
        if (inputJournalCount > 1) {
            LOG_ERROR(sLogger,
                      ("InputJournal: multiple input_journal instances found in the same config",
                       "only one input_journal is allowed per config")("config", mContext->GetConfigName())(
                          "count", inputJournalCount)("idx", mIndex));
            return false;
        }
    }

    LOG_INFO(sLogger, ("starting InputJournal", "")("config", mContext->GetConfigName())("idx", mIndex));

    JournalServer::GetInstance()->Init();

    JournalConfig config = JournalConfig::ParseFromJson(mConfigJson, mContext);

    int fixedCount = config.ValidateAndFixConfig();
    if (fixedCount > 0) {
        LOG_WARNING(sLogger,
                    ("journal config fixed", "some values were corrected")("config", mContext->GetConfigName())(
                        "idx", mIndex)("fixed_count", fixedCount));
    }

    if (!config.IsValid()) {
        LOG_ERROR(sLogger,
                  ("invalid journal config", "cannot start")("config", mContext->GetConfigName())("idx", mIndex));
        return false;
    }

    // 注册到JournalServer
    JournalServer::GetInstance()->AddJournalInput(mContext->GetConfigName(), config);

    LOG_INFO(sLogger,
             ("InputJournal registered with JournalServer", "")("config", mContext->GetConfigName())("idx", mIndex));

    return true;
}

bool InputJournal::Stop(bool isPipelineRemoving) {
    bool expected = false;
    if (!mShutdown.compare_exchange_strong(expected, true)) {
        return true;
    }

    if (!mContext) {
        LOG_WARNING(sLogger, ("InputJournal Stop called without context", "skipping cleanup"));
        return true;
    }

    JournalServer::GetInstance()->RemoveJournalInput(mContext->GetConfigName());
    
    if (isPipelineRemoving) {
        LOG_INFO(sLogger, ("InputJournal removed with checkpoint cleanup", "")("config", mContext->GetConfigName()));
        if (!JournalServer::GetInstance()->HasRegisteredPlugins()) {
            JournalServer::GetInstance()->Stop();
        }
    } else {
        LOG_INFO(sLogger,
                 ("InputJournal stopped for config update, checkpoint preserved", "")("config", mContext->GetConfigName()));
    }

    return true;
}

} // namespace logtail
