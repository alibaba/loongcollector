/*
 * Copyright 2023 iLogtail Authors
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

#include "processor/inner/ProcessorTagNative.h"

#include <vector>

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "log_pb/sls_logs.pb.h"
#include "pipeline/Pipeline.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

DECLARE_FLAG_STRING(ALIYUN_LOG_FILE_TAGS);

using namespace std;

namespace logtail {

const string ProcessorTagNative::sName = "processor_tag_native";

bool ProcessorTagNative::Init(const Json::Value& config) {
    return true;
}

void ProcessorTagNative::Process(PipelineEventGroup& logGroup) {
    // group level
    StringView filePath = logGroup.GetMetadata(EventGroupMetaKey::LOG_FILE_PATH);
    if (!filePath.empty()) {
        logGroup.SetTagNoCopy(LOG_RESERVED_KEY_PATH, filePath.substr(0, 511));
    }

    // process level
#ifdef __ENTERPRISE__
    string agentTag = EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet();
    if (!agentTag.empty()) {
        auto sb = logGroup.GetSourceBuffer()->CopyString(agentTag);
        logGroup.SetTagNoCopy(LOG_RESERVED_KEY_USER_DEFINED_ID, StringView(sb.data, sb.size));
    }
#endif

    if (!STRING_FLAG(ALIYUN_LOG_FILE_TAGS).empty()) {
        vector<sls_logs::LogTag>& fileTags = AppConfig::GetInstance()->GetFileTags();
        if (!fileTags.empty()) { // reloadable, so we must get it every time and copy value
            for (size_t i = 0; i < fileTags.size(); ++i) {
                logGroup.SetTag(fileTags[i].key(), fileTags[i].value());
            }
        }
    }

    if (mContext->GetPipeline().IsFlushingThroughGoPipeline()) {
        return;
    }

    // process level
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_HOSTNAME, LogFileProfiler::mHostname);
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LogFileProfiler::mIpAddr);
    auto sb = logGroup.GetSourceBuffer()->CopyString(Application::GetInstance()->GetUUID());
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_MACHINE_UUID, StringView(sb.data, sb.size));
    static const vector<sls_logs::LogTag>& sEnvTags = AppConfig::GetInstance()->GetEnvTags();
    if (!sEnvTags.empty()) {
        for (size_t i = 0; i < sEnvTags.size(); ++i) {
            logGroup.SetTagNoCopy(sEnvTags[i].key(), sEnvTags[i].value());
        }
    }
}

bool ProcessorTagNative::IsSupportedEvent(const PipelineEventPtr& /*e*/) const {
    return true;
}

} // namespace logtail
