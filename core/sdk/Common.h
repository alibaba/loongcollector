/*
 * Copyright 2022 iLogtail Authors
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
#include <map>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <time.h>
#include <atomic>
#include <mutex>
#include <cstring>
#include <cassert>

namespace logtail {
namespace sdk {

    const int64_t kInvalidHashKeySeqID = 0;
    const int64_t kFirstHashKeySeqID = 1;

    const uint32_t LOG_REQUEST_TIMEOUT = 20;
    const uint32_t MD5_BYTES = 16;
#define DATE_FORMAT_RFC822 "%a, %d %b %Y %H:%M:%S GMT" ///<RFC822 date formate, GMT time.

    extern const char* const LOG_HEADSIGNATURE_PREFIX; ///< "";

    extern const char* const LOGE_REQUEST_ERROR; //= "RequestError";
    extern const char* const LOGE_INVALID_HOST; //= "InvalidHost"
    extern const char* const LOGE_UNKNOWN_ERROR; //= "UnknownError";
    extern const char* const LOGE_NOT_IMPLEMENTED; //= "NotImplemented";
    extern const char* const LOGE_SERVER_BUSY; //= "ServerBusy";
    extern const char* const LOGE_INTERNAL_SERVER_ERROR; //= "InternalServerError";
    extern const char* const LOGE_RESPONSE_SIG_ERROR; //= "ResponseSignatureError";
    extern const char* const LOGE_PARAMETER_INVALID; //= "ParameterInvalid";
    extern const char* const LOGE_MISSING_PARAMETER; //= "MissingParameter";
    extern const char* const LOGE_INVALID_METHOD; //= "InvalidMethod";
    extern const char* const LOGE_INVALID_CONTENTTYPE; //= "InvalidContentType";
    extern const char* const LOGE_INVALID_CONTENTLENGTH; //= "InvalidContentLength";
    extern const char* const LOGE_BAD_RESPONSE; //= "BadResponse";
    extern const char* const LOGE_UNAUTHORIZED; //= "Unauthorized";
    extern const char* const LOGE_QUOTA_EXCEED; //= "ExceedQuota";
    extern const char* const LOGE_REQUEST_TIMEOUT; //= "RequestTimeout";
    extern const char* const LOGE_CLIENT_OPERATION_TIMEOUT; //= "ClientOpertaionTimeout";
    extern const char* const LOGE_CLIENT_NETWORK_ERROR; //= "ClientNetworkError";
    extern const char* const LOGE_USER_NOT_EXIST; //= "UserNotExist";
    extern const char* const LOGE_CATEGORY_NOT_EXIST; //= "CategoryNotExist";
    extern const char* const LOGE_TOPIC_NOT_EXIST; //= "TopicNotExist";
    extern const char* const LOGE_POST_BODY_INVALID; //= "PostBodyInvalid";
    extern const char* const LOGE_INVALID_HOST; //= "InvalidHost";
    extern const char* const LOGE_INVALID_APIVERSION; //="InvalidAPIVersion";
    extern const char* const LOGE_PROJECT_NOT_EXIST; //="ProjectNotExist";
    extern const char* const LOGE_MACHINEGROUP_NOT_EXIST; //="MachineGroupNotExist";
    extern const char* const LOGE_MACHINEGROUP_ALREADY_EXIST; //="MachineGroupAlreadyExist";
    extern const char* const LOGE_CONFIG_NOT_EXIST; //="ConfigNotExist";
    extern const char* const LOGE_CONFIG_ALREADY_EXIST; //="ConfigAlreadyExist";
    extern const char* const LOGE_LOGSTORE_NOT_EXIST; //="LogStoreNotExist";
    extern const char* const LOGE_INVALID_ACCESSKEYID; //="InvalidAccessKeyId";
    extern const char* const LOGE_SIGNATURE_NOT_MATCH; //="SignatureNotMatch";
    extern const char* const LOGE_PROJECT_FORBIDDEN; //="ProjectForbidden";
    extern const char* const LOGE_WRITE_QUOTA_EXCEED; //="WriteQuotaExceed";
    extern const char* const LOGE_READ_QUOTA_EXCEED; //="ReadQuotaExceed";
    extern const char* const LOGE_REQUEST_TIME_EXPIRED; //="RequestTimeExpired";
    extern const char* const LOGE_INVALID_REQUEST_TIME; //="InvalidRequestTime";
    extern const char* const LOGE_POST_BODY_TOO_LARGE; //="PostBodyTooLarge";
    extern const char* const LOGE_INVALID_TIME_RANGE; //="InvalidTimeRange";
    extern const char* const LOGE_INVALID_REVERSE; //="InvalidReverse";
    extern const char* const LOGE_LOGSTORE_WITHOUT_SHARD; //="LogStoreWithoutShard";
    extern const char* const LOGE_INVALID_SEQUENCE_ID; //="InvalidSequenceId";

    extern const char* const LOGSTORES; //= "/logstores"
    extern const char* const SHARDS; //= "/shards"
    extern const char* const INDEX; //= "/index"
    extern const char* const CONFIGS; //= "/configs"
    extern const char* const MACHINES; //= "/machines"
    extern const char* const MACHINEGROUPS; //= "/machinegroups"
    extern const char* const ACLS; //= "/acls"

    extern const char* const HTTP_GET; //= "GET";
    extern const char* const HTTP_POST; //= "POST";
    extern const char* const HTTP_PUT; //= "PUT";
    extern const char* const HTTP_DELETE; //= "DELETE";

    extern const char* const HOST; //= "Host";
    extern const char* const DATE; //= "Date";
    extern const char* const USER_AGENT; //= "User-Agent";
    extern const char* const LOG_HEADER_PREFIX; //= "x-log-";
    extern const char* const LOG_OLD_HEADER_PREFIX; //= "x-sls-";
    extern const char* const X_LOG_KEYPROVIDER; // = "x-log-keyprovider";
    extern const char* const X_LOG_APIVERSION; // = "x-log-apiversion";
    extern const char* const X_LOG_COMPRESSTYPE; // = "x-log-compresstype";
    extern const char* const X_LOG_BODYRAWSIZE; // = "x-log-bodyrawsize";
    extern const char* const X_LOG_SIGNATUREMETHOD; // = "x-log-signaturemethod";
    extern const char* const X_ACS_SECURITY_TOKEN; // = "x-acs-security-token";
    extern const char* const X_LOG_CURSOR; // = "cursor";
    extern const char* const X_LOG_REQUEST_ID; // = "x-log-requestid";
    extern const char* const X_LOG_MODE; // = "x-log-mode";

    extern const char* const X_LOG_PROGRESS; // = "x-log-progress";
    extern const char* const X_LOG_COUNT; // = "x-log-count";
    extern const char* const X_LOG_HOSTIP; // = "x-log-hostip"

    extern const char* const HTTP_ACCEPT; // = "accept";
    extern const char* const DEFLATE; //= "deflate";
    extern const char* const HMAC_SHA1; //= "hmac-sha1";
    extern const char* const CONTENT_LENGTH; //= "Content-Length";
    extern const char* const CONTENT_TYPE; //= "Content-Type";
    extern const char* const CONTENT_MD5; //= "Content-MD5";
    extern const char* const AUTHORIZATION; //= "Authorization";
    extern const char* const SIGNATURE; //= "Signature";
    extern const char* const ACCEPT_ENCODING; // = "Accept-Encoding";
    extern const char* const ENCONDING_GZIP; // = "gzip";
    extern const char* const TYPE_LOG_PROTOBUF; //="application/x-protobuf";
    extern const char* const TYPE_LOG_JSON; //="application/json";
    extern const char* const LOG_MODE_BATCH_GROUP; //="batch_group";
    extern const char* const LOGITEM_TIME_STAMP_LABEL; //="__time__";
    extern const char* const LOGITEM_SOURCE_LABEL; //="__source__";
    extern const char* const LOG_API_VERSION; // = "0.6.0";
    extern const char* const LOGTAIL_USER_AGENT; // = "ali-sls-logtail";
    extern const char* const MD5_SHA1_SALT_KEYPROVIDER; // = "md5-shal-salt";
    extern const char* const LOG_TYPE_CURSOR; // = "cursor";
    extern const char* const LOG_TYPE; // = "type";
    extern const char* const LOGE_NOT_SUPPORTED_ACCEPT_CONTENT_TYPE;
    extern const char* const LOGE_NOT_SUPPORTED_ACCEPT_ENCODING;
    extern const char* const LOGE_SHARD_NOT_EXIST;
    extern const char* const LOGE_INVALID_CURSOR;
    extern const char* const LOGE_SHARD_WRITE_QUOTA_EXCEED;
    extern const char* const LOGE_SHARD_READ_QUOTA_EXCEED;
    extern const char* const LOG_LZ4; //= "lz4";

    extern const char* const LOG_ERROR_CODE; //= "errorCode";
    extern const char* const LOG_ERROR_MESSAGE; //= "errorMessage";

    extern const char* const LOG_SHARD_STATUS_READWRITE; // "readwrite";
    extern const char* const LOG_SHARD_STATUS_READONLY; // "readonly";

    /**
 * HTTP message structure includes three parts: http status code, http header, and http content.
 */
    struct HttpMessage {
        int32_t statusCode = 0; ///<Http status code
        std::map<std::string, std::string> header; ///<Only contains the header lines which have key:value pair
        std::string content; ///<Http content
        /** Constructor with no parameter.
    * @param void None.
    * @return The objcect pointer.
    */
        HttpMessage() {}
        /** Constructor with header and content.
    * @param para_header A map structure which contains the key:value pair of http header lines.
    Those header lines which do not contains key:value pair are not included.
    * @param para_content A string which contains the content of http request.
    * @return The objcect pointer.
    */
        HttpMessage(const std::map<std::string, std::string>& para_header, const std::string& para_content)
            : header(para_header), content(para_content) {}
        /** Constructor with status code, header and content.
    * @param para_statusCode Http status code.
    * @param para_header A map structure which contains the key:value pair of http header lines.
    Those header lines which do not contains key:value pair are not included.
    * @param para_content A string which contains the http content of http content.
    * @return The objcect pointer.
    */
        HttpMessage(const int32_t para_statusCode,
                    const std::map<std::string, std::string>& para_header,
                    const std::string& para_content)
            : statusCode(para_statusCode), header(para_header), content(para_content) {}

        bool IsLogServiceResponse() const;

        void FillResponse(const int32_t para_statusCode, const std::string& para_content) {
            statusCode = para_statusCode;
            content = para_content;
        }

        std::string RequestID() const {
            const auto iter = header.find(X_LOG_REQUEST_ID);
            return iter != header.end() ? iter->second : "";
        }

        time_t GetServerTimeFromHeader() const;
    };

    /*
 * Responses
 */
    struct Response {
        int32_t statusCode = 0; ///<Http status code
        std::string requestId;

        virtual ~Response() {}

        virtual void ParseSuccess(const HttpMessage& message);

        void SetError(int32_t code, const std::string& request) {
            statusCode = code;
            requestId = request;
        }
    };

    struct PostLogStoreLogsResponse : public Response {
        int32_t bodyBytes;
    };

    struct GetRealIpResponse : public Response {
        std::string realIp;
    };

