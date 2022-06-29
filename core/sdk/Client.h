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
#include "log_pb/sls_logs.pb.h"
#include "Closure.h"
#include "Common.h"

namespace logtail {
namespace sdk {

    class Client {
    public:
        /** Constructor needs at least three parameters.
    * @param LOGHost LOG service address, for example:http://cn-hangzhou.log.aliyuncs.com.
    * @param accessKeyId Aliyun AccessKeyId.
    * @param accessKey Aliyun AccessKey Secret.
    * @param timeout Timeout time of one operation.
    * @param source Source identifier used to differentiate data from different machines. If it is empty, constructor will use machine ip as its source.
    * @param compressFlag The flag decides whether compresses the data or not when put data to LOG.
    * @return The objcect pointer.
    */
        Client(const std::string& slsHost,
               const std::string& accessKeyId,
               const std::string& accessKey,
               int32_t timeout = LOG_REQUEST_TIMEOUT,
               const std::string& source = "",
               bool compressFlag = true,
               const std::string& intf = "");
        Client(const std::string& slsHost,
               const std::string& accessKeyId,
               const std::string& accessKey,
               const std::string& securityToken,
               int32_t timeout = LOG_REQUEST_TIMEOUT,
               const std::string& source = "",
               bool compressFlag = true,
               const std::string& intf = "");
        ~Client() throw();

        void SetPort(int32_t port);

        GetRealIpResponse GetRealIp();
        bool TestNetwork();

        std::string GetHost(const std::string& project);

        void SetUserAgent(const std::string& userAgent) { mUserAgent = userAgent; }
        void SetKeyProvider(const std::string& keyProvider) { mKeyProvider = keyProvider; }

        void SetAccessKey(const std::string& accessKey);
        std::string GetAccessKey();
        void SetAccessKeyId(const std::string& accessKeyId);
        std::string GetAccessKeyId();
        void SetSlsHost(const std::string& slsHost);
        std::string GetSlsHost();
        std::string GetRawSlsHost();
        std::string GetHostFieldSuffix();
        bool GetRawSlsHostFlag();

        // @note not used
        const std::string& GetSecurityToken() { return mSecurityToken; }
        // @note not used
        void SetSecurityToken(const std::string& securityToken) { mSecurityToken = securityToken; }
        // @note not used
        void RemoveSecurityToken() { SetSecurityToken(""); }
        void SetSlsHostUpdateTime(int32_t uptime) { mSlsHostUpdateTime = uptime; }
        int32_t GetSlsHostUpdateTime() { return mSlsHostUpdateTime; }

        void SetSlsRealIpUpdateTime(int32_t uptime) { mSlsRealIpUpdateTime = uptime; }
        int32_t GetSlsRealIpUpdateTime() { return mSlsRealIpUpdateTime; }

        void SetUsingHTTPS(bool flag) { mUsingHTTPS = flag; }

        /////////////////////////////////////Internal Interface For Logtail////////////////////////////////////////
        /** Sync Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
     * @param project The project name
     * @param serialized data of logGroup, LZ4 comressed
     * @rawSize before compress
     * @return request_id.
     */
        PostLogStoreLogsResponse PostLogStoreLogs(const std::string& project,
                                                  const std::string& logstore,
                                                  const std::string& compressedLogGroup,
                                                  uint32_t rawSize,
                                                  const std::string& hashKey = "");
        /** Sync Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
     * @param project The project name
     * @param serialized data of logPackageList, consist of several LogGroup
     * @return request_id.
     */
        PostLogStoreLogsResponse PostLogStoreLogPackageList(const std::string& project,
                                                            const std::string& logstore,
                                                            const std::string& packageListData,
                                                            const std::string& hashKey = "");
        /** Async Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
     * @param project The project name
     * @param serialized data of logGroup, LZ4 comressed
     * @rawSize before compress
     * @return request_id.
     */
        void PostLogStoreLogs(const std::string& project,
                              const std::string& logstore,
                              const std::string& compressedLogGroup,
                              uint32_t rawSize,
                              PostLogStoreLogsClosure* callBack,
                              const std::string& hashKey = "",
                              int64_t hashKeySeqID = kInvalidHashKeySeqID);
        /** Async Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
     * @param project The project name
     * @param serialized data of logPackageList, consist of several LogGroup
     * @return request_id.
     */
        void PostLogStoreLogPackageList(const std::string& project,
                                        const std::string& logstore,
                                        const std::string& packageListData,
                                        PostLogStoreLogsClosure* callBack,
                                        const std::string& hashKey = "");

        PostLogStoreLogsResponse PostLogUsingWebTracking(const std::string& project,
                                                         const std::string& logstore,
                                                         const std::string& compressedLogGroup,
                                                         uint32_t rawSize);
        /////////////////////////////////////////////////////////////////////////////////////////////////

    protected:
        void SendRequest(const std::string& project,
                         const std::string& httpMethod,
                         const std::string& url,
                         const std::string& body,
                         const std::map<std::string, std::string>& parameterList,
                         std::map<std::string, std::string>& header,
                         HttpMessage& httpMessage,
                         std::string* realIpPtr = NULL);

        void AsynPostLogStoreLogs(const std::string& project,
                                  const std::string& logstore,
                                  const std::string& body,
                                  std::map<std::string, std::string>& httpHeader,
                                  PostLogStoreLogsClosure* callBack,
                                  const std::string& hashKey,
                                  int64_t hashKeySeqID);

        // PingSLSServer sends a trivial data packet to SLS for some inner purposes.
        PostLogStoreLogsResponse
        PingSLSServer(const std::string& project, const std::string& logstore, std::string* realIpPtr = NULL);

        PostLogStoreLogsResponse SynPostLogStoreLogs(const std::string& project,
                                                     const std::string& logstore,
                                                     const std::string& body,
                                                     std::map<std::string, std::string>& httpHeader,
                                                     const std::string& hashKey,
                                                     std::string* realIpPtr = NULL);

        void SetCommonHeader(std::map<std::string, std::string>& httpHeader,
                             int32_t contentLength,
                             const std::string& project = "");

    protected:
        int32_t mSlsHostUpdateTime;
        int32_t mSlsRealIpUpdateTime;
        std::string mRawSlsHost;
        std::string mSlsHost;
        std::string mAccessKeyId;
        std::string mAccessKey;
        std::string mSecurityToken;
        std::string mSource;
        bool mCompressFlag;
        int32_t mTimeout;
        std::string mUserAgent;
        std::string mKeyProvider;
        std::string mHostFieldSuffix;
        bool mIsHostRawIp;
        std::string mInterface;
        int32_t mPort;
        bool mUsingHTTPS;

        SpinLock mSpinLock;

        HTTPClient* mClient;
    };

} // namespace sdk

} // namespace logtail
