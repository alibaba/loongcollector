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

#include "metadata/ContainerMetadata.h"

#include "go_pipeline/LogtailPlugin.h"

namespace logtail {

ContainerMetadata::ContainerMetadata(size_t cidCacheSize) : mContainerCache(cidCacheSize, 20) {
}

bool ContainerMetadata::Enable() {
    return LogtailPlugin::GetInstance()->IsPluginOpened();
}
std::shared_ptr<ContainerMeta> ContainerMetadata::GetInfoByContainerId(const StringView& containerId) {
    if (!Enable()) {
        return nullptr;
    }
    std::shared_ptr<ContainerMeta> containerInfo;
    bool isValid = mContainerCache.tryGetCopy(containerId, containerInfo);
    if (isValid) {
        return containerInfo;
    }
    K8sContainerMeta meta = LogtailPlugin::GetInstance()->GetContainerMeta(containerId);
    containerInfo = std::make_shared<ContainerMeta>();
    containerInfo->mContainerId = containerId.to_string();
    containerInfo->mContainerName = meta.ContainerName;
    containerInfo->mImageName = meta.Image;
    mContainerCache.insert(containerInfo->mContainerId, containerInfo);
    return containerInfo;
}

} // namespace logtail
