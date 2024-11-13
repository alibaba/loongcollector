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

#pragma once

#include "config/provider/ConfigProvider.h"
#include "monitor/profile_sender/ProfileSender.h"

namespace logtail {
/*
    providers modules are used to replace the default implementation of ilogtail.
*/

// GetRemoteConfigProviders returns a vector of pairs for remote config providers.
// It currently returns two providers: LegacyCommonConfigProvider and CommonConfigProvider.
std::vector<ConfigProvider*> GetRemoteConfigProviders();

// InitRemoteConfigProviders initializes the remote config providers.
// It currently initializes the LegacyCommonConfigProvider and CommonConfigProvider.
void InitRemoteConfigProviders();

// GetProfileSender returns the ProfileSender instance.
ProfileSender* GetProfileSender();
} // namespace logtail