/*
 * Copyright 2024 iLogtail Authors
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

#include "file_server/FileTagOptions.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/ParamExtractor.h"
#include "constants/TagConstants.h"

using namespace std;

namespace logtail {

bool FileTagOptions::Init(const Json::Value& config,
                          const CollectionPipelineContext& context,
                          const string& pluginType,
                          bool enableContainerDiscovery) {
    string errorMsg;

    // Deprecated: should use FileInodeTagKey instead
    // AppendingLogPositionMeta
    bool appendingLogPositionMeta = false;
    if (!GetOptionalBoolParam(config, "AppendingLogPositionMeta", appendingLogPositionMeta, errorMsg)) {
        PARAM_WARNING_DEFAULT(context.GetLogger(),
                              context.GetAlarm(),
                              errorMsg,
                              appendingLogPositionMeta,
                              pluginType,
                              context.GetConfigName(),
                              context.GetProjectName(),
                              context.GetLogstoreName(),
                              context.GetRegion());
    }

    // Tags
    const char* tagKey = "Tags";
    const Json::Value* tagConfig = config.find(tagKey, tagKey + strlen(tagKey));
    if (tagConfig) {
        if (!tagConfig->isObject()) {
            PARAM_WARNING_IGNORE(context.GetLogger(),
                                 context.GetAlarm(),
                                 "param Tags is not of type object",
                                 pluginType,
                                 context.GetConfigName(),
                                 context.GetProjectName(),
                                 context.GetLogstoreName(),
                                 context.GetRegion());
            tagConfig = nullptr;
        }
    }

    // the priority of FileOffsetKey and FileInodeTagKey is higher than appendingLogPositionMeta
    if (config.isMember("FileOffsetKey") || (tagConfig && tagConfig->isMember("FileInodeTagKey"))) {
        ParseTagKey(&config, "FileOffsetKey", TagKey::FILE_OFFSET_KEY, mFileTags, context, pluginType, false);
        ParseTagKey(tagConfig, "FileInodeTagKey", TagKey::FILE_INODE_TAG_KEY, mFileTags, context, pluginType, false);
    } else if (appendingLogPositionMeta) {
        mFileTags[TagKey::FILE_OFFSET_KEY] = GetDefaultTagKeyString(TagKey::FILE_OFFSET_KEY);
        mFileTags[TagKey::FILE_INODE_TAG_KEY] = GetDefaultTagKeyString(TagKey::FILE_INODE_TAG_KEY);
    }
    ParseTagKey(tagConfig, "FilePathTagKey", TagKey::FILE_PATH_TAG_KEY, mFileTags, context, pluginType, true);

    // ContainerDiscovery
    if (enableContainerDiscovery) {
        ParseTagKey(
            tagConfig, "K8sNamespaceTagKey", TagKey::K8S_NAMESPACE_TAG_KEY, mFileTags, context, pluginType, true);
        ParseTagKey(tagConfig, "K8sPodNameTagKey", TagKey::K8S_POD_NAME_TAG_KEY, mFileTags, context, pluginType, true);
        ParseTagKey(tagConfig, "K8sPodUidTagKey", TagKey::K8S_POD_UID_TAG_KEY, mFileTags, context, pluginType, true);
        ParseTagKey(
            tagConfig, "ContainerNameTagKey", TagKey::CONTAINER_NAME_TAG_KEY, mFileTags, context, pluginType, true);
        ParseTagKey(tagConfig, "ContainerIpTagKey", TagKey::CONTAINER_IP_TAG_KEY, mFileTags, context, pluginType, true);
        ParseTagKey(tagConfig,
                    "ContainerImageNameTagKey",
                    TagKey::CONTAINER_IMAGE_NAME_TAG_KEY,
                    mFileTags,
                    context,
                    pluginType,
                    true);
    }

    return true;
}

StringView FileTagOptions::GetFileTagKeyName(TagKey key) const {
    auto it = mFileTags.find(key);
    if (it != mFileTags.end()) {
        // FileTagOption will not be deconstructed or changed before all event be sent
        return StringView(it->second.c_str(), it->second.size());
    }
    return StringView();
}

bool FileTagOptions::EnableLogPositionMeta() {
    auto offsetIter = mFileTags.find(TagKey::FILE_OFFSET_KEY);
    if (offsetIter != mFileTags.end() && !offsetIter->second.empty()) {
        return true;
    }
    auto inodeIter = mFileTags.find(TagKey::FILE_INODE_TAG_KEY);
    if (inodeIter != mFileTags.end() && !inodeIter->second.empty()) {
        return true;
    }
    return false;
}

} // namespace logtail
