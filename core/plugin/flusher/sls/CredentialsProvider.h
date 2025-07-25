/*
 * Copyright 2025 loongcollector Authors
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

enum class AuthType { ANONYMOUS, AK, STS };
class CredentialsProvider {
public:
    CredentialsProvider() = default;
    virtual ~CredentialsProvider() = default;

    virtual bool
    GetCredentials(AuthType& type, std::string& accessKeyId, std::string& accessKeySecret, std::string& secToken)
        = 0;
    uint32_t GetErrorCnt() { return mErrorCnt; }
    // Record the number of errors using credentails. The caller can reset the provider or do something else based on
    // the number of errors.
    void SetErrorCnt(uint32_t cnt) { mErrorCnt = cnt; }

protected:
    uint32_t mErrorCnt = 0;
    std::string mAccessKeyId;
    std::string mAccessKeySecret;
    std::string mSecToken;
};

class StaticCredentialsProvider : public CredentialsProvider {
public:
    StaticCredentialsProvider(const std::string& accessKeyId, const std::string& accessKeySecret);

    bool GetCredentials(AuthType& type,
                        std::string& accessKeyId,
                        std::string& accessKeySecret,
                        std::string& secToken) override;

    void SetAuthType(AuthType type);

private:
    AuthType mAuthType = AuthType::AK;
};

} // namespace logtail
