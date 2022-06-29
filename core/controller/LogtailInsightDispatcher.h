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
#define INSIGHT_CMD_TYPE 0x19

#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stddef.h>
#include <string>
#include <map>
#include "log_pb/sls_logs.pb.h"

namespace logtail {

struct LogFileInfo;
struct LogFileCollectProgress;

class LogtailInsightDispatcher {
public:
    static LogtailInsightDispatcher* GetInstance() {
        static LogtailInsightDispatcher* insightDispatcher = new LogtailInsightDispatcher();
        return insightDispatcher;
    }
    ~LogtailInsightDispatcher() {}

    int32_t ExecuteCommand(int fd, const char* cmdBuf, int cmdSize);

protected:
    LogtailInsightDispatcher() {}

private:
    bool GetAllFileProgress(sls_logs::LogGroup* logGroup);
    int GetFileProgress(const std::string& filename, sls_logs::LogGroup* logGroup);
    void BuildLogGroup(sls_logs::LogGroup* logGroup, const LogFileInfo& info, const LogFileCollectProgress& progress);
};

} // namespace logtail
