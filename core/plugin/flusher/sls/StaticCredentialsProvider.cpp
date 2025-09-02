// Copyright 2025 loongcollector Authors
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

#include "plugin/flusher/sls/StaticCredentialsProvider.h"

using namespace std;
namespace logtail {

StaticCredentialsProvider::StaticCredentialsProvider(const std::string& accessKeyId,
                                                     const std::string& accessKeySecret) {
    mAccessKeyId = accessKeyId;
    mAccessKeySecret = accessKeySecret;
}

bool StaticCredentialsProvider::GetCredentials(AuthType& type,
                                               std::string& accessKeyId,
                                               std::string& accessKeySecret,
                                               std::string& secToken) {
    type = mAuthType;
    accessKeyId = mAccessKeyId;
    accessKeySecret = mAccessKeySecret;
    secToken = mSecToken;
    return true;
}

void StaticCredentialsProvider::SetAuthType(AuthType type) {
    mAuthType = type;
}

} // namespace logtail
