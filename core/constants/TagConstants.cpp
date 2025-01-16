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

using namespace std;

namespace logtail {

const string& GetDefaultTagKeyString(TagKey key) {
    static const vector<string> TagKeyDefaultValue = {
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
const string DEFAULT_CONFIG_TAG_KEY_VALUE = "__default__";

////////////////////////// LOG ////////////////////////
#ifndef __ENTERPRISE__ // 开源版
const string DEFAULT_LOG_TAG_HOST_NAME = "namespace"; // should keep same with metric
const string DEFAULT_LOG_TAG_NAMESPACE = "host_name"; // should keep same with metric
const string DEFAULT_LOG_TAG_POD_NAME = "pod_name"; // should keep same with metric
const string DEFAULT_LOG_TAG_POD_UID = "pod_uid"; // should keep same with metric
const string DEFAULT_LOG_TAG_CONTAINER_NAME = "container_name"; // should keep same with metric
const string DEFAULT_LOG_TAG_CONTAINER_IP = "container_ip"; // should keep same with metric
const string DEFAULT_LOG_TAG_IMAGE_NAME = "image_name"; // should keep same with metric
const string DEFAULT_LOG_TAG_FILE_OFFSET = "file_offset";
const string DEFAULT_LOG_TAG_FILE_INODE = "file_inode";
const string DEFAULT_LOG_TAG_FILE_PATH = "file_path";

const string DEFAULT_LOG_TAG_HOST_IP = "host_ip";
const string DEFAULT_LOG_TAG_HOST_ID = "host_id";
const string DEFAULT_LOG_TAG_CLOUD_PROVIDER = "cloud_provider";
#else
const string DEFAULT_LOG_TAG_HOST_NAME = "__hostname__";
const string DEFAULT_LOG_TAG_NAMESPACE = "_namespace_";
const string DEFAULT_LOG_TAG_POD_NAME = "_pod_name_";
const string DEFAULT_LOG_TAG_POD_UID = "_pod_uid_";
const string DEFAULT_LOG_TAG_CONTAINER_NAME = "_container_name_";
const string DEFAULT_LOG_TAG_CONTAINER_IP = "_container_ip_";
const string DEFAULT_LOG_TAG_IMAGE_NAME = "_image_name_";
const string DEFAULT_LOG_TAG_FILE_OFFSET = "__file_offset__";
const string DEFAULT_LOG_TAG_FILE_INODE = "__inode__";
const string DEFAULT_LOG_TAG_FILE_PATH = "__path__";
const string DEFAULT_LOG_TAG_HOST_ID = "__host_id__";
const string DEFAULT_LOG_TAG_CLOUD_PROVIDER = "__cloud_provider__";

const string DEFAULT_LOG_TAG_USER_DEFINED_ID = "__user_defined_id__";
#endif

const string LOG_RESERVED_KEY_SOURCE = "__source__";
const string LOG_RESERVED_KEY_TOPIC = "__topic__";
const string LOG_RESERVED_KEY_MACHINE_UUID = "__machine_uuid__";
const string LOG_RESERVED_KEY_PACKAGE_ID = "__pack_id__";

////////////////////////// METRIC ////////////////////////
const string DEFAULT_METRIC_TAG_NAMESPACE = "namespace"; // should keep same with log
const string DEFAULT_METRIC_TAG_POD_NAME = "pod_name"; // should keep same with log
const string DEFAULT_METRIC_TAG_POD_UID = "pod_uid"; // should keep same with log
const string DEFAULT_METRIC_TAG_CONTAINER_NAME = "container_name"; // should keep same with log
const string DEFAULT_METRIC_TAG_CONTAINER_IP = "container_ip"; // should keep same with log
const string DEFAULT_METRIC_TAG_IMAGE_NAME = "image_name"; // should keep same with log

////////////////////////// TRACE ////////////////////////


} // namespace logtail
