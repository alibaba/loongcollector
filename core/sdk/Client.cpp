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

#include "Client.h"
#include "logger/Logger.h"
#include "Exception.h"
#include "Result.h"
#include "CurlImp.h"

namespace logtail {
namespace sdk {

    using namespace std;

#define LOG_SDK_IDENTIFICATION "ali-log-logtail"


    static string GetHostIp(const std::string& intf) {
        // @todo
        return "127.0.0.1";
    }

    Client::Client(const string& slsHost,
                   const string& accessKeyId,
                   const string& accessKey,
                   int32_t timeout,
                   const string& source,
                   bool compressFlag,
                   const string& intf)
        : mAccessKeyId(accessKeyId),
          mAccessKey(accessKey),
          mSource(source),
          mCompressFlag(compressFlag),
          mTimeout(timeout),
          mUserAgent(LOG_SDK_IDENTIFICATION),
          mKeyProvider(""),
          mHostFieldSuffix(""),
          mIsHostRawIp(false),
          mPort(80),
          mUsingHTTPS(false) {
        mClient = new CurlClient();
        mInterface = intf;
        mSlsHostUpdateTime = 0;
        mSlsRealIpUpdateTime = 0;
        SetSlsHost(slsHost);
        if (mSource.empty()) {
            mSource = GetHostIp(mInterface);
        }
        if (mTimeout <= 0) {
            mTimeout = LOG_REQUEST_TIMEOUT;
        }
    }
    Client::Client(const string& slsHost,
                   const string& accessKeyId,
                   const string& accessKey,
                   const std::string& securityToken,
                   int32_t timeout,
                   const string& source,
                   bool compressFlag,
                   const string& intf)
        : mAccessKeyId(accessKeyId),
          mAccessKey(accessKey),
          mSecurityToken(securityToken),
          mSource(source),
          mCompressFlag(compressFlag),
          mTimeout(timeout),
          mUserAgent(LOG_SDK_IDENTIFICATION),
          mKeyProvider(""),
          mHostFieldSuffix(""),
          mIsHostRawIp(false),
          mPort(80),
          mUsingHTTPS(false) {
        mClient = new CurlClient();
        mInterface = intf;
        mSlsHostUpdateTime = 0;
        mSlsRealIpUpdateTime = 0;
        SetSlsHost(slsHost);
        if (mSource.empty()) {
            mSource = GetHostIp(mInterface);
        }
        if (mTimeout <= 0) {
            mTimeout = LOG_REQUEST_TIMEOUT;
        }
    }

    Client::~Client() throw() {}

    void Client::SetPort(int32_t port) {
        mPort = port;
        mUsingHTTPS = (443 == mPort);
    }


    void Client::SetAccessKey(const string& accessKey) {
        mSpinLock.lock();
        mAccessKey = accessKey;
        mSpinLock.unlock();
    }

    string Client::GetAccessKey() {
        mSpinLock.lock();
        string accessKey = mAccessKey;
        mSpinLock.unlock();
        return accessKey;
    }

    void Client::SetAccessKeyId(const string& accessKeyId) {
        mSpinLock.lock();
        mAccessKeyId = accessKeyId;
        mSpinLock.unlock();
    }

    string Client::GetAccessKeyId() {
        mSpinLock.lock();
        string accessKeyId = mAccessKeyId;
        mSpinLock.unlock();
        return accessKeyId;
    }

    string Client::GetSlsHost() {
        mSpinLock.lock();
        string slsHost = mSlsHost;
        mSpinLock.unlock();
        return slsHost;
    }

    string Client::GetRawSlsHost() {
        mSpinLock.lock();
        string rawSlsHost = mRawSlsHost;
        mSpinLock.unlock();
        return rawSlsHost;
    }

    string Client::GetHostFieldSuffix() {
        mSpinLock.lock();
        string hostFieldSuffix = mHostFieldSuffix;
        mSpinLock.unlock();
        return hostFieldSuffix;
    }

