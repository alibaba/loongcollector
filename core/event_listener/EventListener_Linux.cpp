// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "EventListener_Linux.h"
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "logger/Logger.h"
#include "profiler/LogtailAlarm.h"
#include "common/ErrorUtil.h"
#include "common/Flags.h"
#include "controller/EventDispatcher.h"

DECLARE_FLAG_BOOL(fs_events_inotify_enable);

namespace logtail {

const uint32_t EventListener::mWatchEventMask
    = IN_CREATE | IN_MODIFY | IN_MASK_ADD | IN_DELETE_SELF | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE;

logtail::EventListener::~EventListener() {
    Destroy();
}

bool logtail::EventListener::Init() {
    mInotifyFd = inotify_init();
    return mInotifyFd != -1;
}

int logtail::EventListener::AddWatch(const char* dir) {
    return inotify_add_watch(mInotifyFd, dir, mWatchEventMask);
    ;
}

bool logtail::EventListener::RemoveWatch(int wd) {
    return inotify_rm_watch(mInotifyFd, wd) != -1;
}

int32_t logtail::EventListener::ReadEvents(std::vector<logtail::Event*>& eventVec) {
    eventVec.clear();
    if (mInotifyFd < 0) {
        return 0;
    }
    int len = 0;
    ioctl(mInotifyFd, FIONREAD, &len);
    if (len < 1)
        return 0;
    static char* s_lastHalfEventBuf = new char[65536];
    static int32_t s_lastHalfEventSize = 0;

    char* buffer = new char[len + s_lastHalfEventSize];
    if (s_lastHalfEventSize > 0) {
        memcpy(buffer, s_lastHalfEventBuf, s_lastHalfEventSize);
    }
    size_t readLen = read(mInotifyFd, buffer + s_lastHalfEventSize, len);
    if (readLen == 0) {
        LOG_ERROR(sLogger, ("read inotify fd error", ErrnoToString(GetErrno()))("read len", len));
        delete[] buffer;
        return 0;
    }
    // update len
    len = readLen + s_lastHalfEventSize;
    // when read success, set lastHalfSize 0
    s_lastHalfEventSize = 0;
    if (BOOL_FLAG(fs_events_inotify_enable)) {
        static EventDispatcher* dispatcher = EventDispatcher::GetInstance();
        int n = 0;
        struct inotify_event* event;
        while (n < len) {
            // maybe invalid, must check if this packet is a whole packet
            event = (struct inotify_event*)&buffer[n];

            int tailSize = len - n;
            if ((size_t)tailSize < sizeof(struct inotify_event)
                || (size_t)tailSize < event->len + sizeof(struct inotify_event)) {
                s_lastHalfEventSize = tailSize;
                LOG_WARNING(sLogger,
                            ("read notify event abnormal, half packet is readed, proccess size", n)("read len", len));
                memcpy(s_lastHalfEventBuf, buffer + n, tailSize);
                break;
            }


            // when interrupt (config update), must check event buf tail, if not a whole packet, next read will crash
            if (dispatcher->IsInterupt()) {
                n += sizeof(struct inotify_event) + event->len;
                continue;
            }
            EventType etype = 0;
            if (event->mask & IN_Q_OVERFLOW) {
                LOG_INFO(sLogger, ("inotify event queue overflow", "miss inotify events"));
                LogtailAlarm::GetInstance()->SendAlarm(INOTIFY_EVENT_OVERFLOW_ALARM, "inotify event queue overflow");
            } else {
                etype |= event->mask & IN_DELETE_SELF ? EVENT_TIMEOUT : 0;
                etype |= event->mask & IN_CREATE ? EVENT_CREATE : 0;
                etype |= event->mask & IN_MODIFY ? EVENT_MODIFY : 0;
                etype |= event->mask & IN_ISDIR ? EVENT_ISDIR : 0;
                etype |= event->mask & IN_MOVED_FROM ? EVENT_MOVE_FROM : 0;
                etype |= event->mask & IN_MOVED_TO ? EVENT_MOVE_TO : 0;
                etype |= event->mask & IN_DELETE ? EVENT_DELETE : 0;
                std::string path;
                if (dispatcher->IsRegistered(event->wd, path))
                    eventVec.push_back(
                        new Event(path, event->len > 0 ? event->name : "", etype, event->wd, event->cookie));
            }
            n += sizeof(struct inotify_event) + event->len;
        }
    }
    delete[] buffer;
    return (int32_t)eventVec.size();
}

bool logtail::EventListener::IsInit() {
    return mInotifyFd != -1;
}

void logtail::EventListener::Destroy() {
    if (mInotifyFd >= 0)
        close(mInotifyFd);
}

bool EventListener::IsValidID(int id) {
    return id >= 0;
}

} // namespace logtail
