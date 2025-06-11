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

#include "common/Flags.h"

DEFINE_FLAG_BOOL(disable_container_meta, "disable container metadata", true);

namespace logtail {

ContainerMetadata::ContainerMetadata(size_t cidCacheSize) : mContainerCache(cidCacheSize, 20) {
}

bool ContainerMetadata::Enable() {
    return !BOOL_FLAG(disable_container_meta);
}

std::shared_ptr<ContainerMeta> ContainerMetadata::GetInfoByContainerId(const StringView&) {
    if (!Enable()) {
        return nullptr;
    }
    return nullptr;
}

} // namespace logtail
