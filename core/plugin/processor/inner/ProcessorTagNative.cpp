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

#include "json/value.h"

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "common/Flags.h"
#include "common/MachineInfoUtil.h"
#include "common/ParamExtractor.h"
#include "constants/EntityConstants.h"
#include "constants/TagConstants.h"
#include "models/PipelineEventGroup.h"
#include "monitor/Monitor.h"
#include "protobuf/sls/sls_logs.pb.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif

DECLARE_FLAG_STRING(ALIYUN_LOG_FILE_TAGS);

using namespace std;

namespace logtail {

const string ProcessorTagNative::sName = "processor_tag_native";

bool ProcessorTagNative::Init(const Json::Value& config) {
    string errorMsg;
    // PipelineMetaTagKey
    const char* pipelineTagKey = "PipelineMetaTagKey";
    const Json::Value* tagConfig = config.find(pipelineTagKey, pipelineTagKey + strlen(pipelineTagKey));
    if (tagConfig) {
        if (!tagConfig->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "param PipelineMetaTagKey is not of type object",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
            tagConfig = nullptr;
        }
    }
    ParseTagKey(tagConfig, "HOST_NAME", TagKey::HOST_NAME_TAG_KEY, mPipelineMetaTagKey, *mContext, sName, true);

#ifdef __ENTERPRISE__
    ParseTagKey(tagConfig, "HOST_ID", TagKey::HOST_ID_TAG_KEY, mPipelineMetaTagKey, *mContext, sName, false);
    ParseTagKey(
        tagConfig, "CLOUD_PROVIDER", TagKey::CLOUD_PROVIDER_TAG_KEY, mPipelineMetaTagKey, *mContext, sName, false);
    ParseTagKey(tagConfig, "AGENT_TAG", TagKey::AGENT_TAG_TAG_KEY, mPipelineMetaTagKey, *mContext, sName, true);
#else
    ParseTagKey(tagConfig, "HOST_IP", TagKey::HOST_IP_TAG_KEY, mPipelineMetaTagKey, *mContext, sName, true);
#endif

    // AgentEnvMetaTagKey
    const std::string envTagKey = "AgentEnvMetaTagKey";
    const Json::Value* itr = config.find(envTagKey.c_str(), envTagKey.c_str() + envTagKey.length());
    if (!itr) {
        mAppendingAllEnvMetaTag = true;
    } else {
        if (!GetOptionalMapParam(config, "AgentEnvMetaTagKey", mAgentEnvMetaTagKey, errorMsg)) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 errorMsg,
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        }
    }
    return true;
}

// should keep same with Go addAllConfigurableTags
void ProcessorTagNative::Process(PipelineEventGroup& logGroup) {
    AddTag(logGroup, TagKey::HOST_NAME_TAG_KEY, LoongCollectorMonitor::GetInstance()->mHostname);
#ifdef __ENTERPRISE__
    auto entity = InstanceIdentity::Instance()->GetEntity();
    if (entity != nullptr) {
        AddTag(logGroup, TagKey::HOST_ID_TAG_KEY, entity->GetHostID());
        ECSMeta meta = entity->GetECSMeta();
        const string cloudProvider
            = meta.GetInstanceID().empty() ? DEFAULT_VALUE_DOMAIN_INFRA : DEFAULT_VALUE_DOMAIN_ACS;
        AddTag(logGroup, TagKey::CLOUD_PROVIDER_TAG_KEY, cloudProvider);
    }
    AddTag(logGroup, TagKey::AGENT_TAG_TAG_KEY, EnterpriseConfigProvider::GetInstance()->GetUserDefinedIdSet());
#else
    AddTag(logGroup, TagKey::HOST_IP_TAG_KEY, LoongCollectorMonitor::GetInstance()->mIpAddr);
#endif

    if (!STRING_FLAG(ALIYUN_LOG_FILE_TAGS).empty()) {
        vector<sls_logs::LogTag>& fileTags = AppConfig::GetInstance()->GetFileTags();
        if (!fileTags.empty()) { // reloadable, so we must get it every time and copy value
            for (size_t i = 0; i < fileTags.size(); ++i) {
                logGroup.SetTag(fileTags[i].key(), fileTags[i].value());
            }
        }
    }
    static const vector<sls_logs::LogTag>& sEnvTags = AppConfig::GetInstance()->GetEnvTags();
    if (!sEnvTags.empty()) {
        for (size_t i = 0; i < sEnvTags.size(); ++i) {
            if (mAgentEnvMetaTagKey.empty() && mAppendingAllEnvMetaTag) {
                logGroup.SetTagNoCopy(sEnvTags[i].key(), sEnvTags[i].value());
            } else {
                auto envTagKey = sEnvTags[i].key();
                auto iter = mAgentEnvMetaTagKey.find(envTagKey);
                if (iter != mAgentEnvMetaTagKey.end()) {
                    if (!iter->second.empty()) {
                        logGroup.SetTagNoCopy(iter->second, sEnvTags[i].value());
                    }
                }
            }
        }
    }

    if (mContext->GetPipeline().IsFlushingThroughGoPipeline()) {
        return;
    }
    // machine_uuid is used in serializer
    // When flushing through Go pipeline, it will skip serializer, add a new unexpected tag
    auto sb = logGroup.GetSourceBuffer()->CopyString(Application::GetInstance()->GetUUID());
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_MACHINE_UUID, StringView(sb.data, sb.size));
    logGroup.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LoongCollectorMonitor::mIpAddr);
}

bool ProcessorTagNative::IsSupportedEvent(const PipelineEventPtr& /*e*/) const {
    return true;
}

void ProcessorTagNative::AddTag(PipelineEventGroup& logGroup, TagKey tagKey, const string& value) const {
    auto it = mPipelineMetaTagKey.find(tagKey);
    if (it != mPipelineMetaTagKey.end()) {
        if (!it->second.empty()) {
            auto sb = logGroup.GetSourceBuffer()->CopyString(value);
            logGroup.SetTagNoCopy(it->second, StringView(sb.data, sb.size));
        }
        // empty value means delete
    }
}


void ProcessorTagNative::AddTag(PipelineEventGroup& logGroup, TagKey tagKey, StringView value) const {
    auto it = mPipelineMetaTagKey.find(tagKey);
    if (it != mPipelineMetaTagKey.end()) {
        if (!it->second.empty()) {
            logGroup.SetTagNoCopy(it->second, value);
        }
        // empty value means delete
    }
}

} // namespace logtail
