// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "plugin/flusher/sls/SLSConstant.h"

using namespace std;

namespace logtail {

const string LOGSTORES = "/logstores";
const string METRICSTORES = "/prometheus";
const string HEALTH = "/health";

const string LOGTAIL_USER_AGENT = "ali-log-logtail";

const string CONTENT_MD5 = "Content-MD5";

const string LOG_HEADER_PREFIX = "x-log-";
const string LOG_OLD_HEADER_PREFIX = "x-sls-";
const string ACS_HEADER_PREFIX = "x-acs-";
const string X_LOG_KEYPROVIDER = "x-log-keyprovider";
const string X_LOG_APIVERSION = "x-log-apiversion";
const string X_LOG_COMPRESSTYPE = "x-log-compresstype";
const string X_LOG_BODYRAWSIZE = "x-log-bodyrawsize";
const string X_LOG_SIGNATUREMETHOD = "x-log-signaturemethod";
const string X_LOG_MODE = "x-log-mode";
const string X_LOG_HOSTIP = "x-log-hostip";
const string X_LOG_REQUEST_ID = "x-log-requestid";
const string HMAC_SHA1 = "hmac-sha1";
const string LOG_HEADSIGNATURE_PREFIX = "LOG ";
const string LOG_API_VERSION = "0.6.0";
const string LOG_MODE_BATCH_GROUP = "batch_group";

const string MD5_SHA1_SALT_KEYPROVIDER = "md5-sha1-salt";

const string LOGE_REQUEST_ERROR = "RequestError";
const string LOGE_INVALID_HOST = "InvalidHost";
const string LOGE_UNKNOWN_ERROR = "UnknownError";
const string LOGE_NOT_IMPLEMENTED = "NotImplemented";
const string LOGE_SERVER_BUSY = "ServerBusy";
const string LOGE_INTERNAL_SERVER_ERROR = "InternalServerError";
const string LOGE_RESPONSE_SIG_ERROR = "ResponseSignatureError";
const string LOGE_PARAMETER_INVALID = "ParameterInvalid";
const string LOGE_MISSING_PARAMETER = "MissingParameter";
const string LOGE_INVALID_METHOD = "InvalidMethod";
const string LOGE_BAD_RESPONSE = "BadResponse";
const string LOGE_UNAUTHORIZED = "Unauthorized";
const string LOGE_QUOTA_EXCEED = "ExceedQuota";
const string LOGE_REQUEST_TIMEOUT = "RequestTimeout";
const string LOGE_CLIENT_OPERATION_TIMEOUT = "ClientOpertaionTimeout";
const string LOGE_CLIENT_NETWORK_ERROR = "ClientNetworkError";
const string LOGE_USER_NOT_EXIST = "UserNotExist";
const string LOGE_CATEGORY_NOT_EXIST = "CategoryNotExist";
const string LOGE_TOPIC_NOT_EXIST = "TopicNotExist";
const string LOGE_POST_BODY_INVALID = "PostBodyInvalid";
const string LOGE_INVALID_CONTENTTYPE = "InvalidContentType";
const string LOGE_INVALID_CONTENLENGTH = "InvalidContentLength";
const string LOGE_INVALID_APIVERSION = "InvalidAPIVersion";
const string LOGE_PROJECT_NOT_EXIST = "ProjectNotExist";
const string LOGE_MACHINEGROUP_NOT_EXIST = "MachineGroupNotExist";
const string LOGE_MACHINEGROUP_ALREADY_EXIST = "MachineGroupAlreadyExist";
const string LOGE_CONFIG_NOT_EXIST = "ConfigNotExist";
const string LOGE_CONFIG_ALREADY_EXIST = "ConfigAlreadyExist";
const string LOGE_LOGSTORE_NOT_EXIST = "LogStoreNotExist";
const string LOGE_INVALID_ACCESSKEYID = "InvalidAccessKeyId";
const string LOGE_SIGNATURE_NOT_MATCH = "SignatureNotMatch";
const string LOGE_PROJECT_FORBIDDEN = "ProjectForbidden";
const string LOGE_WRITE_QUOTA_EXCEED = "WriteQuotaExceed";
const string LOGE_READ_QUOTA_EXCEED = "ReadQuotaExceed";
const string LOGE_REQUEST_TIME_EXPIRED = "RequestTimeExpired";
const string LOGE_INVALID_REQUEST_TIME = "InvalidRequestTime";
const string LOGE_POST_BODY_TOO_LARGE = "PostBodyTooLarge";
const string LOGE_INVALID_TIME_RANGE = "InvalidTimeRange";
const string LOGE_INVALID_REVERSE = "InvalidReverse";
const string LOGE_LOGSTORE_WITHOUT_SHARD = "LogStoreWithoutShard";
const string LOGE_SHARD_WRITE_QUOTA_EXCEED = "ShardWriteQuotaExceed";
const string LOGE_SHARD_READ_QUOTA_EXCEED = "ShardReadQuotaExceed";
const string LOGE_INVALID_SEQUENCE_ID = "InvalidSequenceId";
const string LOGE_NOT_SUPPORTED_ACCEPT_CONTENT_TYPE = "InvalidAcceptContentType";
const string LOGE_NOT_SUPPORTED_ACCEPT_ENCODING = "InvalidAcceptEncoding";
const string LOGE_SHARD_NOT_EXIST = "ShardNotExist";
const string LOGE_INVALID_CURSOR = "InvalidCursor";

const string LOG_ERROR_CODE = "errorCode";
const string LOG_ERROR_MESSAGE = "errorMessage";

} // namespace logtail
