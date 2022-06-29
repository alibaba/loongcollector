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

#include "CurlAsynInstance.h"
#include "Closure.h"
#include "Exception.h"
#include "Result.h"
#include <curl/curl.h>
#include <curl/multi.h>
#include "logger/Logger.h"
#include "app_config/AppConfig.h"
#include "common/TimeUtil.h"

using namespace std;

namespace logtail {

namespace sdk {

    CURL* PackCurlRequest(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          HttpMessage& httpMessage,
                          const std::string& intf,
                          const bool httpsFlag,
                          curl_slist*& headers);


    CurlAsynInstance::CurlAsynInstance() {
        for (int i = 0; i < LOGTAIL_SDK_CURL_THREAD_POOL_SIZE; ++i) {
            mMainThreads.push_back(new boost::thread(boost::bind(&CurlAsynInstance::Run, this)));
        }
    }

    CurlAsynInstance::~CurlAsynInstance() {
        for (int i = 0; i < LOGTAIL_SDK_CURL_THREAD_POOL_SIZE; ++i) {
            mMainThreads[i]->join();
            delete mMainThreads[i];
        }
    }

    static bool AddRequestToMultiHandler(CURLM* multi_handle, AsynRequest* request) {
        curl_slist* headers = NULL;
        CURL* curl = PackCurlRequest(request->mHTTPMethod,
                                     request->mHost,
                                     request->mPort,
                                     request->mUrl,
                                     request->mQueryString,
                                     request->mHeader,
                                     request->mBody,
                                     request->mTimeout,
                                     request->mCallBack->mHTTPMessage,
                                     request->mInterface,
                                     request->mHTTPSFlag,
                                     headers);
        if (curl == NULL) {
            request->mCallBack->OnFail(request->mResponse, LOGE_UNKNOWN_ERROR, "Init curl fail.");
            delete request;
            return false;
        }
        request->mPrivateData = headers;
        curl_easy_setopt(curl, CURLOPT_PRIVATE, request);
        auto addRst = curl_multi_add_handle(multi_handle, curl);
        if (addRst != CURLM_OK) {
            request->mCallBack->OnFail(
                request->mResponse, LOGE_UNKNOWN_ERROR, "curl_multi_add_handle failed: " + std::to_string(addRst));
            delete request;
            return false;
        }
        return true;
    }

    static void on_handle_done(CURL* curl, curl_slist* headers, AsynRequest* request, CURLcode res) {
        if (headers != NULL) {
            curl_slist_free_all(headers);
        }

        switch (res) {
            case CURLE_OK:
                break;
            case CURLE_OPERATION_TIMEDOUT:
                curl_easy_cleanup(curl);
                request->mCallBack->OnFail(request->mResponse, LOGE_REQUEST_ERROR, "Request operation timeout.");
                return;
            case CURLE_COULDNT_CONNECT:
                curl_easy_cleanup(curl);
                request->mCallBack->OnFail(request->mResponse, LOGE_REQUEST_ERROR, "Can not connect to server.");
                return;
            default:
                curl_easy_cleanup(curl);
                request->mCallBack->OnFail(request->mResponse,
                                           LOGE_REQUEST_ERROR,
                                           string("Request operation failed, curl error code : ")
                                               + curl_easy_strerror(res));
                return;
        }

        long http_code = 0;
        if ((res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code)) != CURLE_OK) {
            curl_easy_cleanup(curl);
            request->mCallBack->OnFail(request->mResponse,
                                       LOGE_UNKNOWN_ERROR,
                                       string("Get curl response code error, curl error code : ")
                                           + curl_easy_strerror(res));
            return;
        }
        request->mCallBack->mHTTPMessage.statusCode = (int32_t)http_code;
        curl_easy_cleanup(curl);
        if (!request->mCallBack->mHTTPMessage.IsLogServiceResponse()) {
            request->mCallBack->OnFail(request->mResponse, LOGE_REQUEST_ERROR, "Get invalid response");
            return;
        }

        const auto& httpMsg = request->mCallBack->mHTTPMessage;

        // Update every 10000 requests.
        if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
            static uint32_t sCount = 0;
            if (sCount++ % 10000 == 0) {
                time_t serverTime = httpMsg.GetServerTimeFromHeader();
                if (serverTime > 0) {
                    logtail::UpdateTimeDelta(serverTime);
                }
            }
        }