    bool Client::GetRawSlsHostFlag() { return mIsHostRawIp; }

    void Client::SetSlsHost(const string& slsHost) {
        mSpinLock.lock();
        if (slsHost == mRawSlsHost) {
            mSpinLock.unlock();
            return;
        }
        mRawSlsHost = slsHost;
        size_t bpos = slsHost.find("://");
        if (bpos == string::npos)
            bpos = 0;
        else
            bpos += 3;
        string tmpstr = slsHost.substr(bpos);
        size_t epos = tmpstr.find_first_of("/");
        if (epos == string::npos)
            epos = tmpstr.length();
        string host = tmpstr.substr(0, epos);

        mSlsHost = host;

        mHostFieldSuffix = "." + host;
        size_t i = 0;
        for (; i < host.length(); ++i) {
            if ((host[i] >= 'a' && host[i] <= 'z') || (host[i] >= 'A' && host[i] <= 'Z'))
                break;
        }
        if (i == host.length())
            mIsHostRawIp = true;
        else
            mIsHostRawIp = false;
        mSpinLock.unlock();
    }


    string Client::GetHost(const string& project) {
        if (mIsHostRawIp || project.empty()) {
            return GetSlsHost();
        } else {
            return project + GetHostFieldSuffix();
        }
    }

    GetRealIpResponse Client::GetRealIp() {
        static string project = "logtail-real-ip-project";
        static string logstore = "logtail-real-ip-logstore";
        GetRealIpResponse rsp;
        try {
            PingSLSServer(project, logstore, &rsp.realIp);
        } catch (const LOGException&) {
        }
        return rsp;
    }

    bool Client::TestNetwork() {
        static string project = "logtail-test-network-project";
        static string logstore = "logtail-test-network-logstore";
        PingSLSServer(project, logstore);
        return true;
    }

    PostLogStoreLogsResponse Client::PostLogStoreLogs(const std::string& project,
                                                      const std::string& logstore,
                                                      const std::string& compressedLogGroup,
                                                      uint32_t rawSize,
                                                      const std::string& hashKey) {
        map<string, string> httpHeader;
        httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
        if (!mKeyProvider.empty()) {
            httpHeader[X_LOG_KEYPROVIDER] = mKeyProvider;
        }
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(rawSize);
        httpHeader[X_LOG_COMPRESSTYPE] = LOG_LZ4;
        return SynPostLogStoreLogs(project, logstore, compressedLogGroup, httpHeader, hashKey);
    }

    PostLogStoreLogsResponse Client::PostLogStoreLogPackageList(const std::string& project,
                                                                const std::string& logstore,
                                                                const std::string& packageListData,
                                                                const std::string& hashKey) {
        map<string, string> httpHeader;
        httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
        if (!mKeyProvider.empty()) {
            httpHeader[X_LOG_KEYPROVIDER] = mKeyProvider;
        }
        httpHeader[X_LOG_MODE] = LOG_MODE_BATCH_GROUP;
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(packageListData.size());
        httpHeader[X_LOG_COMPRESSTYPE] = LOG_LZ4;
        return SynPostLogStoreLogs(project, logstore, packageListData, httpHeader, hashKey);
    }

    void Client::PostLogStoreLogs(const std::string& project,
                                  const std::string& logstore,
                                  const std::string& compressedLogGroup,
                                  uint32_t rawSize,
                                  PostLogStoreLogsClosure* callBack,
                                  const std::string& hashKey,
                                  int64_t hashKeySeqID) {
        map<string, string> httpHeader;
        httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
        if (!mKeyProvider.empty()) {
            httpHeader[X_LOG_KEYPROVIDER] = mKeyProvider;
        }
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(rawSize);
        httpHeader[X_LOG_COMPRESSTYPE] = LOG_LZ4;
        AsynPostLogStoreLogs(project, logstore, compressedLogGroup, httpHeader, callBack, hashKey, hashKeySeqID);
    }

