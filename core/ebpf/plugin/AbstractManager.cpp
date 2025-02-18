// Copyright 2023 iLogtail Authors
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

#include "AbstractManager.h"

#include <coolbpf/security/type.h>

#include "common/TimeUtil.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

const std::string AbstractManager::sCallNameKey = "call_name";
const std::string AbstractManager::sEventTypeKey = "event_type";
const std::string AbstractManager::sKprobeValue = "kprobe";


AbstractManager::AbstractManager(std::shared_ptr<BaseManager> bm,
                                 std::shared_ptr<SourceManager> sourceManager,
                                 moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>& queue,
                                 std::shared_ptr<Timer> scheduler)
    : mBaseManager(bm), mSourceManager(sourceManager), mCommonEventQueue(queue), mScheduler(scheduler) {
    mTimeDiff = GetTimeDiffFromMonotonic();
}

int AbstractManager::GetCallNameIdx(const std::string& callName) {
    if (callName == "security_file_permission")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION;
    else if (callName == "security_mmap_file")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_MMAP_FILE;
    else if (callName == "security_path_truncate")
        return SECURE_FUNC_TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE;
    else if (callName == "sys_write")
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_WRITE;
    else if (callName == "sys_read")
        return SECURE_FUNC_TRACEPOINT_FUNC_SYS_READ;
    else if (callName == "tcp_close")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CLOSE;
    else if (callName == "tcp_connect")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_CONNECT;
    else if (callName == "tcp_sendmsg")
        return SECURE_FUNC_TRACEPOINT_FUNC_TCP_SENDMSG;
    LOG_WARNING(sLogger, ("unknown call name", callName));
    return -1;
}

} // namespace ebpf
} // namespace logtail