        if (httpMsg.statusCode != 200) {
            string requestId = httpMsg.RequestID();
            request->mResponse->SetError((int32_t)http_code, requestId);
            try {
                ErrorCheck(httpMsg.content, requestId, httpMsg.statusCode);
            } catch (LOGException& e) {
                const auto& errCode = e.GetErrorCode();
                const auto& errMsg = e.GetMessage_();
                if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust() && sdk::LOGE_REQUEST_TIME_EXPIRED == errCode) {
                    time_t serverTime = httpMsg.GetServerTimeFromHeader();
                    if (serverTime > 0) {
                        logtail::UpdateTimeDelta(serverTime);
                    }
                }

                request->mCallBack->OnFail(request->mResponse, errCode, errMsg);
            }
        } else {
            request->mResponse->ParseSuccess(httpMsg);
            request->mCallBack->OnSuccess(request->mResponse);
        }
    }

    /* Check for completed transfers, and remove their easy handles */
    static void check_multi_info(CURLM* multi_handle) {
        int msgs_left;
        CURL* easy;
        CURLcode res;
        CURLMsg* msg = curl_multi_info_read(multi_handle, &msgs_left);
        while (msg) {
            if (msg->msg == CURLMSG_DONE) {
                easy = msg->easy_handle;
                res = msg->data.result;
                AsynRequest* request = NULL;
                curl_easy_getinfo(easy, CURLINFO_PRIVATE, &request);
                LOG_DEBUG(sLogger, ("DONE: ", res)(request->mHost + request->mUrl, curl_easy_strerror(res)));
                curl_multi_remove_handle(multi_handle, easy);
                on_handle_done(easy, (curl_slist*)request->mPrivateData, request, res);
                delete request;
            }
            msg = curl_multi_info_read(multi_handle, &msgs_left);
        }
    }

    void CurlAsynInstance::Run() {
        CURLM* multi_handle = curl_multi_init();
        if (multi_handle == NULL) {
            LOG_ERROR(sLogger, ("Init multi curl error", ""));
            return;
        }
        while (true) {
            AsynRequest* request = NULL;
            if (mRequestQueue.wait_and_pop(request)) {
                if (!AddRequestToMultiHandler(multi_handle, request)) {
                    continue;
                }
            }
            MultiHandlerLoop(multi_handle);
        }
    }

    bool CurlAsynInstance::MultiHandlerLoop(CURLM* multi_handle) {
        int still_running = 1;
        /* we start some action by calling perform right away */

        bool needRead = true;
        while (still_running) {
            curl_multi_perform(multi_handle, &still_running);
            check_multi_info(multi_handle);

            struct timeval timeout;
            int rc; /* select() return code */
            CURLMcode mc; /* curl_multi_fdset() return code */

            fd_set fdread;
            fd_set fdwrite;
            fd_set fdexcep;
            int maxfd = -1;

            long curl_timeo = -1;

            FD_ZERO(&fdread);
            FD_ZERO(&fdwrite);
            FD_ZERO(&fdexcep);

            /* set a suitable timeout to play around with */
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            curl_multi_timeout(multi_handle, &curl_timeo);
            if (curl_timeo >= 0) {
                timeout.tv_sec = curl_timeo / 1000;
                if (timeout.tv_sec > 1)
                    timeout.tv_sec = 1;
                else
                    timeout.tv_usec = (curl_timeo % 1000) * 1000;
            }

            /* get file descriptors from the transfers */
            mc = curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

            if (mc != CURLM_OK) {
                fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
                break;
            }

            /* On success the value of maxfd is guaranteed to be >= -1. We call
           select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
           no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
           to sleep 100ms, which is the minimum suggested value in the
           curl_multi_fdset() doc. */
            AsynRequest* request = NULL;
            if (mRequestQueue.try_pop(request)) {
                if (AddRequestToMultiHandler(multi_handle, request)) {
                    ++still_running;
                    continue;
                }
            }
            if (maxfd == -1) {
#ifdef _WIN32
                Sleep(100);
                rc = 0;
#else
                /* Portable sleep for platforms other than Windows. */
                struct timeval wait = {0, 100 * 1000}; /* 100ms */
                rc = select(0, NULL, NULL, NULL, &wait);
#endif
            } else {
                /* Note that on some platforms 'timeout' may be modified by select().
               If you need access to the original value save a copy beforehand. */
                rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
            }

            switch (rc) {
                case -1:
                    /* select error */
                    needRead = false;
                    break;
                case 0:
                default:
                    /* timeout or readable/writable sockets */
                    needRead = true;
                    break;
            }
        }
        return true;
    }


} // namespace sdk
} // namespace logtail
