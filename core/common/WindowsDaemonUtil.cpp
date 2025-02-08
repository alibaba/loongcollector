// Copyright 2024 iLogtail Authors
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
#include <vector>
#include "WindowsDaemonUtil.h"
#include "application/Application.h"
#include "common/Flags.h"
#include "common/RuntimeUtil.h"
#include "logger/Logger.h"
#include <windows.h>
using std::string;
using std::vector;
DEFINE_FLAG_INT32(win_open_global_objects_interval,
                  "interval to retry openning global objects in daemon, seconds",
                  60 * 30);
namespace logtail {
HANDLE g_DaemonMutex = NULL;
HANDLE g_DaemonEvent = NULL;
static bool OpenGlobalObjects(std::string& err) {
    if (g_DaemonMutex != NULL && g_DaemonEvent != NULL)
        return true;
    auto binaryPath = logtail::GetBinaryName();
    auto namePrefix = std::string("ILOGTAIL_HANDLE_PREFIX");
    auto index = binaryPath.rfind("\\");
    if (index > 0)
        namePrefix = binaryPath.substr(0, index);
    for (auto& c : namePrefix) {
        if ('\\' == c)
            c = '_';
    }
    g_DaemonMutex = OpenMutex(SYNCHRONIZE, FALSE, (namePrefix + "_mutex").c_str());
    if (NULL == g_DaemonMutex) {
        err = "Open daemon mutex failed, can not connect to daemon" + std::to_string(GetLastError());
        return false;
    }
    g_DaemonEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, (namePrefix + "_event").c_str());
    if (NULL == g_DaemonEvent) {
        err = "Open daemon event failed, can not connect to daemon" + std::to_string(GetLastError());
        CloseHandle(g_DaemonMutex);
        g_DaemonMutex = NULL;
        return false;
    }
    return true;
}
void InitWindowsSignalObject() {
    // Open global named objects to communicate with daemon.
    std::string errMsg;
    if (!OpenGlobalObjects(errMsg)) {
        LOG_ERROR(sLogger, ("Open global objects failed, disabled self update, error", errMsg));
    }
}
void SyncWindowsSignalObject() {
    // Check daemon mutex and assert heartbeat with event.
    if (NULL == g_DaemonMutex) {
        static auto lastOpenTime = time(0);
        if (time(0) - lastOpenTime >= INT32_FLAG(win_open_global_objects_interval)) {
            std::string err;
            if (!OpenGlobalObjects(err)) {
                LOG_ERROR(sLogger, ("Retry to open global objects failed", err));
            } else {
                LOG_INFO(sLogger, ("Retry to open global objects success, enable self update", ""));
            }
            lastOpenTime = time(0);
        }
    }
    if (g_DaemonMutex != NULL) {
        if (WAIT_OBJECT_0 == WaitForSingleObject(g_DaemonMutex, 0)) {
            LOG_INFO(sLogger, ("Stop signal from daemon", ""));
            Application::GetInstance()->SetSigTermSignalFlag(true);
            LOG_INFO(sLogger, ("ReleaseMutex", ReleaseMutex(g_DaemonMutex)));
        }
        if (FALSE == SetEvent(g_DaemonEvent)) {
            LOG_ERROR(sLogger, ("SetEvent error", GetLastError()));
        }
    }
}
void ReleaseWindowsSignalObject() {
    if (g_DaemonMutex)
        CloseHandle(g_DaemonMutex);
    if (g_DaemonEvent)
        CloseHandle(g_DaemonEvent);
}
} // namespace logtail
