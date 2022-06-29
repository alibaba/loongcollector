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

#ifndef _LOG_SDK_BEAST_IMPL_H__
#define _LOG_SDK_BEAST_IMPL_H__

#include <string>
#include <map>
#include "Closure.h"
#include "Common.h"

namespace logtail {

namespace sdk {

    class BeastClient : public HTTPClient {
    public:
        virtual void Send(const std::string& httpMethod,
                          const std::string& host,
                          const int32_t port,
                          const std::string& url,
                          const std::string& queryString,
                          const std::map<std::string, std::string>& header,
                          const std::string& body,
                          const int32_t timeout,
                          HttpMessage& httpMessage,
                          const std::string& interface,
                          const bool httpsFlag);
        virtual void AsynSend(const std::string& httpMethod,
                              const std::string& host,
                              const int32_t port,
                              const std::string& url,
                              const std::string& queryString,
                              const std::map<std::string, std::string>& header,
                              const std::string& body,
                              const int32_t timeout,
                              LogsClosure* callBack,
                              const std::string& interface,
                              const bool httpsFlag);
    };

} // namespace sdk
} // namespace logtail

#endif //_LOG_SDK_BEAST_IMPL_H__
