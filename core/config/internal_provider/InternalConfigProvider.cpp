/*
 * Copyright 2024 iLogtail Authors
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

#include "InternalConfigProvider.h"
#ifdef __ENTERPRISE__
#include "EnterpriseInternalConfigProvider.h"
#endif

namespace logtail {

InternalConfigProvider* InternalConfigProvider::GetInstance() {
#ifdef __ENTERPRISE__
    static InternalConfigProvider* ptr = new EnterpriseInternalConfigProvider();
#else
    static InternalConfigProvider* ptr = new InternalConfigProvider();
#endif
    return ptr;
}

const std::map<std::string, std::string>& InternalConfigProvider::GetAllInernalPipelineConfigs() {
    return mPipelineMaps;
}

} // namespace logtail