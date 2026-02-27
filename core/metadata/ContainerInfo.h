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
#include <ctime>

#include <string>
#include <unordered_map>
#include <vector>

namespace logtail {

struct ContainerMeta {
    std::string mContainerId;
    std::string mContainerName;
    // std::string mImageId;
    std::string mImageName;
    // std::string mContainerIp;
};

struct K8sPodInfo {
    std::unordered_map<std::string, std::string> mImages;
    std::unordered_map<std::string, std::string> mLabels;
    std::string mNamespace;
    std::string mServiceName;
    std::string mWorkloadKind;
    std::string mWorkloadName;
    std::time_t mTimestamp;
    std::string mAppId;
    std::string mAppName;
    std::string mPodIp;
    std::string mPodName;
    // std::string mPodUid;
    int64_t mStartTime;
    std::vector<std::string> mContainerIds;
};

} // namespace logtail
