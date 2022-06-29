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

#ifndef __STREAMLOG_MANAGER_H__
#define __STREAMLOG_MANAGER_H__
#include <queue>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "config_manager/ConfigManager.h"
#include "common/Lock.h"
#include "common/ThreadPool.h"
#include "StreamLogFormat.h"

namespace logtail {

class StreamLogManager {
public:
    StreamLogManager(const uint32_t poolSizeInMb,
                     const uint32_t colSize,
                     const int epollFd,
                     const int rcvThreaNum = 1,
                     const int procThreadNum = 2) {}
    virtual ~StreamLogManager() {}

public:
    void AwakenTimeoutFds() {}
    void ShutdownConfigUsage() {}
    void StartupConfigUsage() {}
    void AddRcvTask(const int fd) {}
    const bool AcceptedFdsContains(const int fd) { return true; }
    void InsertToAcceptedFds(const int fd) {}
    void DeleteFd(const int fd) {}
    void Shutdown() {}
};
}; // namespace logtail

#endif
