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
#include <map>
#include <string>

#include "Common.h"
#include "CurlImp.h"
#include "protobuf/sls/sls_logs.pb.h"
#include "runner/sink/http/HttpSinkRequest.h"

namespace logtail {
namespace sdk {

    class Client {
    public:
        /** Constructor needs at least three parameters.
         * @param LOGHost LOG service address, for example:http://cn-hangzhou.log.aliyuncs.com.
         * @param timeout Timeout time of one operation.
         */
        Client(const std::string& aliuid,
               const std::string& slsHost,
               int32_t timeout = LOG_REQUEST_TIMEOUT);
        ~Client() throw();

        void SetPort(int32_t port);

        GetRealIpResponse GetRealIp();
        bool TestNetwork();

        std::string GetHost(const std::string& project);

        void SetSlsHost(const std::string& slsHost);
        std::string GetSlsHost();
        std::string GetRawSlsHost();
        std::string GetHostFieldSuffix();
        bool GetRawSlsHostFlag();

        void SetSlsHostUpdateTime(int32_t uptime) { mSlsHostUpdateTime = uptime; }
        int32_t GetSlsHostUpdateTime() { return mSlsHostUpdateTime; }

        void SetSlsRealIpUpdateTime(int32_t uptime) { mSlsRealIpUpdateTime = uptime; }
        int32_t GetSlsRealIpUpdateTime() { return mSlsRealIpUpdateTime; }
        bool IsUsingHTTPS() { return mUsingHTTPS; }

        /////////////////////////////////////Internal Interface For Logtail////////////////////////////////////////
        /** Sync Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
         * @param project The project name
         * @param logstore The logstore name
         * @param compressedLogGroup serialized data of logGroup, LZ4 or ZSTD comressed
         * @param rawSize before compress
         * @param compressType compression type
         * @return request_id.
         */
        PostLogStoreLogsResponse PostLogStoreLogs(const std::string& project,
                                                  const std::string& logstore,
                                                  sls_logs::SlsCompressType compressType,
                                                  const std::string& compressedLogGroup,
                                                  uint32_t rawSize,
                                                  const std::string& hashKey = "",
                                                  bool isTimeSeries = false);

        PostLogStoreLogsResponse PostMetricStoreLogs(const std::string& project,
                                                     const std::string& logstore,
                                                     sls_logs::SlsCompressType compressType,
                                                     const std::string& compressedLogGroup,
                                                     uint32_t rawSize) {
            return PostLogStoreLogs(project, logstore, compressType, compressedLogGroup, rawSize, "", true);
        }


        /** Sync Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
         * @param project The project name
         * @param logstore The logstore name
         * @param packageListData data of logPackageList, consist of several LogGroup
         * @return request_id.
         */
        PostLogStoreLogsResponse PostLogStoreLogPackageList(const std::string& project,
                                                            const std::string& logstore,
                                                            sls_logs::SlsCompressType compressType,
                                                            const std::string& packageListData,
                                                            const std::string& hashKey = "");
        /** Async Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
         * @param project The project name
         * @param logstore The logstore name
         * @param compressedLogGroup data of logGroup, LZ4 comressed
         * @param rawSize before compress
         * @param compressType compression type
         * @return request_id.
         */
        std::unique_ptr<HttpSinkRequest> CreatePostLogStoreLogsRequest(const std::string& project,
                                                                       const std::string& logstore,
                                                                       sls_logs::SlsCompressType compressType,
                                                                       const std::string& compressedLogGroup,
                                                                       uint32_t rawSize,
                                                                       SenderQueueItem* item,
                                                                       const std::string& hashKey = "",
                                                                       int64_t hashKeySeqID = kInvalidHashKeySeqID,
                                                                       bool isTimeSeries = false);
        /** Async Put metrics data to SLS metricstore. Unsuccessful opertaion will cause an LOGException.
         * @param project The project name
         * @param logstore The logstore name
         * @param compressedLogGroup data of logGroup, LZ4 comressed
         * @param rawSize before compress
         * @param compressType compression type
         * @return request_id.
         */
        std::unique_ptr<HttpSinkRequest> CreatePostMetricStoreLogsRequest(const std::string& project,
                                                                          const std::string& logstore,
                                                                          sls_logs::SlsCompressType compressType,
                                                                          const std::string& compressedLogGroup,
                                                                          uint32_t rawSize,
                                                                          SenderQueueItem* item) {
            return CreatePostLogStoreLogsRequest(
                project, logstore, compressType, compressedLogGroup, rawSize, item, "", kInvalidHashKeySeqID, true);
        }


