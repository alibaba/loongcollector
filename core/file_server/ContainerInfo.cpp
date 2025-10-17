// Copyright 2022 iLogtail Authors
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

#include "file_server/ContainerInfo.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "common/StringTools.h"
#include "logger/Logger.h"
#include "models/PipelineEventGroup.h"

namespace logtail {

static const std::unordered_map<std::string, TagKey> containerNameTag = {
    {"_image_name_", TagKey::CONTAINER_IMAGE_NAME_TAG_KEY},
    {"_container_name_", TagKey::CONTAINER_NAME_TAG_KEY},
    {"_pod_name_", TagKey::K8S_POD_NAME_TAG_KEY},
    {"_namespace_", TagKey::K8S_NAMESPACE_TAG_KEY},
    {"_pod_uid_", TagKey::K8S_POD_UID_TAG_KEY},
    {"_container_ip_", TagKey::CONTAINER_IP_TAG_KEY},
};


void RawContainerInfo::AddMetadata(const std::string& key, const std::string& value) {
    auto it = containerNameTag.find(key);
    if (it != containerNameTag.end()) {
        mMetadatas.emplace_back(it->second, value);
    } else {
        mCustomMetadatas.emplace_back(key, value);
    }
}

} // namespace logtail
