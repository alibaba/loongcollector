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

#include "LogtailInsightDispatcher.h"
#include "logger/Logger.h"
#include "common/Flags.h"
#include "common/TimeUtil.h"
#include "common/LogFileCollectOffsetIndicator.h"

DEFINE_FLAG_INT32(default_wait_second, "default wait time for non-block fd, milliseconds", 50);

using namespace std;

namespace logtail {

typedef struct MessageInsightHdr {
    uint32_t type;
    uint32_t len;
} MessageInsightHdr;


static int SendToFDWithWait(int fd, const char* data, int size, int waitMs) {
    int sendOffset = 0;
    uint64_t startTimeMs = GetCurrentTimeInMilliSeconds();
    while (sendOffset < size) {
        int sendRst = send(fd, data + sendOffset, size - sendOffset, MSG_DONTWAIT);
        if (sendRst > 0) {
            sendOffset += size;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_DEBUG(sLogger, ("send data to fd error, fd", fd)("error", errno));
                return -1;
            }
            if (GetCurrentTimeInMilliSeconds() - startTimeMs > (uint64_t)waitMs) {
                LOG_DEBUG(sLogger, ("send data to fd timeout, fd", fd)("error", errno));
                return -1;
            }
            usleep(10 * 1000);
        }
    }
    return size;
}

static void SendErrorToFD(int fd, const std::string& errorMessage) {
    MessageInsightHdr header;
    header.type = INSIGHT_CMD_TYPE;
    sls_logs::Log outLog;
    outLog.set_time(time(NULL));
    sls_logs::Log_Content* content = outLog.add_contents();
    content->set_key("error");
    content->set_value(errorMessage);
    std::string data = outLog.SerializeAsString();

    header.len = data.size();
    if (SendToFDWithWait(fd, (const char*)&header, sizeof(header), INT32_FLAG(default_wait_second)) != sizeof(header)) {
        return;
    }
    SendToFDWithWait(fd, data.c_str(), data.size(), 50);
}

/**
 * @return -1 for invalid cmd params, -2 for send error, positive nums which > 0 for sent bytes
 */
int32_t LogtailInsightDispatcher::ExecuteCommand(int fd, const char* cmdBuf, int cmdSize) {
    sls_logs::Log cmd;
    if (!cmd.ParseFromArray((const void*)cmdBuf, cmdSize)) {
        LOG_ERROR(sLogger, ("parse command error", cmdSize));
        SendErrorToFD(fd, "parse command error");
        return -1;
    }
    if (cmd.contents_size() < 1) {
        LOG_ERROR(sLogger, ("invalid command", "content size < 1"));
        SendErrorToFD(fd, "invalid command");
        return -1;
    }
    const std::string cmdType = cmd.contents(0).value();
    MessageInsightHdr header;
    header.type = INSIGHT_CMD_TYPE;
    if (cmdType == "file") {
        if (cmd.contents_size() < 2) {
            LOG_ERROR(sLogger, ("invalid command file", "content size < 2"));
            SendErrorToFD(fd, "invalid command : file");
            return -1;
        }
        std::string filename = cmd.contents(1).value();
        bool detailFlag = false;
        if (cmd.contents_size() >= 3 && cmd.contents(2).value() == "detail") {
            detailFlag = true;
        }

        sls_logs::LogGroup logGroup;
        if (filename == "all") {
            bool finishedFlag = false;
            if (detailFlag) {
                finishedFlag = GetAllFileProgress(&logGroup);
            } else {
                finishedFlag = GetAllFileProgress(NULL);

                sls_logs::Log* log = logGroup.add_logs();
                log->set_time(time(NULL));
                sls_logs::Log_Content* content = log->add_contents();
                content->set_key("isFinished");
                content->set_value(ToString(finishedFlag));
            }
        } else {
            int result = -1;
            if (detailFlag) {
                result = GetFileProgress(filename, &logGroup);
            } else {
                result = GetFileProgress(filename, NULL);

                sls_logs::Log* log = logGroup.add_logs();
                log->set_time(time(NULL));
                sls_logs::Log_Content* content = log->add_contents();
                content->set_key("isFinished");
                if (result == -1) {
                    content->set_value("invalid filename");
                } else {
                    content->set_value(result == 0 ? ToString(false) : ToString(true));
                }
            }
        }

        std::string data = logGroup.SerializeAsString();
        header.len = data.size();
        int sendResult
            = SendToFDWithWait(fd, (const char*)&header, sizeof(header), INT32_FLAG(default_wait_second) * 2);
        if (sendResult != sizeof(header)) {
            return -2;
        }
        sendResult = SendToFDWithWait(fd, data.c_str(), data.size(), INT32_FLAG(default_wait_second));
        if (sendResult != (int)data.size()) {
            return -2;
        }
        return sendResult;
    } else {
        LOG_ERROR(sLogger, ("invalid command type", cmdType));
        SendErrorToFD(fd, string("invalid command type : ") + cmdType);
        return -1;
    }
}

