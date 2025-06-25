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

#include "ebpf/plugin/ProcessExecveRetryableEvent.h"

#include <memory>

#include "common/StringTools.h"
#include "ebpf/plugin/ProcessCleanupRetryableEvent.h"
#include "ebpf/type/table/BaseElements.h"
#include "logger/Logger.h"
#include "metadata/ContainerMetadata.h"
#include "metadata/K8sMetadata.h"
#include "security/data_msg.h"

namespace logtail::ebpf {

bool ProcessExecveRetryableEvent::HandleMessage() {
    LOG_DEBUG(sLogger,
              ("pid", mRawEvent->process.pid)("ktime", mRawEvent->process.ktime)("event", "execve")("action",
                                                                                                    "HandleMessage"));
    auto cacheValue = std::make_shared<ProcessCacheValue>();
    fillProcessPlainFields(*mRawEvent, *cacheValue);

    mProcessCacheValue = cacheValue;
    mProcessCache.AddCache({mRawEvent->process.pid, mRawEvent->process.ktime}, cacheValue);
    LOG_DEBUG(
        sLogger,
        ("pid", mRawEvent->process.pid)("ktime", mRawEvent->process.ktime)("event", "execve")("action", "AddCache"));
    if (incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    mCleanupKey = {mRawEvent->cleanup_key.pid, mRawEvent->cleanup_key.ktime};
    mProcessEvent = std::make_shared<ProcessEvent>(static_cast<uint32_t>(mRawEvent->process.pid),
                                                   static_cast<uint64_t>(mRawEvent->process.ktime),
                                                   KernelEventType::PROCESS_EXECVE_EVENT,
                                                   static_cast<uint64_t>(mRawEvent->common.ktime));
    mRawEvent = nullptr;

    if (attachContainerMeta(false)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (attachK8sPodMeta(false)) {
        CompleteTask(kAttachK8sPodMeta);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        cleanupCloneParent();
        return true;
    }
    return false;
}

void ProcessExecveRetryableEvent::fillProcessPlainFields(const msg_execve_event& event, ProcessCacheValue& cacheValue) {
    if (mRawEvent->cleanup_key.ktime == 0 || (mRawEvent->process.flags & EVENT_CLONE) != 0) {
        cacheValue.mPPid = mRawEvent->parent.pid;
        cacheValue.mPKtime = mRawEvent->parent.ktime;
    } else { // process created from execve only
        cacheValue.mPPid = mRawEvent->cleanup_key.pid;
        cacheValue.mPKtime = mRawEvent->cleanup_key.ktime;
    }

    auto execId = GenerateExecId(mHostname, mRawEvent->process.pid, mRawEvent->process.ktime);
    auto userName = mProcParser.GetUserNameByUid(mRawEvent->process.uid);
    auto permitted = GetCapabilities(mRawEvent->creds.caps.permitted, *cacheValue.GetSourceBuffer());
    auto effective = GetCapabilities(mRawEvent->creds.caps.effective, *cacheValue.GetSourceBuffer());
    auto inheritable = GetCapabilities(mRawEvent->creds.caps.inheritable, *cacheValue.GetSourceBuffer());

    fillProcessDataFields(*mRawEvent, cacheValue);
    cacheValue.SetContent<kExecId>(execId);
    cacheValue.SetContent<kProcessId>(mRawEvent->process.pid);
    cacheValue.SetContent<kUid>(mRawEvent->process.uid);
    cacheValue.SetContent<kUser>(userName);
    cacheValue.SetContent<kKtime>(mRawEvent->process.ktime);
    cacheValue.SetContentNoCopy<kCapPermitted>(permitted);
    cacheValue.SetContentNoCopy<kCapEffective>(effective);
    cacheValue.SetContentNoCopy<kCapInheritable>(inheritable);
    // parse exec
    // event->process.tid = eventPtr->process.tid;
    // event->process.nspid = eventPtr->process.nspid;
    // event->process.auid = eventPtr->process.auid;
    // event->process.secure_exec = eventPtr->process.secureexec;
    // event->process.nlink = eventPtr->process.i_nlink;
    // event->process.ino = eventPtr->process.i_ino;

    // dockerid
    StringView ebpfDockerId(mRawEvent->kube.docker_id);
    if (!ebpfDockerId.empty()) {
        StringView containerId;
        ProcParser::LookupContainerId(ebpfDockerId, true, containerId);
        cacheValue.SetContentNoCopy<kContainerId>(containerId);
    }
}

bool ProcessExecveRetryableEvent::fillProcessDataFields(const msg_execve_event& event, ProcessCacheValue& cacheValue) {
    // When filename or args are in event.buffer, they are null terminated.
    // When they are in data_event, they are not null terminated.
    // args && filename
    static const StringView kENoMem = "enomem";
    thread_local std::string filename;
    thread_local std::string argsdata;
    StringView args = kEmptyStringView;
    StringView cwd = kEmptyStringView;
    // verifier size
    // SIZEOF_EVENT is the total size of all fixed fields, = offsetof(msg_process, args) = 56
    auto size = event.process.size - SIZEOF_EVENT; // remain size
    if (size > PADDED_BUFFER - SIZEOF_EVENT) { // size exceed args buffer size
        LOG_ERROR(
            sLogger,
            ("error", "msg exec size larger than argsbuffer")("pid", event.process.pid)("ktime", event.process.ktime));
        cacheValue.SetContentNoCopy<kBinary>(kENoMem);
        cacheValue.SetContentNoCopy<kArguments>(kENoMem);
        cacheValue.SetContentNoCopy<kCWD>(kENoMem);
        return false;
    }

    // executable filename
    const char* buffer = event.buffer + SIZEOF_EVENT; // equivalent to eventPtr->process.args;
    if (event.process.flags & EVENT_DATA_FILENAME) { // filename should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_FILENAME", "msg exec size less than sizeof(data_event_desc)")(
                          "pid", event.process.pid)("ktime", event.process.ktime));
            cacheValue.SetContentNoCopy<kBinary>(kENoMem);
            cacheValue.SetContentNoCopy<kArguments>(kENoMem);
            cacheValue.SetContentNoCopy<kCWD>(kENoMem);
            return false;
        }
        const auto* desc = reinterpret_cast<const data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_FILENAME, size",
                   desc->size)("leftover", desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        filename = mProcessDataMap.DataGetAndRemove(desc);
        if (filename.empty()) {
            LOG_WARNING(
                sLogger,
                ("EVENT_DATA_FILENAME", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        buffer += sizeof(data_event_desc);
        size -= sizeof(data_event_desc);
    } else if ((event.process.flags & EVENT_ERROR_FILENAME) == 0) { // filename should be in process.args
        const char* nullPos = std::find(buffer, buffer + size, '\0');
        filename = std::string(buffer, nullPos - buffer);
        size -= nullPos - buffer;
        if (size == 0) { // no tailing \0 found
            buffer = nullPos;
        } else {
            buffer = nullPos + 1; // skip \0
            --size;
        }
    } else {
        LOG_WARNING(
            sLogger,
            ("EVENT_DATA_FILENAME", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
        filename.clear();
    }

    // args & cmd
    if (event.process.flags & EVENT_DATA_ARGS) { // arguments should be in data cache
        if (size < sizeof(data_event_desc)) {
            LOG_ERROR(sLogger,
                      ("EVENT_DATA_ARGS", "msg exec size less than sizeof(data_event_desc)")("pid", event.process.pid)(
                          "ktime", event.process.ktime));
            cacheValue.SetContent<kBinary>(filename);
            cacheValue.SetContentNoCopy<kArguments>(kENoMem);
            cacheValue.SetContentNoCopy<kCWD>(kENoMem);
            return false;
        }
        const auto* desc = reinterpret_cast<const data_event_desc*>(buffer);
        LOG_DEBUG(sLogger,
                  ("EVENT_DATA_ARGS, size", desc->size)("leftover",
                                                        desc->leftover)("pid", desc->id.pid)("ktime", desc->id.time));
        argsdata = mProcessDataMap.DataGetAndRemove(desc);
        if (argsdata.empty()) {
            LOG_WARNING(sLogger,
                        ("EVENT_DATA_ARGS", "not found in data cache")("pid", desc->id.pid)("ktime", desc->id.time));
        }
        args = argsdata;
        // the remaining data is cwd
        if (size > sizeof(data_event_desc)) {
            cwd = StringView(buffer + sizeof(data_event_desc), size - sizeof(data_event_desc));
        }
    } else if (size > 0) {
        bool hasCwd = false;
        if (((event.process.flags & EVENT_NO_CWD_SUPPORT) | (event.process.flags & EVENT_ERROR_CWD)
             | (event.process.flags & EVENT_ROOT_CWD))
            == 0) {
            hasCwd = true;
        }
        const char* nullPos = nullptr;
        args = StringView(buffer, size);
        if (hasCwd) {
            // find the last \0 to serapate args and cwd
            for (int i = size - 1; i >= 0; i--) {
                if (buffer[i] == '\0') {
                    nullPos = buffer + i;
                    break;
                }
            }
            if (nullPos == nullptr) {
                cwd = StringView(buffer, size);
                args = StringView(buffer, 0);
            } else {
                cwd = StringView(nullPos + 1, size - (nullPos - buffer + 1));
                args = StringView(buffer, nullPos - buffer);
            }
        }
    }
    if (event.process.flags & EVENT_ERROR_ARGS) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_ARGS", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
    }
    if (event.process.flags & EVENT_ERROR_CWD) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_CWD", "ebpf get data error")("pid", event.process.pid)("ktime", event.process.ktime));
    }
    if (event.process.flags & EVENT_ERROR_PATH_COMPONENTS) {
        LOG_WARNING(sLogger,
                    ("EVENT_DATA_CWD", "cwd too long, maybe truncated")("pid", event.process.pid)("ktime",
                                                                                                  event.process.ktime));
    }

    // Post handle cwd
    if (event.process.flags & EVENT_ROOT_CWD) {
        cwd = "/";
    } else if (event.process.flags & EVENT_PROCFS) {
        cwd = Trim(cwd);
    }
    cacheValue.SetContent<kCWD>(cwd);
    // Post handle args
    cacheValue.SetContent<kArguments>(DecodeArgs(args));
    // Post handle binary
    if (filename.size()) {
        if (filename[0] != '/' && !cwd.empty()) {
            // argsdata is not used anymore, as args and cwd has already been SetContent
            argsdata.reserve(cwd.size() + 1 + filename.size());
            if (cwd.back() != '/') {
                argsdata.assign(cwd.data(), cwd.size()).append("/").append(filename);
            } else {
                argsdata.assign(cwd.data(), cwd.size()).append(filename);
            }
            filename.swap(argsdata);
        }
        cacheValue.SetContent<kBinary>(filename);
    } else {
        LOG_WARNING(sLogger,
                    ("filename is empty, should not happen. pid", event.process.pid)("ktime", event.process.ktime));
        cacheValue.SetContentNoCopy<kBinary>(kENoMem);
        return false;
    }
    return true;
}

bool ProcessExecveRetryableEvent::incrementParentRef() {
    if (mProcessCacheValue->mPPid > 0 || mProcessCacheValue->mPKtime > 0) {
        data_event_id key{mProcessCacheValue->mPPid, mProcessCacheValue->mPKtime};
        auto value = mProcessCache.Lookup(key);
        if (!value) {
            return false;
        }
        mProcessCache.IncRef(key, value);
        LOG_DEBUG(sLogger,
                  ("pid", mProcessCacheValue->Get<kProcessId>())("ktime", mProcessCacheValue->Get<kKtime>())(
                      "event", "execve")("action", "IncRef parent")("ppid", mProcessCacheValue->mPPid)(
                      "pktime", mProcessCacheValue->mPKtime));
    }
    return true;
}

bool ProcessExecveRetryableEvent::attachContainerMeta(bool isRetry) {
    const auto& containerId = mProcessCacheValue->Get<kContainerId>();
    if (containerId.empty() || !ContainerMetadata::GetInstance().Enable()) {
        return true;
    }
    auto containerMeta = ContainerMetadata::GetInstance().GetInfoByContainerId(containerId);
    if (containerMeta) {
        if (!isRetry) {
            mProcessCacheValue->StoreContainerInfoUnsafe(containerMeta);
        } else {
            mProcessCacheValue->StoreContainerInfo(containerMeta);
        }
        return true;
    }
    return false;
}

bool ProcessExecveRetryableEvent::attachK8sPodMeta(bool isRetry) {
    const auto& containerId = mProcessCacheValue->Get<kContainerId>();
    if (containerId.empty() || !K8sMetadata::GetInstance().Enable()) {
        return true;
    }
    auto info = K8sMetadata::GetInstance().GetInfoByContainerIdFromCache(containerId);
    if (info) {
        if (!isRetry) {
            mProcessCacheValue->StoreK8sPodInfoUnsafe(info);
        } else {
            mProcessCacheValue->StoreK8sPodInfo(info);
        }
        return true;
    }
    K8sMetadata::GetInstance().AsyncQueryMetadata(PodInfoType::ContainerIdInfo, containerId);
    return false;
}

bool ProcessExecveRetryableEvent::flushEvent() {
    if (!mFlushProcessEvent) {
        return true;
    }
    if (!mCommonEventQueue.try_enqueue(mProcessEvent)) {
        // don't use move as it will set mProcessEvent to nullptr even
        // if enqueue failed, this is unexpected but don't know why
        LOG_WARNING(sLogger,
                    ("event", "Failed to enqueue process execve event")("pid", mProcessEvent->mPid)(
                        "ktime", mProcessEvent->mKtime));
        // TODO: Alarm discard event if it is called by OnDrop
        return false;
    }
    return true;
}

void ProcessExecveRetryableEvent::cleanupCloneParent() {
    if (mCleanupKey.time == 0) {
        return;
    }
    ProcessCleanupRetryableEvent cleanupEvent(mRetryLeft, mCleanupKey, mProcessCache);
    LOG_DEBUG(
        sLogger,
        ("pid", mProcessCacheValue->Get<kProcessId>())("ktime", mProcessCacheValue->Get<kKtime>())("event", "execve")(
            "action", "create cleanupEvent")("cleanpid", mCleanupKey.pid)("cleanktime", mCleanupKey.time));
    if (cleanupEvent.HandleMessage()) {
        return;
    }
    mRetryableEventCache.AddEvent(std::make_shared<ProcessCleanupRetryableEvent>(cleanupEvent));
}

bool ProcessExecveRetryableEvent::OnRetry() {
    if (!IsTaskCompleted(kIncrementParentRef) && incrementParentRef()) {
        CompleteTask(kIncrementParentRef);
    }

    if (!IsTaskCompleted(kAttachContainerMeta) && attachContainerMeta(true)) {
        CompleteTask(kAttachContainerMeta);
    }
    if (!IsTaskCompleted(kAttachK8sPodMeta) && attachK8sPodMeta(false)) {
        CompleteTask(kAttachK8sPodMeta);
    }
    if (AreAllPreviousTasksCompleted(kFlushEvent) && !IsTaskCompleted(kFlushEvent) && flushEvent()) {
        CompleteTask(kFlushEvent);
    }
    if (AreAllPreviousTasksCompleted(kDone)) {
        cleanupCloneParent();
        return true;
    }
    return false;
}

void ProcessExecveRetryableEvent::OnDrop() {
    if (!IsTaskCompleted(kFlushEvent)) {
        flushEvent();
    }
    cleanupCloneParent();
}

} // namespace logtail::ebpf