#define SHA1_INPUT_WORDS 16
#define SHA1_DIGEST_WORDS 5
#define SHA1_INPUT_BYTES (SHA1_INPUT_WORDS * sizeof(uint32_t))
#define SHA1_DIGEST_BYTES (SHA1_DIGEST_WORDS * sizeof(uint32_t))

#define BIT_COUNT_WORDS 2
#define BIT_COUNT_BYTES (BIT_COUNT_WORDS * sizeof(uint32_t))

    /** Calculate Md5 for a byte stream,
* result is stored in md5[16]
*
* @param poolIn Input data
* @param inputBytesNum Length of input data
* @param md5[16] A 128-bit pool for storing md5
*/
    void DoMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t md5[16]);
    void Base64Encoding(std::istream&,
                        std::ostream&,
                        char makeupChar = '=',
                        const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    std::string CalcMD5(const std::string& message);
    std::string CalcSHA1(const std::string& message, const std::string& key);
    std::string Base64Enconde(const std::string& message);

    std::string UrlEncode(const std::string& str);
    std::string UrlDecode(const std::string& str);

    std::string GetDateString(const std::string& dateFormat);
    std::string GetDateString();
    time_t DecodeDateString(const std::string dateString, const std::string& dateFormat = DATE_FORMAT_RFC822);

    bool StartWith(const std::string& input, const std::string& pattern);

    void GetQueryString(const std::map<std::string, std::string>& parameterList, std::string& queryString);

    std::string GetUrlSignature(const std::string& httpMethod,
                                const std::string& operationType,
                                std::map<std::string, std::string>& httpHeader,
                                const std::map<std::string, std::string>& parameterList,
                                const std::string& content,
                                const std::string& signKey);

    class SHA1 {
    public:
        SHA1() : bits(0) { memcpy(H, IV, sizeof(H)); }
        SHA1(const SHA1& s) {
            bits = s.bits;
            memcpy(H, s.H, sizeof(H));
            memcpy(M, s.M, sizeof(M));
        }
        void init() {
            bits = 0;
            memcpy(H, IV, sizeof(H));
        }
        void add(const uint8_t* data, size_t len);
        uint8_t* result();

    private:
        uint64_t bits;
        uint32_t H[SHA1_DIGEST_WORDS];
        uint8_t M[SHA1_INPUT_BYTES];

        static const uint32_t IV[SHA1_DIGEST_WORDS];
        void transform();
    };

    class HMAC {
    public:
        HMAC(const uint8_t* key, size_t lkey);
        HMAC(const HMAC& hm) : in(hm.in), out(hm.out) {}

        void init(const uint8_t* key, size_t lkey);

        void add(const uint8_t* data, size_t len) { in.add(data, len); }

        uint8_t* result() {
            out.add(in.result(), SHA1_DIGEST_BYTES);
            return out.result();
        }

    private:
        SHA1 in, out;
    };

    class SpinLock {
        std::atomic_flag locked = ATOMIC_FLAG_INIT;

        SpinLock(const SpinLock&) = delete;
        SpinLock& operator=(const SpinLock&) = delete;

    public:
        SpinLock() {}

        void lock() {
            while (locked.test_and_set(std::memory_order_acquire)) {
                ;
            }
        }

        void unlock() { locked.clear(std::memory_order_release); }
    };

    using ScopedSpinLock = std::lock_guard<SpinLock>;

    class LogsClosure;

    struct AsynRequest {
        AsynRequest(const std::string& httpMethod,
                    const std::string& host,
                    const int32_t port,
                    const std::string& url,
                    const std::string& queryString,
                    const std::map<std::string, std::string>& header,
                    const std::string& body,
                    const int32_t timeout,
                    const std::string& intf,
                    const bool httpsFlag,
                    LogsClosure* callBack,
                    Response* response)
            : mHTTPMethod(httpMethod),
              mHost(host),
              mPort(port),
              mUrl(url),
              mQueryString(queryString),
              mHeader(header),
              mBody(body),
              mTimeout(timeout),
              mInterface(intf),
              mHTTPSFlag(httpsFlag),
              mCallBack(callBack),
              mPrivateData(NULL),
              mResponse(response)

        {}

        ~AsynRequest() { delete mResponse; }

        std::string mHTTPMethod;
        std::string mHost;
        int32_t mPort = 80;
        std::string mUrl;
        std::string mQueryString;
        std::map<std::string, std::string> mHeader;
        std::string mBody;
        int32_t mTimeout = 15;
        std::string mInterface;
        bool mHTTPSFlag = false;
        LogsClosure* mCallBack = NULL;
        void* mPrivateData;
        Response* mResponse;
    };

    class HTTPClient {
    public:
        virtual ~HTTPClient() {}
        virtual void Send(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          HttpMessage& httpMessage,
                          const std::string& intf,
                          const bool httpsFlag)
            = 0;
        virtual void AsynSend(AsynRequest* request) = 0;
    };

} // namespace sdk
} // namespace logtail