        /** Async Put data to LOG service. Unsuccessful opertaion will cause an LOGException.
         * @param project The project name
         * @param logstore The logstore name
         * @param packageListData data of logPackageList, consist of several LogGroup
         * @return request_id.
         */
        std::unique_ptr<HttpSinkRequest> CreatePostLogStoreLogPackageListRequest(const std::string& project,
                                                                                 const std::string& logstore,
                                                                                 sls_logs::SlsCompressType compressType,
                                                                                 const std::string& packageListData,
                                                                                 SenderQueueItem* item,
                                                                                 const std::string& hashKey = "");

        PostLogStoreLogsResponse PostLogUsingWebTracking(const std::string& project,
                                                         const std::string& logstore,
                                                         sls_logs::SlsCompressType compressType,
                                                         const std::string& compressedLogGroup,
                                                         uint32_t rawSize);
        /////////////////////////////////////////////////////////////////////////////////////////////////

        static std::string GetCompressTypeString(sls_logs::SlsCompressType compressType);
        static sls_logs::SlsCompressType GetCompressType(std::string compressTypeString,
                                                         sls_logs::SlsCompressType defaultType = sls_logs::SLS_CMP_LZ4);

    protected:
        void SendRequest(const std::string& project,
                         const std::string& httpMethod,
                         const std::string& url,
                         const std::string& body,
                         const std::map<std::string, std::string>& parameterList,
                         std::map<std::string, std::string>& header,
                         HttpMessage& httpMessage,
                         std::string* realIpPtr = NULL);

        std::unique_ptr<HttpSinkRequest>
        CreateAsynPostLogStoreLogsRequest(const std::string& project,
                                          const std::string& logstore,
                                          const std::string& body,
                                          std::map<std::string, std::string>& httpHeader,
                                          const std::string& hashKey,
                                          int64_t hashKeySeqID,
                                          SenderQueueItem* item);

        std::unique_ptr<HttpSinkRequest>
        CreateAsynPostMetricStoreLogsRequest(const std::string& project,
                                             const std::string& logstore,
                                             const std::string& body,
                                             std::map<std::string, std::string>& httpHeader,
                                             SenderQueueItem* item);

        // PingSLSServer sends a trivial data packet to SLS for some inner purposes.
        PostLogStoreLogsResponse
        PingSLSServer(const std::string& project, const std::string& logstore, std::string* realIpPtr = NULL);

        PostLogStoreLogsResponse SynPostLogStoreLogs(const std::string& project,
                                                     const std::string& logstore,
                                                     const std::string& body,
                                                     std::map<std::string, std::string>& httpHeader,
                                                     const std::string& hashKey,
                                                     std::string* realIpPtr = NULL);

        PostLogStoreLogsResponse SynPostMetricStoreLogs(const std::string& project,
                                                        const std::string& logstore,
                                                        const std::string& body,
                                                        std::map<std::string, std::string>& httpHeader,
                                                        std::string* realIpPtr = NULL);

        void SetCommonHeader(std::map<std::string, std::string>& httpHeader,
                             int32_t contentLength,
                             const std::string& project = "");

    protected:
        int32_t mSlsHostUpdateTime;
        int32_t mSlsRealIpUpdateTime;
        std::string mRawSlsHost;
        std::string mSlsHost;
        int32_t mTimeout;
        std::string mHostFieldSuffix;
        bool mIsHostRawIp;
        int32_t mPort;
        bool mUsingHTTPS;
        std::string mAliuid;

        SpinLock mSpinLock;

        CurlClient* mClient;
    };

} // namespace sdk

} // namespace logtail
