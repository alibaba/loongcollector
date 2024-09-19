/*
 * Copyright 2024 iLogtail Authors
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

#include "Provider.h"

#include "config/common_provider/CommonConfigProvider.h"
#include "config/common_provider/LegacyCommonConfigProvider.h"

namespace logtail {


std::vector<ConfigProvider*> GetRemoteConfigProviders() {
    std::vector<ConfigProvider*> providers;
    providers.push_back(LegacyCommonConfigProvider::GetInstance());
    providers.push_back(CommonConfigProvider::GetInstance());
    return providers;
}

void InitRemoteConfigProviders() {
    CommonConfigProvider::GetInstance()->Init("common_v2");
    LegacyCommonConfigProvider::GetInstance()->Init("common");
}

ProfileSender* GetProfileSender() {
    return ProfileSender::GetInstance();
}

} // namespace logtail