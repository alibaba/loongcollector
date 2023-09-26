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

#include "Constants.h"

namespace logtail {

#if defined(__linux__)
const std::string OS_NAME = "Linux";
#elif defined(_MSC_VER)
const std::string OS_NAME = "Windows";
#endif

const std::string LOG_RESERVED_KEY_SOURCE = "__source__";
const std::string LOG_RESERVED_KEY_TOPIC = "__topic__";
const std::string LOG_RESERVED_KEY_USER_DEFINED_ID = "__user_defined_id__";
const std::string LOG_RESERVED_KEY_MACHINE_UUID = "__machine_uuid__";
const std::string LOG_RESERVED_KEY_HOSTNAME = "__hostname__";
const std::string LOG_RESERVED_KEY_PATH = "__path__";
const std::string LOG_RESERVED_KEY_PACKAGE_ID = "__pack_id__";
const std::string LOG_RESERVED_KEY_TRUNCATE_INFO = "__truncate_info__";
const std::string LOG_RESERVED_KEY_ALIPAY_ZONE = "__alipay_zone__";
const std::string LOG_RESERVED_KEY_INODE = "__inode__";
const std::string LOG_RESERVED_KEY_FILE_OFFSET = "__file_offset__";

const char* SLS_EMPTY_STR_FOR_INDEX = "\01";

// profile project
const std::string PROFILE_PROJECT = "profile_project";
const std::string PROFILE_PROJECT_REGION = "profile_project_region";

// global config
const std::string GLOBAL_CONFIG_NODE = "config";
const std::string GLOBAL_CONFIG_VERSION = "version";

// user config
const std::string USER_CONFIG_NODE = "metrics";
const std::string USER_CONFIG_VERSION = "version";
const std::string USER_CONFIG_GROUPTOPIC = "group_topic";
const std::string USER_CONFIG_TOPICFORMAT = "topic_format";

// metric config
const std::string ROW_MAPPING = "row_mapping";
const std::string COLUMN_MAPPING = "column_mapping";
const std::string GROUP_RULE = "group_config_rule";

const std::string TABLE_NAME = "table_name";
const std::string TABLE_PREFIX = "table_prefix";
const std::string DATE_TYPE = "date_type";
const std::string DATE_SUFFIX = "date_suffix";
const std::string INTERVAL = "interval";
const std::string EXPIRE_DAY = "expire_day";

const std::string DEFAULT_CONTENT_KEY = "content";
const std::string DEFAULT_REG = "(.*)";

} // namespace logtail