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

#pragma once

#include <string>

namespace logtail {

extern const std::string LOGSTORES;
extern const std::string METRICSTORES;
extern const std::string HEALTH;

extern const std::string CONTENT_MD5;

extern const std::string LOGTAIL_USER_AGENT;

extern const std::string LOG_HEADER_PREFIX;
extern const std::string LOG_OLD_HEADER_PREFIX;
extern const std::string ACS_HEADER_PREFIX;
extern const std::string X_LOG_KEYPROVIDER;
extern const std::string X_LOG_APIVERSION;
extern const std::string X_LOG_COMPRESSTYPE;
extern const std::string X_LOG_BODYRAWSIZE;
extern const std::string X_LOG_SIGNATUREMETHOD;
extern const std::string X_LOG_MODE;
extern const std::string X_LOG_HOSTIP;
extern const std::string X_LOG_REQUEST_ID;

extern const std::string LOG_HEADSIGNATURE_PREFIX;
extern const std::string LOG_API_VERSION;
extern const std::string LOG_MODE_BATCH_GROUP;
extern const std::string HMAC_SHA1;
extern const std::string MD5_SHA1_SALT_KEYPROVIDER;

extern const std::string LOGE_REQUEST_ERROR;
extern const std::string LOGE_INVALID_HOST;
extern const std::string LOGE_UNKNOWN_ERROR;
extern const std::string LOGE_NOT_IMPLEMENTED;
extern const std::string LOGE_SERVER_BUSY;
extern const std::string LOGE_INTERNAL_SERVER_ERROR;
extern const std::string LOGE_RESPONSE_SIG_ERROR;
extern const std::string LOGE_PARAMETER_INVALID;
extern const std::string LOGE_MISSING_PARAMETER;
extern const std::string LOGE_INVALID_METHOD;
extern const std::string LOGE_INVALID_CONTENTTYPE;
extern const std::string LOGE_INVALID_CONTENTLENGTH;
extern const std::string LOGE_BAD_RESPONSE;
extern const std::string LOGE_UNAUTHORIZED;
extern const std::string LOGE_QUOTA_EXCEED;
extern const std::string LOGE_REQUEST_TIMEOUT;
extern const std::string LOGE_CLIENT_OPERATION_TIMEOUT;
extern const std::string LOGE_CLIENT_NETWORK_ERROR;
extern const std::string LOGE_USER_NOT_EXIST;
extern const std::string LOGE_CATEGORY_NOT_EXIST;
extern const std::string LOGE_TOPIC_NOT_EXIST;
extern const std::string LOGE_POST_BODY_INVALID;
extern const std::string LOGE_INVALID_HOST;
extern const std::string LOGE_INVALID_APIVERSION;
extern const std::string LOGE_PROJECT_NOT_EXIST;
extern const std::string LOGE_MACHINEGROUP_NOT_EXIST;
extern const std::string LOGE_MACHINEGROUP_ALREADY_EXIST;
extern const std::string LOGE_CONFIG_NOT_EXIST;
extern const std::string LOGE_CONFIG_ALREADY_EXIST;
extern const std::string LOGE_LOGSTORE_NOT_EXIST;
extern const std::string LOGE_INVALID_ACCESSKEYID;
extern const std::string LOGE_SIGNATURE_NOT_MATCH;
extern const std::string LOGE_PROJECT_FORBIDDEN;
extern const std::string LOGE_WRITE_QUOTA_EXCEED;
extern const std::string LOGE_READ_QUOTA_EXCEED;
extern const std::string LOGE_REQUEST_TIME_EXPIRED;
extern const std::string LOGE_INVALID_REQUEST_TIME;
extern const std::string LOGE_POST_BODY_TOO_LARGE;
extern const std::string LOGE_INVALID_TIME_RANGE;
extern const std::string LOGE_INVALID_REVERSE;
extern const std::string LOGE_LOGSTORE_WITHOUT_SHARD;
extern const std::string LOGE_INVALID_SEQUENCE_ID;
extern const std::string LOGE_NOT_SUPPORTED_ACCEPT_CONTENT_TYPE;
extern const std::string LOGE_NOT_SUPPORTED_ACCEPT_ENCODING;
extern const std::string LOGE_SHARD_NOT_EXIST;
extern const std::string LOGE_INVALID_CURSOR;
extern const std::string LOGE_SHARD_WRITE_QUOTA_EXCEED;
extern const std::string LOGE_SHARD_READ_QUOTA_EXCEED;

extern const std::string LOG_ERROR_CODE;
extern const std::string LOG_ERROR_MESSAGE;

} // namespace logtail