    void Client::PostLogStoreLogPackageList(const std::string& project,
                                            const std::string& logstore,
                                            const std::string& packageListData,
                                            PostLogStoreLogsClosure* callBack,
                                            const std::string& hashKey) {
        map<string, string> httpHeader;
        httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
        if (!mKeyProvider.empty()) {
            httpHeader[X_LOG_KEYPROVIDER] = mKeyProvider;
        }
        httpHeader[X_LOG_MODE] = LOG_MODE_BATCH_GROUP;
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(packageListData.size());
        httpHeader[X_LOG_COMPRESSTYPE] = LOG_LZ4;
        AsynPostLogStoreLogs(project, logstore, packageListData, httpHeader, callBack, hashKey, kInvalidHashKeySeqID);
    }

    void Client::SendRequest(const std::string& project,
                             const std::string& httpMethod,
                             const std::string& url,
                             const std::string& body,
                             const std::map<std::string, std::string>& parameterList,
                             std::map<std::string, std::string>& header,
                             HttpMessage& httpMessage,
                             std::string* realIpPtr) {
        string host = GetHost(project);
        SetCommonHeader(header, (int32_t)(body.length()), project);
        string signature = GetUrlSignature(httpMethod, url, header, parameterList, body, GetAccessKey());
        header[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + GetAccessKeyId() + ':' + signature;

        string queryString;
        GetQueryString(parameterList, queryString);

        int32_t port = mPort;
        if (mPort == 80 && mUsingHTTPS) {
            port = 443;
        }
        mClient->Send(
            httpMethod, host, port, url, queryString, header, body, mTimeout, httpMessage, mInterface, mUsingHTTPS);

        if (httpMessage.statusCode != 200) {
            if (realIpPtr != NULL) {
                *realIpPtr = httpMessage.header[X_LOG_HOSTIP];
            }
            ErrorCheck(httpMessage.content, httpMessage.header[X_LOG_REQUEST_ID], httpMessage.statusCode);
        }
    }

    void Client::AsynPostLogStoreLogs(const std::string& project,
                                      const std::string& logstore,
                                      const std::string& body,
                                      std::map<std::string, std::string>& httpHeader,
                                      PostLogStoreLogsClosure* callBack,
                                      const std::string& hashKey,
                                      int64_t hashKeySeqID) {
        string operation = LOGSTORES;
        operation.append("/").append(logstore);
        if (hashKey.empty())
            operation.append("/shards/lb");
        else
            operation.append("/shards/route");

        httpHeader[CONTENT_MD5] = CalcMD5(body);

        map<string, string> parameterList;
        if (!hashKey.empty()) {
            parameterList["key"] = hashKey;
            if (hashKeySeqID != kInvalidHashKeySeqID) {
                parameterList["seqid"] = std::to_string(hashKeySeqID);
            }
        }

        string host = GetHost(project);
        SetCommonHeader(httpHeader, (int32_t)(body.length()), project);
        string signature = GetUrlSignature(HTTP_POST, operation, httpHeader, parameterList, body, GetAccessKey());
        httpHeader[AUTHORIZATION] = LOG_HEADSIGNATURE_PREFIX + GetAccessKeyId() + ':' + signature;

        string queryString;
        GetQueryString(parameterList, queryString);

        int32_t port = mPort;
        if (mPort == 80 && mUsingHTTPS) {
            port = 443;
        }
        Response* response = new PostLogStoreLogsResponse();
        AsynRequest* request = new AsynRequest(HTTP_POST,
                                               host,
                                               port,
                                               operation,
                                               queryString,
                                               httpHeader,
                                               body,
                                               mTimeout,
                                               mInterface,
                                               mUsingHTTPS,
                                               callBack,
                                               response);
        mClient->AsynSend(request);
    }

    PostLogStoreLogsResponse
    Client::PingSLSServer(const std::string& project, const std::string& logstore, std::string* realIpPtr) {
        sls_logs::LogGroup logGroup;
        logGroup.set_source(mSource);
        auto serializeData = logGroup.SerializeAsString();

        std::map<string, string> httpHeader;
        httpHeader[CONTENT_TYPE] = TYPE_LOG_PROTOBUF;
        if (!mKeyProvider.empty()) {
            httpHeader[X_LOG_KEYPROVIDER] = mKeyProvider;
        }
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(serializeData.size());
        return SynPostLogStoreLogs(project, logstore, serializeData, httpHeader, "", realIpPtr);
    }

    PostLogStoreLogsResponse Client::SynPostLogStoreLogs(const std::string& project,
                                                         const std::string& logstore,
                                                         const std::string& body,
                                                         std::map<std::string, std::string>& httpHeader,
                                                         const std::string& hashKey,
                                                         std::string* realIpPtr) {
        string operation = LOGSTORES;
        operation.append("/").append(logstore);
        if (hashKey.empty())
            operation.append("/shards/lb");
        else
            operation.append("/shards/route");

        httpHeader[CONTENT_MD5] = CalcMD5(body);

        map<string, string> parameterList;
        if (!hashKey.empty())
            parameterList["key"] = hashKey;

        HttpMessage httpResponse;
        SendRequest(project, HTTP_POST, operation, body, parameterList, httpHeader, httpResponse, realIpPtr);

        PostLogStoreLogsResponse ret;
        ret.bodyBytes = (int32_t)body.size();
        ret.statusCode = httpResponse.statusCode;
        ret.requestId = httpResponse.header[X_LOG_REQUEST_ID];
        return ret;
    }

    PostLogStoreLogsResponse Client::PostLogUsingWebTracking(const std::string& project,
                                                             const std::string& logstore,
                                                             const std::string& compressedLog,
                                                             uint32_t rawSize) {
        map<string, string> httpHeader;
        httpHeader[X_LOG_COMPRESSTYPE] = LOG_LZ4;
        httpHeader[X_LOG_BODYRAWSIZE] = std::to_string(rawSize);
        SetCommonHeader(httpHeader, (int32_t)(compressedLog.length()), project);

        string operation = LOGSTORES;
        operation.append("/").append(logstore).append("/track");

        string host = GetHost(project);
        int32_t port = mPort;
        if (mPort == 80 && mUsingHTTPS) {
            port = 443;
        }

        HttpMessage httpResponse;
        mClient->Send(HTTP_POST,
                      host,
                      port,
                      operation,
                      "",
                      httpHeader,
                      compressedLog,
                      mTimeout,
                      httpResponse,
                      mInterface,
                      mUsingHTTPS);

        PostLogStoreLogsResponse ret;
        ret.bodyBytes = (int32_t)compressedLog.length();
        ret.statusCode = httpResponse.statusCode;
        ret.requestId = httpResponse.header[X_LOG_REQUEST_ID];
        return ret;
    }

    void Client::SetCommonHeader(map<string, string>& httpHeader, int32_t contentLength, const string& project) {
        if (!project.empty()) {
            httpHeader[HOST] = project + GetHostFieldSuffix();
        } else {
            httpHeader[HOST] = GetSlsHost();
        }

        httpHeader[USER_AGENT] = mUserAgent;
        httpHeader[X_LOG_APIVERSION] = LOG_API_VERSION;
        httpHeader[X_LOG_SIGNATUREMETHOD] = HMAC_SHA1;
        httpHeader[DATE] = GetDateString();
        httpHeader[CONTENT_LENGTH] = std::to_string(contentLength);
        if (!mSecurityToken.empty()) {
            httpHeader[X_ACS_SECURITY_TOKEN] = mSecurityToken;
        }
    }
} // namespace sdk
} // namespace logtail
