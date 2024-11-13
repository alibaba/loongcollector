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

#include "LogInput.h"

#include <time.h>

#include "app_config/AppConfig.h"
#include "application/Application.h"
#include "checkpoint/CheckPointManager.h"
#include "common/FileSystemUtil.h"
#include "common/HashUtil.h"
#include "common/LogtailCommonFlags.h"
#include "common/RuntimeUtil.h"
#include "common/StringTools.h"
#include "common/TimeUtil.h"
#include "file_server/ConfigManager.h"
#include "file_server/EventDispatcher.h"
#include "file_server/event/BlockEventManager.h"
#include "file_server/event_handler/EventHandler.h"
#include "file_server/event_handler/HistoryFileImporter.h"
#include "file_server/polling/PollingCache.h"
#include "file_server/polling/PollingDirFile.h"
#include "file_server/polling/PollingEventQueue.h"
#include "file_server/polling/PollingModify.h"
#include "file_server/reader/GloablFileDescriptorManager.h"
#include "file_server/reader/LogFileReader.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "monitor/Monitor.h"
#ifdef __ENTERPRISE__
#include "config/provider/EnterpriseConfigProvider.h"
#endif
#include "file_server/FileServer.h"

using namespace std;

DEFINE_FLAG_INT32(check_symbolic_link_interval, "seconds", 120);
DEFINE_FLAG_INT32(check_base_dir_interval, "seconds", 60);
DEFINE_FLAG_INT32(log_input_thread_wait_interval, "microseconds", 20 * 1000);
DEFINE_FLAG_INT64(read_fs_events_interval, "microseconds", 20 * 1000);
DEFINE_FLAG_INT32(check_handler_timeout_interval, "seconds", 180);
DEFINE_FLAG_INT32(dump_inotify_watcher_interval, "seconds", 180);
DEFINE_FLAG_INT32(clear_config_match_interval, "seconds", 600);
DEFINE_FLAG_INT32(check_block_event_interval, "seconds", 1);
DEFINE_FLAG_INT32(read_local_event_interval, "seconds", 60);
DEFINE_FLAG_BOOL(force_close_file_on_container_stopped,
                 "whether close file handler immediately when associate container stopped",
                 false);

DECLARE_FLAG_BOOL(send_prefer_real_ip);


