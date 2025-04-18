/*
 * Copyright 2024 iLogtail Authors
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

#pragma once
#include <string>

namespace logtail {

enum class TagKey : int {
    FILE_OFFSET_KEY,
    FILE_INODE_TAG_KEY,
    FILE_PATH_TAG_KEY,
    K8S_NAMESPACE_TAG_KEY,
    K8S_POD_NAME_TAG_KEY,
    K8S_POD_UID_TAG_KEY,
    CONTAINER_NAME_TAG_KEY,
    CONTAINER_IP_TAG_KEY,
    CONTAINER_IMAGE_NAME_TAG_KEY,
    HOST_NAME_TAG_KEY,
    HOST_ID_TAG_KEY,
    CLOUD_PROVIDER_TAG_KEY,
#ifndef __ENTERPRISE__
    HOST_IP_TAG_KEY,
#else
    AGENT_TAG_TAG_KEY,
#endif
};

const std::string& GetDefaultTagKeyString(TagKey key);

////////////////////////// COMMON ////////////////////////
extern const std::string DEFAULT_CONFIG_TAG_KEY_VALUE;

////////////////////////// LOG ////////////////////////
extern const std::string DEFAULT_LOG_TAG_HOST_NAME;
extern const std::string DEFAULT_LOG_TAG_NAMESPACE;
extern const std::string DEFAULT_LOG_TAG_POD_NAME;
extern const std::string DEFAULT_LOG_TAG_POD_UID;
extern const std::string DEFAULT_LOG_TAG_CONTAINER_NAME;
extern const std::string DEFAULT_LOG_TAG_CONTAINER_IP;
extern const std::string DEFAULT_LOG_TAG_IMAGE_NAME;
extern const std::string DEFAULT_LOG_TAG_FILE_OFFSET;
extern const std::string DEFAULT_LOG_TAG_FILE_INODE;
extern const std::string DEFAULT_LOG_TAG_FILE_PATH;
extern const std::string DEFAULT_LOG_TAG_HOST_ID;
extern const std::string DEFAULT_LOG_TAG_CLOUD_PROVIDER;
#ifndef __ENTERPRISE__
extern const std::string DEFAULT_LOG_TAG_HOST_IP;
#else
extern const std::string DEFAULT_LOG_TAG_USER_DEFINED_ID;
#endif

extern const std::string LOG_RESERVED_KEY_SOURCE;
extern const std::string LOG_RESERVED_KEY_TOPIC;
extern const std::string LOG_RESERVED_KEY_MACHINE_UUID;
extern const std::string LOG_RESERVED_KEY_PACKAGE_ID;

} // namespace logtail
