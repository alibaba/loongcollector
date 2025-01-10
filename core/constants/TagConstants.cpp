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

#include "TagConstants.h"

namespace logtail {

const std::string& TagKeyToString(TagKey key) {
    static const std::vector<std::string> TagKeyDefaultValue = {
        DEFAULT_LOG_TAG_FILE_OFFSET,
        DEFAULT_LOG_TAG_FILE_INODE,
        DEFAULT_LOG_TAG_FILE_PATH,
        DEFAULT_LOG_TAG_NAMESPACE,
        DEFAULT_LOG_TAG_POD_NAME,
        DEFAULT_LOG_TAG_POD_UID,
        DEFAULT_LOG_TAG_CONTAINER_NAME,
        DEFAULT_LOG_TAG_CONTAINER_IP,
        DEFAULT_LOG_TAG_IMAGE_NAME,
        DEFAULT_LOG_TAG_HOST_NAME,
        DEFAULT_LOG_TAG_HOST_ID,
        DEFAULT_LOG_TAG_CLOUD_PROVIDER,
#ifndef __ENTERPRISE__
        DEFAULT_LOG_TAG_HOST_IP,
#else
        DEFAULT_LOG_TAG_USER_DEFINED_ID,
#endif
    };
    return TagKeyDefaultValue[key];
}

////////////////////////// COMMON ////////////////////////
const std::string DEFAULT_TAG_NAMESPACE = "namespace";
const std::string DEFAULT_TAG_HOST_NAME = "host_name";
const std::string DEFAULT_TAG_HOST_IP = "host_ip";
const std::string DEFAULT_TAG_POD_NAME = "pod_name";
const std::string DEFAULT_TAG_POD_UID = "pod_uid";
const std::string DEFAULT_TAG_CONTAINER_NAME = "container_name";
const std::string DEFAULT_TAG_CONTAINER_IP = "container_ip";
const std::string DEFAULT_TAG_IMAGE_NAME = "image_name";

const std::string DEFAULT_CONFIG_TAG_KEY_VALUE = "__default__";

////////////////////////// LOG ////////////////////////
#ifndef __ENTERPRISE__ // 开源版
const std::string DEFAULT_LOG_TAG_HOST_NAME = DEFAULT_TAG_HOST_NAME;
const std::string DEFAULT_LOG_TAG_NAMESPACE = DEFAULT_TAG_NAMESPACE;
const std::string DEFAULT_LOG_TAG_POD_NAME = DEFAULT_TAG_POD_NAME;
const std::string DEFAULT_LOG_TAG_POD_UID = DEFAULT_TAG_POD_UID;
const std::string DEFAULT_LOG_TAG_CONTAINER_NAME = DEFAULT_TAG_CONTAINER_NAME;
const std::string DEFAULT_LOG_TAG_CONTAINER_IP = DEFAULT_TAG_CONTAINER_IP;
const std::string DEFAULT_LOG_TAG_IMAGE_NAME = DEFAULT_TAG_IMAGE_NAME;
const std::string DEFAULT_LOG_TAG_FILE_OFFSET = "file_offset";
const std::string DEFAULT_LOG_TAG_FILE_INODE = "file_inode";
const std::string DEFAULT_LOG_TAG_FILE_PATH = "file_path";

const std::string DEFAULT_LOG_TAG_HOST_IP = DEFAULT_TAG_HOST_IP;
const std::string DEFAULT_LOG_TAG_HOST_ID = "host_id";
const std::string DEFAULT_LOG_TAG_CLOUD_PROVIDER = "cloud_provider";
#else
const std::string DEFAULT_LOG_TAG_HOST_NAME = "__hostname__";
const std::string DEFAULT_LOG_TAG_NAMESPACE = "_namespace_";
const std::string DEFAULT_LOG_TAG_POD_NAME = "_pod_name_";
const std::string DEFAULT_LOG_TAG_POD_UID = "_pod_uid_";
const std::string DEFAULT_LOG_TAG_CONTAINER_NAME = "_container_name_";
const std::string DEFAULT_LOG_TAG_CONTAINER_IP = "_container_ip_";
const std::string DEFAULT_LOG_TAG_IMAGE_NAME = "_image_name_";
const std::string DEFAULT_LOG_TAG_FILE_OFFSET = "__file_offset__";
const std::string DEFAULT_LOG_TAG_FILE_INODE = "__inode__";
const std::string DEFAULT_LOG_TAG_FILE_PATH = "__path__";
const std::string DEFAULT_LOG_TAG_HOST_ID = "__host_id__";
const std::string DEFAULT_LOG_TAG_CLOUD_PROVIDER = "__cloud_provider__";

const std::string DEFAULT_LOG_TAG_USER_DEFINED_ID = "__user_defined_id__";
#endif

const std::string LOG_RESERVED_KEY_SOURCE = "__source__";
const std::string LOG_RESERVED_KEY_TOPIC = "__topic__";
const std::string LOG_RESERVED_KEY_MACHINE_UUID = "__machine_uuid__";
const std::string LOG_RESERVED_KEY_PACKAGE_ID = "__pack_id__";

////////////////////////// METRIC ////////////////////////
const std::string DEFAULT_METRIC_TAG_NAMESPACE = DEFAULT_TAG_NAMESPACE;
const std::string DEFAULT_METRIC_TAG_POD_NAME = DEFAULT_TAG_POD_NAME;
const std::string DEFAULT_METRIC_TAG_POD_UID = DEFAULT_TAG_POD_UID;
const std::string DEFAULT_METRIC_TAG_CONTAINER_NAME = DEFAULT_TAG_CONTAINER_NAME;
const std::string DEFAULT_METRIC_TAG_CONTAINER_IP = DEFAULT_TAG_CONTAINER_IP;
const std::string DEFAULT_METRIC_TAG_IMAGE_NAME = DEFAULT_TAG_IMAGE_NAME;

////////////////////////// TRACE ////////////////////////


} // namespace logtail
