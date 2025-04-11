// Copyright 2024 iLogtail Authors
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

#include "constants/TagConstants.h"

#include <unordered_map>

#include "ebpf/type/table/BaseElements.h"

using namespace std;

namespace logtail {

const string& GetDefaultTagKeyString(TagKey key) {
    static const unordered_map<TagKey, string> TagKeyDefaultValue = {
        {TagKey::FILE_OFFSET_KEY, DEFAULT_LOG_TAG_FILE_OFFSET},
        {TagKey::FILE_INODE_TAG_KEY, DEFAULT_LOG_TAG_FILE_INODE},
        {TagKey::FILE_PATH_TAG_KEY, DEFAULT_LOG_TAG_FILE_PATH},
        {TagKey::K8S_NAMESPACE_TAG_KEY, DEFAULT_LOG_TAG_NAMESPACE},
        {TagKey::K8S_POD_NAME_TAG_KEY, DEFAULT_LOG_TAG_POD_NAME},
        {TagKey::K8S_POD_UID_TAG_KEY, DEFAULT_LOG_TAG_POD_UID},
        {TagKey::CONTAINER_NAME_TAG_KEY, DEFAULT_LOG_TAG_CONTAINER_NAME},
        {TagKey::CONTAINER_IP_TAG_KEY, DEFAULT_LOG_TAG_CONTAINER_IP},
        {TagKey::CONTAINER_IMAGE_NAME_TAG_KEY, DEFAULT_LOG_TAG_IMAGE_NAME},
        {TagKey::HOST_NAME_TAG_KEY, DEFAULT_LOG_TAG_HOST_NAME},
        {TagKey::HOST_ID_TAG_KEY, DEFAULT_LOG_TAG_HOST_ID},
        {TagKey::CLOUD_PROVIDER_TAG_KEY, DEFAULT_LOG_TAG_CLOUD_PROVIDER},
#ifndef __ENTERPRISE__
        {TagKey::HOST_IP_TAG_KEY, DEFAULT_LOG_TAG_HOST_IP},
#else
        {TagKey::AGENT_TAG_TAG_KEY, DEFAULT_LOG_TAG_USER_DEFINED_ID},
#endif
    };
    static const string unknown = "unknown_tag_key";
    auto iter = TagKeyDefaultValue.find(key);
    if (iter != TagKeyDefaultValue.end()) {
        return iter->second;
    } else {
        return unknown;
    }
}

////////////////////////// COMMON ////////////////////////
const string DEFAULT_CONFIG_TAG_KEY_VALUE = "__default__";

////////////////////////// LOG ////////////////////////
const string DEFAULT_LOG_TAG_NAMESPACE = "_namespace_";
const string DEFAULT_LOG_TAG_POD_NAME = "_pod_name_";
const string DEFAULT_LOG_TAG_POD_UID = "_pod_uid_";
const string DEFAULT_LOG_TAG_CONTAINER_NAME = "_container_name_";
const string DEFAULT_LOG_TAG_CONTAINER_IP = "_container_ip_";
const string DEFAULT_LOG_TAG_IMAGE_NAME = "_image_name_";
const string DEFAULT_LOG_TAG_HOST_NAME = "__hostname__";
const string DEFAULT_LOG_TAG_FILE_OFFSET = "__file_offset__";
const string DEFAULT_LOG_TAG_FILE_INODE = "__inode__";
const string DEFAULT_LOG_TAG_FILE_PATH = "__path__";
const string DEFAULT_LOG_TAG_HOST_ID = "__host_id__";
const string DEFAULT_LOG_TAG_CLOUD_PROVIDER = "__cloud_provider__";
#ifndef __ENTERPRISE__
const string DEFAULT_LOG_TAG_HOST_IP = "__host_ip__";
#else
const string DEFAULT_LOG_TAG_USER_DEFINED_ID = "__user_defined_id__";
#endif

// only used in pipeline, not serialized
const string LOG_RESERVED_KEY_SOURCE = "__source__";
const string LOG_RESERVED_KEY_TOPIC = "__topic__";
const string LOG_RESERVED_KEY_MACHINE_UUID = "__machine_uuid__";
const string LOG_RESERVED_KEY_PACKAGE_ID = "__pack_id__";

} // namespace logtail
