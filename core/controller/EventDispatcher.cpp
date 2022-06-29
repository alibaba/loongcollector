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

#include "EventDispatcher.h"
#if defined(__linux__)
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <fnmatch.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <vector>
#include <sys/types.h>
#if !defined(LOGTAIL_NO_TC_MALLOC)
#include <gperftools/malloc_extension.h>
#include <gperftools/tcmalloc.h>
#endif
#include "common/version.h"
#include "common/util.h"
#include "common/StringTools.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/ErrorUtil.h"
#include "common/GlobalPara.h"
#include "common/FileSystemUtil.h"
#include "common/TimeUtil.h"
#ifdef __linux__
#include "streamlog/StreamLogManager.h"
#endif
#include "app_config/AppConfig.h"
#include "event_handler/EventHandler.h"
#include "event_handler/LogInput.h"
#include "event/Event.h"
#include "processor/LogProcess.h"
#include "sender/Sender.h"
#include "profiler/LogFileProfiler.h"
#include "profiler/LogtailAlarm.h"
#include "profiler/LogIntegrity.h"
#include "profiler/LogLineCount.h"
#include "log_pb/metric.pb.h"
#include "log_pb/sls_logs.pb.h"
#include "checkpoint/CheckPointManager.h"
#include "checkpoint/CheckpointManagerV2.h"
#include "shennong/MetricSender.h"
#include "polling/PollingDirFile.h"
#include "polling/PollingModify.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "polling/PollingEventQueue.h"
#endif
#include "plugin/LogtailPlugin.h"
#include "config_manager/ConfigManager.h"
#if !defined(_MSC_VER)
#include "LogtailInsightDispatcher.h"
#endif

#ifdef LOGTAIL_RUNTIME_PLUGIN
#include "LogtailRuntimePlugin.h"
#endif

using std::string;
using std::vector;
using namespace sls_logs;

DECLARE_FLAG_INT32(check_point_dump_interval);
DECLARE_FLAG_INT32(ilogtail_max_epoll_events);
DECLARE_FLAG_INT32(ilogtail_epoll_wait_events);
DECLARE_FLAG_INT64(max_logtail_writer_packet_size);

namespace logtail {
EventDispatcher::EventDispatcher() : EventDispatcherBase() {
}

EventDispatcher::~EventDispatcher() {
}

bool EventDispatcher::IsInterupt() {
    if (LogtailGlobalPara::Instance()->GetSigtermFlag()) {
        // LOG_DEBUG(sLogger, ("logtail received SIGTERM signal", "break process loop"));
    } else if (ConfigManager::GetInstance()->IsUpdateConfig()) {
        // LOG_DEBUG(sLogger, ("logtail prepare update config", "break process loop"));
    } else
        return false;

    return true;
}

#if defined(_MSC_VER)
void EventDispatcher::InitWindowsSignalObject() {
}

void EventDispatcher::SyncWindowsSignalObject() {
}

void EventDispatcher::ReleaseWindowsSignalObject() {
}
#endif

#if defined(__linux__)
void EventDispatcher::InitShennong() {
}

void EventDispatcher::CheckShennong() {
}
#endif

void EventDispatcher::ExtraWork() {
}

} // namespace logtail
