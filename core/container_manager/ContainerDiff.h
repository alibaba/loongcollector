


/*
 * Copyright 2023 iLogtail Authors
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

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "file_server/ContainerInfo.h"

namespace logtail {

struct ContainerDiff {
    std::vector<std::shared_ptr<RawContainerInfo>> mAdded;
    std::vector<std::shared_ptr<RawContainerInfo>> mModified;
    std::vector<std::string> mRemoved;

    bool IsEmpty() { return mRemoved.empty() && mAdded.empty() && mModified.empty(); }

    std::string ToString() const {
        std::stringstream ss;
        ss << "Added: ";
        for (const auto& container : mAdded) {
            ss << "{" << "containerName:" << container->mName << " ";
            ss << "containerID:" << container->mID << " ";
            ss << "containerStatus:" << container->mStatus << " ";
            ss << "}" << " ";
        }
        ss << "Modified: ";
        for (const auto& container : mModified) {
            ss << "{" << "containerName:" << container->mName << " ";
            ss << "containerID:" << container->mID << " ";
            ss << "containerStatus:" << container->mStatus << " ";
            ss << "}" << " ";
        }
        ss << "Removed: ";
        for (const auto& containerID : mRemoved) {
            ss << containerID << " ";
        }
        return ss.str();
    }
};

} // namespace logtail
