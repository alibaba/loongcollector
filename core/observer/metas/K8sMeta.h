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
#include <unordered_map>

namespace logtail {

struct K8sPodMeta {
    std::string NameSpace;
    std::string WorkloadName; // deploy/daemonset/statefulset
    std::string PodName;
    std::string PodUUID;
    std::unordered_map<std::string, std::string> Labels;

    void Clear() {
        NameSpace.clear();
        WorkloadName.clear();
        PodName.clear();
        PodUUID.clear();
        Labels.erase(Labels.begin(), Labels.end());
    }
};

struct ContainerMeta {
    std::string Image;
    std::string ContainerName;
    std::string ContainerID;
    std::unordered_map<std::string, std::string> Labels;
    std::unordered_map<std::string, std::string> Envs;

    void Clear() {
        Image.clear();
        ContainerName.clear();
        ContainerID.clear();
        Labels.erase(Labels.begin(), Labels.end());
        Envs.erase(Envs.begin(), Envs.end());
    }
};

} // namespace logtail
