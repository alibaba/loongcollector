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

namespace logtail {
namespace sdk {

    /**
 *LOG Exception includes an error code and a detail message.
 */
    class LOGException : public std::exception {
        /** Constructor with error code and message. 
* @param errorCode LOG error code. 
* @param message Detailed information for the exception.
* @return The objcect pointer.
*/
    public:
        LOGException() { mHttpCode = 0; }
        LOGException(const std::string& errorCode,
                     const std::string& message,
                     const std::string& requestId = "",
                     const int32_t httpCode = 0)
            : mErrorCode(errorCode), mMessage(message), mRequestId(requestId), mHttpCode(httpCode) {}
        ~LOGException() throw() {}
        /** Function that return error code. 
    * @param void None. 
    * @return Error code string.
    */
        std::string GetErrorCode(void) const { return mErrorCode; }
        /** Function that return error message. 
    * @param void None. 
    * @return Error message string.
    */
        std::string GetMessage(void) const { return mMessage; }
        std::string GetMessage_() const { return mMessage; }
        /** Function that return request id. 
    * @param void None. 
    * @return request id string.
    */
        std::string GetRequestId(void) const { return mRequestId; }
        /** Function that return http response code. 
    * @param void None.
    * @return http response code int32_t, if client error, return 0.
    */
        int32_t GetHttpCode(void) const { return mHttpCode; }

    private:
        std::string mErrorCode;
        std::string mMessage;
        std::string mRequestId;
        int32_t mHttpCode;
    };

    /**
 *LOG Json Exception includes an error code and a detail message.
 */
    class JsonException : public std::exception {
        /** Constructor with error code and message. 
* @param errorCode LOG error code. 
* @param message Detailed information for the exception.
* @return The objcect pointer.
*/
    public:
        JsonException(const std::string& errorCode, const std::string& message)
            : mErrorCode(errorCode), mMessage(message) {}
        ~JsonException() throw() {}
        /** Function that return error code. 
    * @param void None. 
    * @return Error code string.
    */
        std::string GetErrorCode(void) const { return mErrorCode; }
        /** Function that return error message. 
    * @param void None. 
    * @return Error message string.
    */
        std::string GetMessage(void) const { return mMessage; }

    private:
        std::string mErrorCode;
        std::string mMessage;
    };

} // namespace sdk
} // namespace logtail