namespace logtail {
LogInput::LogInput() : mAccessMainThreadRWL(ReadWriteLock::PREFER_WRITER) {
    mCheckBaseDirInterval = INT32_FLAG(check_base_dir_interval);
    mCheckSymbolicLinkInterval = INT32_FLAG(check_symbolic_link_interval);
    mInteruptFlag = false;
    mForceClearFlag = false;
    mIdleFlag = false;
    mEventProcessCount = 0;
    mLastUpdateMetricTime = 0;
}

LogInput::~LogInput() {
}

// Start() should only be called once except for UT
void LogInput::Start() {
    mIdleFlag = false;
    static bool initialized = false;
    if (initialized)
        return;
    else
        initialized = true;

    mInteruptFlag = false;

    mLastRunTime = FileServer::GetInstance()->GetMetricsRecordRef().CreateIntGauge(METRIC_RUNNER_LAST_RUN_TIME);
    mRegisterdHandlersTotal
        = FileServer::GetInstance()->GetMetricsRecordRef().CreateIntGauge(METRIC_RUNNER_FILE_WATCHED_DIRS_TOTAL);
    mActiveReadersTotal
        = FileServer::GetInstance()->GetMetricsRecordRef().CreateIntGauge(METRIC_RUNNER_FILE_ACTIVE_READERS_TOTAL);
    mEnableFileIncludedByMultiConfigs = FileServer::GetInstance()->GetMetricsRecordRef().CreateIntGauge(
        METRIC_RUNNER_FILE_ENABLE_FILE_INCLUDED_BY_MULTI_CONFIGS_FLAG);

    new Thread([this]() { ProcessLoop(); });
}

void LogInput::Resume() {
    LOG_INFO(sLogger, ("event handle daemon resume", "starts"));
    mInteruptFlag = false;
    mAccessMainThreadRWL.unlock();
    LOG_INFO(sLogger, ("event handle daemon resume", "succeeded"));
}

void LogInput::HoldOn() {
    LOG_INFO(sLogger, ("event handle daemon pause", "starts"));
    if (BOOL_FLAG(enable_full_drain_mode) && Application::GetInstance()->IsExiting()) {
        unique_lock<mutex> lock(mThreadRunningMux);
        mStopCV.wait(lock, [this]() { return mInteruptFlag; });
    } else {
        mInteruptFlag = true;
        mAccessMainThreadRWL.lock();
    }
    LOG_INFO(sLogger, ("event handle daemon pause", "succeeded"));
}

void LogInput::TryReadEvents(bool forceRead) {
    if (mInteruptFlag)
        return;

    int64_t curMicroSeconds = GetCurrentTimeInMicroSeconds();
    if (forceRead || curMicroSeconds - mLastReadEventMicroSeconds >= INT64_FLAG(read_fs_events_interval)) {
        vector<Event*> inotifyEvents;
        EventDispatcher::GetInstance()->ReadInotifyEvents(inotifyEvents);
        if (inotifyEvents.size() > 0) {
            PushEventQueue(inotifyEvents);
        }
        mLastReadEventMicroSeconds = curMicroSeconds;
    }

    vector<Event*> feedbackEvents;
    BlockedEventManager::GetInstance()->GetFeedbackEvent(feedbackEvents);
    if (feedbackEvents.size() > 0) {
        PushEventQueue(feedbackEvents);
    }

    vector<Event*> pollingEvents;
    PollingEventQueue::GetInstance()->PopAllEvents(pollingEvents);
    if (pollingEvents.size() > 0) {
        PushEventQueue(pollingEvents);
    }

    std::vector<Event*> containerStoppedEvents;
    ConfigManager::GetInstance()->GetContainerStoppedEvents(containerStoppedEvents);
    if (containerStoppedEvents.size() > 0) {
        PushEventQueue(containerStoppedEvents);
    }

    mLastReadEventTime = ((int32_t)time(NULL));
}

void LogInput::FlowControl() {
    const static int32_t FLOW_CONTROL_SLEEP_MICROSECONDS = 20 * 1000; // 20ms
    const static int32_t MAX_SLEEP_COUNT = 50; // 1s
    static int32_t sleepCount = 10;
    static int32_t lastCheckTime = 0;
    int32_t i = 0;
    while (i < sleepCount) {
        if (mInteruptFlag)
            return;
        usleep(FLOW_CONTROL_SLEEP_MICROSECONDS);
        ++i;
        if (i % 5 == 0)
            TryReadEvents(true);
    }

    if (mInteruptFlag)
        return;
    int32_t curTime = time(NULL);
    if (curTime - lastCheckTime >= 1) {
        lastCheckTime = curTime;
        double cpuUsageLevel = LogtailMonitor::GetInstance()->GetRealtimeCpuLevel();
        if (cpuUsageLevel >= 1.5) {
            sleepCount += 5;
            if (sleepCount > MAX_SLEEP_COUNT)
                sleepCount = MAX_SLEEP_COUNT;
        } else if (cpuUsageLevel >= 1.2) {
            sleepCount += 2;
            if (sleepCount > MAX_SLEEP_COUNT)
                sleepCount = MAX_SLEEP_COUNT;
        } else if (cpuUsageLevel >= 1.0) {
            if (sleepCount < MAX_SLEEP_COUNT)
                ++sleepCount;
        } else if (cpuUsageLevel >= 0.9) {
        } else if (cpuUsageLevel >= 0.6) {
            if (sleepCount > 0)
                --sleepCount;
        } else if (cpuUsageLevel >= 0.3) {
            sleepCount -= 2;
            if (sleepCount < 0)
                sleepCount = 0;
        } else {
            sleepCount -= 5;
            if (sleepCount < 0)
                sleepCount = 0;
        }
        LOG_DEBUG(sLogger, ("cpuUsageLevel", cpuUsageLevel)("sleepCount", sleepCount));
    }
}

bool LogInput::ReadLocalEvents() {
    Json::Value localEventJson; // will contains the root value after parsing.
    ParseConfResult loadRes = ParseConfig(GetLocalEventDataFileName(), localEventJson);
    LOG_DEBUG(sLogger, ("load local events", GetLocalEventDataFileName())("result", loadRes));
    if (loadRes != CONFIG_OK || !localEventJson.isArray()) {
        return false;
    }
    // set discard old data flag, so that history data will not be dropped.
    BOOL_FLAG(ilogtail_discard_old_data) = false;
    LOG_INFO(sLogger, ("load local events", GetLocalEventDataFileName())("event count", localEventJson.size()));
    for (Json::ValueIterator iter = localEventJson.begin(); iter != localEventJson.end(); ++iter) {
        const Json::Value& eventItem = *iter;
        if (!eventItem.isObject()) {
            continue;
        }

        string source;
        string object;
        string configName;
        if (eventItem.isMember("dir") && eventItem["dir"].isString()) {
            source = eventItem["dir"].asString();
        } else {
            continue;
        }

        if (source.empty()) {
            continue;
        }

        // remove last '/' to makesure source not end with '/'
        if (source[source.size() - 1] == PATH_SEPARATOR[0]) {
            source.resize(source.size() - 1);
        }

        if (eventItem.isMember("name") && eventItem["name"].isString()) {
            object = eventItem["name"].asString();
        } else {
            continue;
        }

        if (object.size() == 0) {
            continue;
        }

        if (eventItem.isMember("config") && eventItem["config"].isString()) {
            configName = eventItem["config"].asString();
        } else {
            continue;
        }

        FileDiscoveryConfig discoveryConfig = FileServer::GetInstance()->GetFileDiscoveryConfig(configName);
        FileReaderConfig readerConfig = FileServer::GetInstance()->GetFileReaderConfig(configName);
        MultilineConfig multilineConfig = FileServer::GetInstance()->GetMultilineConfig(configName);
        uint32_t concurrency = FileServer::GetInstance()->GetExactlyOnceConcurrency(configName);
        if (!readerConfig.first) {
            LOG_WARNING(sLogger, ("can not find config", configName));
            continue;
        }

        HistoryFileEvent historyFileEvent;
        historyFileEvent.mDirName = source;
        historyFileEvent.mFileName = object;
        historyFileEvent.mConfigName = configName;
        historyFileEvent.mDiscoveryconfig = discoveryConfig;
        historyFileEvent.mReaderConfig = readerConfig;
        historyFileEvent.mMultilineConfig = multilineConfig;
        historyFileEvent.mEOConcurrency = concurrency;

        vector<string> objList;
        if (!GetAllFiles(source, object, objList)) {
            LOG_WARNING(sLogger, ("get all files", "failed"));
            continue;
        }

        LOG_INFO(
            sLogger,
            ("process local event, dir", source)("file name", object)("config", configName)(
                "project", readerConfig.second->GetProjectName())("logstore", readerConfig.second->GetLogstoreName()));
        AlarmManager::GetInstance()->SendAlarm(LOAD_LOCAL_EVENT_ALARM,
                                               string("process local event, dir:") + source + ", file name:" + object
                                                   + ", config:" + configName
                                                   + ", file count:" + ToString(objList.size()),
                                               readerConfig.second->GetProjectName(),
                                               readerConfig.second->GetLogstoreName(),
                                               readerConfig.second->GetRegion());

        HistoryFileImporter* importer = HistoryFileImporter::GetInstance();
        importer->PushEvent(historyFileEvent);
    }


    // after process event, clear the local file
    FILE* pFile = fopen(GetLocalEventDataFileName().c_str(), "w");
    if (pFile != NULL) {
        fclose(pFile);
    }
    return true;
}

void LogInput::ProcessEvent(EventDispatcher* dispatcher, Event* ev) {
    const string& source = ev->GetSource();
    const string& object = ev->GetObject();
    LOG_DEBUG(sLogger,
              ("process event, type", ev->GetTypeString())("dir", ev->GetSource())("filename", ev->GetObject())(
                  "config", ev->GetConfigName()));
    if (ev->IsTimeout())
        dispatcher->UnregisterAllDir(source);
    else {
        if (ev->IsDir()
            && (ev->IsMoveFrom() || (ev->IsContainerStopped() && BOOL_FLAG(force_close_file_on_container_stopped)))) {
            string path = source;
            if (object.size() > 0)
                path += PATH_SEPARATOR + object;
            dispatcher->UnregisterAllDir(path);
        } else if (ev->IsDir() && ev->IsContainerStopped()) {
            string path = source;
            if (object.size() > 0)
                path += PATH_SEPARATOR + object;
            dispatcher->StopAllDir(path);
        } else {
            EventHandler* handler = dispatcher->GetHandler(source.c_str());
            if (handler) {
                handler->Handle(*ev);
                dispatcher->PropagateTimeout(source.c_str());
            } else if (!ev->IsDeleted()) {
                LOG_DEBUG(sLogger,
                          ("LogInput handle event, find no handler", "register for it")("type", ev->GetType())(
                              "wd", ev->GetWd())("source", source)("object", object)("inode", ev->GetInode()));
                if (ConfigManager::GetInstance()->RegisterDirectory(source, object)) {
                    handler = dispatcher->GetHandler(source.c_str());
                    if (handler) {
                        handler->Handle(*ev);
                        dispatcher->PropagateTimeout(source.c_str());
                    }
                }
            }
        }
    }
    delete ev;
}

void LogInput::UpdateCriticalMetric(int32_t curTime) {
    LogtailMonitor::GetInstance()->UpdateMetric("last_read_event_time",
                                                GetTimeStamp(mLastReadEventTime, "%Y-%m-%d %H:%M:%S"));
    mLastRunTime->Set(mLastReadEventTime.load());

    LogtailMonitor::GetInstance()->UpdateMetric("event_tps",
                                                1.0 * mEventProcessCount / (curTime - mLastUpdateMetricTime));
    int32_t openFdTotal = GloablFileDescriptorManager::GetInstance()->GetOpenedFilePtrSize();
    LogtailMonitor::GetInstance()->UpdateMetric("open_fd", openFdTotal);
    LoongCollectorMonitor::GetInstance()->SetAgentOpenFdTotal(openFdTotal);
    size_t handlerCount = EventDispatcher::GetInstance()->GetHandlerCount();
    LogtailMonitor::GetInstance()->UpdateMetric("register_handler", handlerCount);
    mRegisterdHandlersTotal->Set(handlerCount);
    LogtailMonitor::GetInstance()->UpdateMetric("reader_count", CheckPointManager::Instance()->GetReaderCount());
    mActiveReadersTotal->Set(CheckPointManager::Instance()->GetReaderCount());
    LogtailMonitor::GetInstance()->UpdateMetric("multi_config", AppConfig::GetInstance()->IsAcceptMultiConfig());
    mEventProcessCount = 0;
}

void* LogInput::ProcessLoop() {
    LOG_INFO(sLogger, ("event handle daemon", "started"));
    EventDispatcher* dispatcher = EventDispatcher::GetInstance();
    dispatcher->StartTimeCount();
    int32_t prevTime = time(NULL);
    mLastReadEventTime = prevTime;
    int32_t curTime = prevTime;
    srand(prevTime);
    int32_t lastCheckDir = prevTime - rand() % 60;
    int32_t lastCheckSymbolicLink = prevTime - rand() % 60;
    time_t lastCheckHandlerTimeOut = prevTime - rand() % 60;
    int32_t lastDumpInotifyWatcherTime = prevTime - rand() % 60;
    int32_t lastForceClearFlag = prevTime - rand() % 60;
    int32_t lastClearConfigCache = prevTime - rand() % 60;
    mLastReadEventMicroSeconds = 0;
    mLastUpdateMetricTime = prevTime - rand() % 60;
    int32_t lastCheckBlockedTime = prevTime;
    int32_t lastReadLocalEventTime = prevTime;
    mEventProcessCount = 0;
    BlockedEventManager* pBlockedEventManager = BlockedEventManager::GetInstance();
    string path;
    while (true) {
        ReadLock lock(mAccessMainThreadRWL);
        TryReadEvents(false);
        Event* ev = PopEventQueue();
        if (ev != NULL) {
            ++mEventProcessCount;
            if (mIdleFlag)
                delete ev;
            else
                ProcessEvent(dispatcher, ev);
        } else {
            unique_lock<mutex> lock(mFeedbackMux);
            mFeedbackCV.wait_for(lock, chrono::microseconds(INT32_FLAG(log_input_thread_wait_interval)));
        }

        if (mIdleFlag)
            continue;

        curTime = time(NULL);


        if (curTime - lastCheckBlockedTime >= INT32_FLAG(check_block_event_interval)) {
            std::vector<Event*> pEventVec;
            pBlockedEventManager->GetTimeoutEvent(pEventVec, curTime);
            if (pEventVec.size() > 0) {
                PushEventQueue(pEventVec);
            }
            lastCheckBlockedTime = curTime;
        }

        if (curTime - lastReadLocalEventTime >= INT32_FLAG(read_local_event_interval)) {
            ReadLocalEvents();
            lastReadLocalEventTime = curTime;
        }
        if (curTime - mLastUpdateMetricTime >= 40) {
            UpdateCriticalMetric(curTime);
            mLastUpdateMetricTime = curTime;
        }
        if (curTime - lastForceClearFlag > 600 && GetForceClearFlag()) {
            lastForceClearFlag = curTime;
            prevTime = 0;
            lastCheckDir = 0;
            lastCheckHandlerTimeOut = 0;
            lastCheckSymbolicLink = 0;
        }

        if (curTime - prevTime >= INT32_FLAG(timeout_interval)) {
            dispatcher->HandleTimeout();
            prevTime = curTime;
        }

        if (curTime - lastCheckDir >= mCheckBaseDirInterval) {
            // do not need to clear file checkpoint, we will clear all checkpoint after DumpCheckPointToLocal
            // CheckPointManager::Instance()->CheckTimeoutCheckPoint();
            // check root watch dir
            ConfigManager::GetInstance()->RegisterHandlers();
            lastCheckDir = curTime;
        }

        if (curTime - lastCheckSymbolicLink >= mCheckSymbolicLinkInterval) {
            dispatcher->CheckSymbolicLink();
            lastCheckSymbolicLink = curTime;
        }

        if (curTime - lastCheckHandlerTimeOut >= INT32_FLAG(check_handler_timeout_interval)) {
            // call handle timeout
            dispatcher->ProcessHandlerTimeOut();
            lastCheckHandlerTimeOut = curTime;
        }

        if (curTime - lastDumpInotifyWatcherTime > INT32_FLAG(dump_inotify_watcher_interval)) {
            dispatcher->DumpInotifyWatcherDirs();
            lastDumpInotifyWatcherTime = curTime;
        }

        if (curTime - lastClearConfigCache > INT32_FLAG(clear_config_match_interval)) {
            ConfigManager::GetInstance()->ClearConfigMatchCache();
            lastClearConfigCache = curTime;
        }

        if (BOOL_FLAG(enable_full_drain_mode) && Application::GetInstance()->IsExiting()
            && EventDispatcher::GetInstance()->IsAllFileRead()) {
            break;
        }
    }

    mInteruptFlag = true;
    mStopCV.notify_one();

    if (!BOOL_FLAG(enable_full_drain_mode)) {
        LOG_WARNING(sLogger, ("LogInputThread", "Exit"));
    }
    return NULL;
}

void LogInput::PushEventQueue(std::vector<Event*>& eventVec) {
    for (std::vector<Event*>::iterator iter = eventVec.begin(); iter != eventVec.end(); ++iter) {
        string key;
        key.append((*iter)->GetSource())
            .append(">")
            .append((*iter)->GetObject())
            .append(">")
            .append(ToString((*iter)->GetDev()))
            .append(">")
            .append(ToString((*iter)->GetInode()))
            .append(">")
            .append((*iter)->GetConfigName());
        int64_t hashKey = HashSignatureString(key.c_str(), key.size());
        if ((*iter)->GetType() == EVENT_MODIFY) {
            if (mModifyEventSet.find(hashKey) != mModifyEventSet.end()) {
                delete (*iter);
                *iter = NULL;
                continue;
            } else
                mModifyEventSet.insert(hashKey);
        }
        mInotifyEventQueue.push(*iter);
        (*iter)->SetHashKey(hashKey);
    }
}

void LogInput::PushEventQueue(Event* ev) {
    string key;
    key.append(ev->GetSource())
        .append(">")
        .append(ev->GetObject())
        .append(">")
        .append(ToString(ev->GetDev()))
        .append(">")
        .append(ToString(ev->GetInode()))
        .append(">")
        .append(ev->GetConfigName());
    int64_t hashKey = HashSignatureString(key.c_str(), key.size());
    if (ev->GetType() == EVENT_MODIFY) {
        if (mModifyEventSet.find(hashKey) != mModifyEventSet.end()) {
            delete ev;
            return;
        } else
            mModifyEventSet.insert(hashKey);
    }
    ev->SetHashKey(hashKey);
    mInotifyEventQueue.push(ev);
}

Event* LogInput::PopEventQueue() {
    if (mInotifyEventQueue.size() > 0) {
        Event* ev = mInotifyEventQueue.front();
        mInotifyEventQueue.pop();
        if (ev->GetType() == EVENT_MODIFY)
            mModifyEventSet.erase(ev->GetHashKey());
        return ev;
    }
    return NULL;
}

#ifdef APSARA_UNIT_TEST_MAIN
void LogInput::CleanEnviroments() {
    mIdleFlag = true;
    usleep(100 * 1000);
    while (true) {
        Event* ev = PopEventQueue();
        if (ev == NULL)
            break;
        delete ev;
    }
    mModifyEventSet.clear();
}
#endif

} // namespace logtail
