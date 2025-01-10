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

#include "plugin/processor/inner/ProcessorTagNative.h"

#include <vector>

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "common/MachineInfoUtil.h"
#include "constants/EntityConstants.h"
#include "constants/TagConstants.h"
#include "models/PipelineEventGroup.h"
#include "monitor/Monitor.h"
#include "pipeline/Pipeline.h"
#include "protobuf/sls/sls_logs.pb.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

DECLARE_FLAG_STRING(ALIYUN_LOG_FILE_TAGS);

using namespace std;

namespace logtail {

const string ProcessorTagNative::sName = "processor_tag_native";

bool ProcessorTagNative::Init(const Json::Value& config) {
    mPipelineMetaTagKey = mContext->GetGlobalConfig().mPipelineMetaTagKey;
#ifdef __ENTERPRISE__
    mEnableAgentEnvMetaTagControl = mContext->GetGlobalConfig().mEnableAgentEnvMetaTagControl;
    mAgentEnvMetaTagKey = mContext->GetGlobalConfig().mAgentEnvMetaTagKey;
#endif
    return true;
}

void ProcessorTagNative::Process(PipelineEventGroup& logGroup) {
#ifdef __ENTERPRISE__
    addDefaultAddedTag(logGroup, TagKey::AGENT_TAG, EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet());
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
    addDefaultAddedTag(logGroup, TagKey::HOST_NAME, LoongCollectorMonitor::GetInstance()->mHostname);
    addDefaultAddedTag(logGroup, TagKey::HOST_IP, LoongCollectorMonitor::GetInstance()->mIpAddr);

    auto entity = InstanceIdentity::Instance()->GetEntity();
    if (entity != nullptr) {
        addOptionalTag(logGroup, TagKey::HOST_ID, entity->GetHostID());
#ifdef __ENTERPRISE__
        ECSMeta meta = entity->GetECSMeta();
        const string cloudProvider
            = meta.GetInstanceID().empty() ? DEFAULT_VALUE_DOMAIN_INFRA : DEFAULT_VALUE_DOMAIN_ACS;
#else
        const string cloudProvider = DEFAULT_VALUE_DOMAIN_INFRA;
#endif
        addOptionalTag(logGroup, TagKey::CLOUD_PROVIDER, cloudProvider);
    }

    auto sb = logGroup.GetSourceBuffer()->CopyString(Application::GetInstance()->GetUUID());
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_MACHINE_UUID, StringView(sb.data, sb.size));
    static const vector<sls_logs::LogTag>& sEnvTags = AppConfig::GetInstance()->GetEnvTags();
    if (!sEnvTags.empty()) {
        for (size_t i = 0; i < sEnvTags.size(); ++i) {
#ifdef __ENTERPRISE__
            if (mEnableAgentEnvMetaTagControl) {
                auto envTagKey = sEnvTags[i].key();
                if (mAgentEnvMetaTagKey.find(envTagKey) != mAgentEnvMetaTagKey.end()) {
                    if (!mAgentEnvMetaTagKey[envTagKey].empty()) {
                        logGroup.SetTagNoCopy(mAgentEnvMetaTagKey[envTagKey], sEnvTags[i].value());
                    }
                }
                continue;
            }
#endif
            logGroup.SetTagNoCopy(sEnvTags[i].key(), sEnvTags[i].value());
        }
    }
}

bool ProcessorTagNative::IsSupportedEvent(const PipelineEventPtr& /*e*/) const {
    return true;
}

void ProcessorTagNative::addDefaultAddedTag(PipelineEventGroup& logGroup, TagKey tagKey, const string& value) const {
    auto sb = logGroup.GetSourceBuffer()->CopyString(value);
    auto it = mPipelineMetaTagKey.find(tagKey);
    if (it != mPipelineMetaTagKey.end()) {
        if (!it->second.empty()) {
            if (it->second == DEFAULT_CONFIG_TAG_KEY_VALUE) {
                logGroup.SetTagNoCopy(TagKeyToString(tagKey), StringView(sb.data, sb.size));
            } else {
                logGroup.SetTagNoCopy(it->second, StringView(sb.data, sb.size));
            }
        }
        // empty value means delete
    } else {
        logGroup.SetTagNoCopy(TagKeyToString(tagKey), StringView(sb.data, sb.size));
    }
}

void ProcessorTagNative::addOptionalTag(PipelineEventGroup& logGroup, TagKey tagKey, const string& value) const {
    auto sb = logGroup.GetSourceBuffer()->CopyString(value);
    auto it = mPipelineMetaTagKey.find(tagKey);
    if (it != mPipelineMetaTagKey.end()) {
        if (!it->second.empty()) {
            if (it->second == DEFAULT_CONFIG_TAG_KEY_VALUE) {
                logGroup.SetTagNoCopy(TagKeyToString(tagKey), StringView(sb.data, sb.size));
            } else {
                logGroup.SetTagNoCopy(it->second, StringView(sb.data, sb.size));
            }
        }
        // empty value means delete
    }
}

void ProcessorTagNative::addOptionalTag(PipelineEventGroup& logGroup, TagKey tagKey, StringView value) const {
    auto it = mPipelineMetaTagKey.find(tagKey);
    if (it != mPipelineMetaTagKey.end()) {
        if (!it->second.empty()) {
            if (it->second == DEFAULT_CONFIG_TAG_KEY_VALUE) {
                logGroup.SetTagNoCopy(TagKeyToString(tagKey), value);
            } else {
                logGroup.SetTagNoCopy(it->second, value);
            }
        }
        // empty value means delete
    }
}

} // namespace logtail