bool LogtailInsightDispatcher::GetAllFileProgress(sls_logs::LogGroup* logGroup) {
    LogFileCollectProgressMap m = LogFileCollectOffsetIndicator::GetInstance()->GetAllFileProgress();
    bool finishedFlag = true;
    for (LogFileCollectProgressMap::iterator iter = m.begin(); iter != m.end(); ++iter) {
        const LogFileInfo& info = iter->first;
        LogFileCollectProgress progress = iter->second;

        struct stat buf;
        if (stat(info.mFilename.c_str(), &buf)) {
            LOG_ERROR(sLogger, ("invalid file", info.mFilename));
            continue;
        }
        progress.mFileSize = buf.st_size;

        if (!progress.IsFinished()) {
            finishedFlag = false;
        }
        if (logGroup != NULL) {
            BuildLogGroup(logGroup, info, progress);
        }
    }

    return finishedFlag;
}

// return value: -1 for invalid file, 0 for not finished, 1 for finished
int LogtailInsightDispatcher::GetFileProgress(const std::string& filename, sls_logs::LogGroup* logGroup) {
    LogFileCollectProgressMap m;
    int result = -1;
    if (LogFileCollectOffsetIndicator::GetInstance()->GetFileProgress(filename, m)) {
        result = 1;
        for (LogFileCollectProgressMap::iterator iter = m.begin(); iter != m.end(); ++iter) {
            const LogFileInfo& info = iter->first;
            LogFileCollectProgress progress = iter->second;

            struct stat buf;
            if (stat(info.mFilename.c_str(), &buf)) {
                LOG_ERROR(sLogger, ("invalid file", info.mFilename));
                continue;
            }
            progress.mFileSize = buf.st_size;

            if (!progress.IsFinished()) {
                result = 0;
            }
            if (logGroup != NULL) {
                BuildLogGroup(logGroup, info, progress);
            }
        }
    }
    return result;
}

void LogtailInsightDispatcher::BuildLogGroup(sls_logs::LogGroup* logGroup,
                                             const LogFileInfo& info,
                                             const LogFileCollectProgress& progress) {
    sls_logs::Log* log = logGroup->add_logs();
    log->set_time(time(NULL));

    sls_logs::Log_Content* content = log->add_contents();
    content->set_key("project");
    content->set_value(info.mProjectName);

    content = log->add_contents();
    content->set_key("logstore");
    content->set_value(info.mLogstore);

    content = log->add_contents();
    content->set_key("configName");
    content->set_value(info.mConfigName);

    content = log->add_contents();
    content->set_key("filename");
    content->set_value(info.mFilename);

    content = log->add_contents();
    content->set_key("offset");
    content->set_value(ToString(progress.mSendOffset));

    content = log->add_contents();
    content->set_key("filePos");
    content->set_value(ToString(progress.mFileLastPos));

    content = log->add_contents();
    content->set_key("readPos");
    content->set_value(ToString(progress.mFileReadPos));

    content = log->add_contents();
    content->set_key("size");
    content->set_value(ToString(progress.mFileSize));

    content = log->add_contents();
    content->set_key("isFinished");
    content->set_value(ToString(progress.IsFinished()));
}

} // namespace logtail
