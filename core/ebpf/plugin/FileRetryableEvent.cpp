// Copyright 2025 iLogtail Authors
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

#include "ebpf/plugin/FileRetryableEvent.h"

#include "ebpf/plugin/ProcessCache.h"
#include "logger/Logger.h"

namespace logtail::ebpf {

bool FileRetryableEvent::HandleMessage() {
    // 创建文件事件
    KernelEventType type;
    switch (mRawEvent->func) {
        case TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION:
            type = KernelEventType::FILE_PERMISSION_EVENT;
            break;
        case TRACEPOINT_FUNC_SECURITY_MMAP_FILE:
            type = KernelEventType::FILE_MMAP;
            break;
        case TRACEPOINT_FUNC_SECURITY_PATH_TRUNCATE:
            type = KernelEventType::FILE_PATH_TRUNCATE;
            break;
        case TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION_WRITE:
            type = KernelEventType::FILE_PERMISSION_EVENT_WRITE;
            break;
        case TRACEPOINT_FUNC_SECURITY_FILE_PERMISSION_READ:
            type = KernelEventType::FILE_PERMISSION_EVENT_READ;
            break;
        default:
            LOG_WARNING(sLogger, ("unknown func", mRawEvent->func));
            return false;
    }

    mFileEvent = std::make_shared<FileEvent>(static_cast<uint32_t>(mRawEvent->key.pid),
                                             static_cast<uint64_t>(mRawEvent->key.ktime),
                                             type,
                                             static_cast<uint64_t>(mRawEvent->timestamp),
                                             StringView(&mRawEvent->path[4]));
    mRawEvent = nullptr;
    // LOG_DEBUG(sLogger, ("event", "file")("action", "HandleMessage")("path", mFileEvent->mPath)("pid",
    // mFileEvent->mPid)("ktime", mFileEvent->mKtime));

    if (!IsTaskCompleted(kFindProcess) && findProcess()) {
        CompleteTask(kFindProcess);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }

    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }

    return false;
}

bool FileRetryableEvent::findProcess() {
    if (mFileEvent->mPid <= 0 || mFileEvent->mKtime <= 0) {
        LOG_WARNING(sLogger, ("file event", "not process pid or ktime"));
        return true;
    }

    auto cacheValue = mProcessCache.Lookup({mFileEvent->mPid, mFileEvent->mKtime});
    if (!cacheValue) {
        LOG_DEBUG(sLogger,
                  ("file event", "process cache not found")("pid", mFileEvent->mPid)("ktime", mFileEvent->mKtime)(
                      "path", mFileEvent->mPath));
        return false;
    }

    return true;
}
bool FileRetryableEvent::flushEvent() {
    if (!mCommonEventQueue.try_enqueue(mFileEvent)) {
        LOG_DEBUG(sLogger,
                  ("event", "Failed to enqueue file event. retrying soon")("pid", mFileEvent->mPid)(
                      "ktime", mFileEvent->mKtime));
        return false;
    }
    return true;
}

bool FileRetryableEvent::OnRetry() {
    if (!IsTaskCompleted(kFindProcess) && findProcess()) {
        CompleteTask(kFindProcess);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && !IsTaskCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        return true;
    }
    return false;
}

void FileRetryableEvent::OnDrop() {
    if (mFileEvent && !IsTaskCompleted(kFlushEvent)) {
        flushEvent();
    }
}

} // namespace logtail::ebpf
