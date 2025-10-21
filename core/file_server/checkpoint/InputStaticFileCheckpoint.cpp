// Copyright 2025 iLogtail Authors
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

#include "file_server/checkpoint/InputStaticFileCheckpoint.h"

#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/SelfMonitorServer.h"
#include "monitor/profile_sender/ProfileSender.h"
#include "monitor/task_status_constants/TaskStatusConstants.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "provider/Provider.h"

using namespace std;

namespace logtail {

static const string& StaticFileReadingStatusToString(StaticFileReadingStatus status) {
    switch (status) {
        case StaticFileReadingStatus::RUNNING: {
            static const string kRunningStr = "running";
            return kRunningStr;
        }
        case StaticFileReadingStatus::FINISHED: {
            static const string kFinishedStr = "finished";
            return kFinishedStr;
        }
        case StaticFileReadingStatus::ABORT: {
            static const string kAbortStr = "abort";
            return kAbortStr;
        }
        default: {
            // should not happen
            static const string kUnknownStr = "unknown";
            return kUnknownStr;
        }
    }
}

static StaticFileReadingStatus GetStaticFileReadingStatusFromString(const string& statusStr) {
    if (statusStr == "running") {
        return StaticFileReadingStatus::RUNNING;
    } else if (statusStr == "finished") {
        return StaticFileReadingStatus::FINISHED;
    } else if (statusStr == "abort") {
        return StaticFileReadingStatus::ABORT;
    } else {
        return StaticFileReadingStatus::UNKNOWN;
    }
}

InputStaticFileCheckpoint::InputStaticFileCheckpoint(const string& configName,
                                                     size_t idx,
                                                     //  uint64_t hash,
                                                     vector<FileCheckpoint>&& fileCpts,
                                                     uint32_t startTime,
                                                     uint32_t expireTime)
    : mConfigName(configName),
      mInputIdx(idx),
      mFileCheckpoints(std::move(fileCpts)),
      mStartTime(startTime),
      mExpireTime(expireTime) {
}

bool InputStaticFileCheckpoint::UpdateCurrentFileCheckpoint(uint64_t offset, uint64_t size, bool& needDump) {
    if (mCurrentFileIndex >= mFileCheckpoints.size()) {
        // should not happen
        return false;
    }
    needDump = false;
    auto& fileCpt = mFileCheckpoints[mCurrentFileIndex];
    switch (fileCpt.mStatus) {
        case FileStatus::WAITING:
            fileCpt.mStatus = FileStatus::READING;
            fileCpt.mStartTime = time(nullptr);
            needDump = true;
            LOG_INFO(sLogger,
                     ("begin to read file, config", mConfigName)("input idx", mInputIdx)(
                         "current file idx", mCurrentFileIndex)("filepath", fileCpt.mFilePath.string())(
                         "device", fileCpt.mDevInode.dev)("inode", fileCpt.mDevInode.inode)(
                         "signature hash", fileCpt.mSignatureHash)("signature size", fileCpt.mSignatureSize));
        case FileStatus::READING:
            fileCpt.mOffset = offset;
            fileCpt.mSize = size;
            fileCpt.mLastUpdateTime = time(nullptr);
            if (offset == size) {
                fileCpt.mStatus = FileStatus::FINISHED;
                needDump = true;
                LOG_INFO(sLogger,
                         ("file read done, config", mConfigName)("input idx", mInputIdx)(
                             "current file idx", mCurrentFileIndex)("filepath", fileCpt.mFilePath.string())(
                             "device", fileCpt.mDevInode.dev)("inode", fileCpt.mDevInode.inode)(
                             "signature hash", fileCpt.mSignatureHash)("signature size", fileCpt.mSignatureSize)("size",
                                                                                                                 size));
                if (++mCurrentFileIndex == mFileCheckpoints.size()) {
                    mStatus = StaticFileReadingStatus::FINISHED;
                    mFinishTime = time(nullptr);
                    LOG_INFO(sLogger, ("all files read done, config", mConfigName)("input idx", mInputIdx));
                }
            }
            return true;
        default:
            // should not happen
            return false;
    }
}

bool InputStaticFileCheckpoint::InvalidateCurrentFileCheckpoint() {
    if (mCurrentFileIndex >= mFileCheckpoints.size()) {
        // should not happen
        return false;
    }
    auto& fileCpt = mFileCheckpoints[mCurrentFileIndex];
    if (fileCpt.mStatus == FileStatus::ABORT || fileCpt.mStatus == FileStatus::FINISHED) {
        // should not happen
        return false;
    }
    fileCpt.mStatus = FileStatus::ABORT;
    fileCpt.mLastUpdateTime = time(nullptr);
    LOG_WARNING(sLogger,
                ("file read abort, config", mConfigName)("input idx", mInputIdx)("current file idx", mCurrentFileIndex)(
                    "filepath", fileCpt.mFilePath.string())("device", fileCpt.mDevInode.dev)(
                    "inode", fileCpt.mDevInode.inode)("signature hash", fileCpt.mSignatureHash)(
                    "signature size", fileCpt.mSignatureSize)("read offset", fileCpt.mOffset));
    if (++mCurrentFileIndex == mFileCheckpoints.size()) {
        mStatus = StaticFileReadingStatus::FINISHED;
        mFinishTime = time(nullptr);
        LOG_INFO(sLogger, ("all files read done, config", mConfigName)("input idx", mInputIdx));
    }
    return true;
}

bool InputStaticFileCheckpoint::GetCurrentFileFingerprint(FileFingerprint* cpt) {
    if (!cpt) {
        // should not happen
        return false;
    }
    if (mStatus == StaticFileReadingStatus::FINISHED || mStatus == StaticFileReadingStatus::ABORT) {
        return false;
    }
    if (mCurrentFileIndex >= mFileCheckpoints.size()) {
        return false;
    }
    auto& fileCpt = mFileCheckpoints[mCurrentFileIndex];
    cpt->mFilePath = fileCpt.mFilePath;
    cpt->mDevInode = fileCpt.mDevInode;
    cpt->mSignatureHash = fileCpt.mSignatureHash;
    cpt->mSignatureSize = fileCpt.mSignatureSize;
    return true;
}

void InputStaticFileCheckpoint::SetAbort() {
    if (mStatus == StaticFileReadingStatus::RUNNING) {
        mStatus = StaticFileReadingStatus::ABORT;
        mFinishTime = time(nullptr);
        LOG_WARNING(sLogger, ("file read abort, config", mConfigName)("input idx", mInputIdx));
    }
}

bool InputStaticFileCheckpoint::Serialize(string* res) const {
    if (!res) {
        // should not happen
        return false;
    }
    Json::Value root;
    root["config_name"] = mConfigName;
    root["input_index"] = mInputIdx;
    root["file_count"] = mFileCheckpoints.size(); // for integrity check
    root["start_time"] = mStartTime;
    root["expire_time"] = mExpireTime;
    root["status"] = StaticFileReadingStatusToString(mStatus);
    if (mStatus == StaticFileReadingStatus::RUNNING) {
        root["current_file_index"] = mCurrentFileIndex;
    }
    if (mStatus == StaticFileReadingStatus::FINISHED || mStatus == StaticFileReadingStatus::ABORT) {
        root["finish_time"] = mFinishTime;
    }
    root["files"] = Json::arrayValue;
    auto& files = root["files"];
    for (const auto& cpt : mFileCheckpoints) {
        files.append(Json::objectValue);
        auto& file = files[files.size() - 1];
        file["filepath"] = cpt.mFilePath.string();
        file["status"] = FileStatusToString(cpt.mStatus);
        switch (cpt.mStatus) {
            case FileStatus::WAITING:
                file["dev"] = cpt.mDevInode.dev;
                file["inode"] = cpt.mDevInode.inode;
                file["sig_hash"] = cpt.mSignatureHash;
                file["sig_size"] = cpt.mSignatureSize;
                break;
            case FileStatus::READING:
                file["dev"] = cpt.mDevInode.dev;
                file["inode"] = cpt.mDevInode.inode;
                file["sig_hash"] = cpt.mSignatureHash;
                file["sig_size"] = cpt.mSignatureSize;
                file["size"] = cpt.mSize;
                file["offset"] = cpt.mOffset;
                file["start_time"] = cpt.mStartTime;
                file["last_read_time"] = cpt.mLastUpdateTime;
                break;
            case FileStatus::FINISHED:
                file["size"] = cpt.mSize;
                file["start_time"] = cpt.mStartTime;
                file["finish_time"] = cpt.mLastUpdateTime;
                break;
            case FileStatus::ABORT:
                file["abort_time"] = cpt.mLastUpdateTime;
                break;
            default:
                // should not happen
                break;
        }
    }
    *res = root.toStyledString();
    return true;
}

bool InputStaticFileCheckpoint::Deserialize(const string& str, string* errMsg) {
    if (!errMsg) {
        // should not happen
        return false;
    }
    Json::Value res;
    if (!ParseJsonTable(str, res, *errMsg)) {
        return false;
    }
    if (!res.isObject()) {
        return false;
    }
    if (!GetMandatoryStringParam(res, "config_name", mConfigName, *errMsg)) {
        return false;
    }
    if (!GetMandatoryUInt64Param(res, "input_index", mInputIdx, *errMsg)) {
        return false;
    }
    if (!GetOptionalUIntParam(res, "start_time", mStartTime, *errMsg)) {
        return false;
    }
    if (!GetOptionalUIntParam(res, "expire_time", mExpireTime, *errMsg)) {
        return false;
    }
    string statusStr;
    if (!GetMandatoryStringParam(res, "status", statusStr, *errMsg)) {
        return false;
    }
    mStatus = GetStaticFileReadingStatusFromString(statusStr);
    if (mStatus == StaticFileReadingStatus::UNKNOWN) {
        *errMsg = "mandatory string param status is not valid";
        return false;
    }
    if (mStatus == StaticFileReadingStatus::RUNNING) {
        if (!GetMandatoryUInt64Param(res, "current_file_index", mCurrentFileIndex, *errMsg)) {
            return false;
        }
    }
    if (mStatus == StaticFileReadingStatus::FINISHED || mStatus == StaticFileReadingStatus::ABORT) {
        GetOptionalUIntParam(res, "finish_time", mFinishTime, *errMsg);
    }

    uint32_t fileCnt = 0;
    if (!GetMandatoryUIntParam(res, "file_count", fileCnt, *errMsg)) {
        return false;
    }
    const char* key = "files";
    auto it = res.find(key, key + strlen(key));
    if (!it) {
        *errMsg = "mandatory param files is missing";
        return false;
    }
    if (!it->isArray()) {
        *errMsg = "mandatory param files is not of type array";
        return false;
    }
    if (fileCnt != it->size()) {
        *errMsg = "file count mismatch";
        return false;
    }
    for (Json::Value::ArrayIndex i = 0; i < it->size(); ++i) {
        const Json::Value& fileCpt = (*it)[i];
        string outerKey = "files[" + ToString(i) + "]";
        if (!fileCpt.isObject()) {
            *errMsg = "mandatory param " + outerKey + " is not of type object";
            return false;
        }
        FileCheckpoint cpt;
        string filepath;
        if (!GetMandatoryStringParam(fileCpt, outerKey + ".filepath", filepath, *errMsg)) {
            return false;
        }
        cpt.mFilePath = ConvertAndNormalizeNativePath(filepath);

        string statusStr;
        if (!GetMandatoryStringParam(fileCpt, outerKey + ".status", statusStr, *errMsg)) {
            return false;
        }
        cpt.mStatus = GetFileStatusFromString(statusStr);

        switch (cpt.mStatus) {
            case FileStatus::WAITING:
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".dev", cpt.mDevInode.dev, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".inode", cpt.mDevInode.inode, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".sig_hash", cpt.mSignatureHash, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUIntParam(fileCpt, outerKey + ".sig_size", cpt.mSignatureSize, *errMsg)) {
                    return false;
                }
                break;
            case FileStatus::READING:
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".dev", cpt.mDevInode.dev, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".inode", cpt.mDevInode.inode, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".sig_hash", cpt.mSignatureHash, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUIntParam(fileCpt, outerKey + ".sig_size", cpt.mSignatureSize, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".size", cpt.mSize, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".offset", cpt.mOffset, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryIntParam(fileCpt, outerKey + ".start_time", cpt.mStartTime, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryIntParam(fileCpt, outerKey + ".last_read_time", cpt.mLastUpdateTime, *errMsg)) {
                    return false;
                }
                break;
            case FileStatus::FINISHED:
                if (!GetMandatoryUInt64Param(fileCpt, outerKey + ".size", cpt.mSize, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryIntParam(fileCpt, outerKey + ".start_time", cpt.mStartTime, *errMsg)) {
                    return false;
                }
                if (!GetMandatoryIntParam(fileCpt, outerKey + ".finish_time", cpt.mLastUpdateTime, *errMsg)) {
                    return false;
                }
                break;
            case FileStatus::ABORT:
                if (!GetMandatoryIntParam(fileCpt, outerKey + ".abort_time", cpt.mLastUpdateTime, *errMsg)) {
                    return false;
                }
                break;
            default:
                *errMsg = "mandatory string param " + outerKey + ".status is not valid";
                return false;
        }
        mFileCheckpoints.emplace_back(std::move(cpt));
    }
    return true;
}

string buildFileInfoJson(const FileCheckpoint& cpt) {
    Json::Value fileInfo;
    fileInfo["filepath"] = cpt.mFilePath.string();
    fileInfo["status"] = FileStatusToString(cpt.mStatus);

    switch (cpt.mStatus) {
        case FileStatus::WAITING:
            fileInfo["dev"] = cpt.mDevInode.dev;
            fileInfo["inode"] = cpt.mDevInode.inode;
            fileInfo["sig_hash"] = cpt.mSignatureHash;
            fileInfo["sig_size"] = cpt.mSignatureSize;
            break;
        case FileStatus::READING:
            fileInfo["dev"] = cpt.mDevInode.dev;
            fileInfo["inode"] = cpt.mDevInode.inode;
            fileInfo["sig_hash"] = cpt.mSignatureHash;
            fileInfo["sig_size"] = cpt.mSignatureSize;
            fileInfo["size"] = cpt.mSize;
            fileInfo["offset"] = cpt.mOffset;
            fileInfo["start_time"] = cpt.mStartTime;
            fileInfo["last_read_time"] = cpt.mLastUpdateTime;
            break;
        case FileStatus::FINISHED:
            fileInfo["size"] = cpt.mSize;
            fileInfo["start_time"] = cpt.mStartTime;
            fileInfo["finish_time"] = cpt.mLastUpdateTime;
            break;
        case FileStatus::ABORT:
            fileInfo["abort_time"] = cpt.mLastUpdateTime;
            break;
        default:
            // should not happen
            break;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = ""; // 不缩进，生成紧凑的JSON
    return Json::writeString(builder, fileInfo);
}

bool InputStaticFileCheckpoint::SerializeToLogEvents() const {
    // 如果 mLastSentIndex 已经到达或超过文件总数，说明所有文件都已处理完毕
    if (mLastSentIndex >= mFileCheckpoints.size()) {
        return true;
    }

    size_t startIndex = mLastSentIndex;

    string project;
    string region;
    size_t prefixPos = mConfigName.find("##1.0##");
    if (prefixPos != string::npos) {
        size_t dollarPos = mConfigName.find('$', prefixPos + 7);
        if (dollarPos != string::npos) {
            project = mConfigName.substr(prefixPos + 7, dollarPos - prefixPos - 7);
            region = FlusherSLS::GetProjectRegion(project);
        }
    }
    if (region.empty()) {
        region = ProfileSender::GetInstance()->GetDefaultProfileRegion();
    }

    auto now = GetCurrentLogtailTime();
    auto timestamp = AppConfig::GetInstance()->EnableLogTimeAutoAdjust() ? now.tv_sec + GetTimeDelta() : now.tv_sec;

    // 从上次发送位置开始发送，避免重复处理已发送的文件
    for (size_t i = startIndex; i < mFileCheckpoints.size(); ++i) {
        const auto& cpt = mFileCheckpoints[i];

        LogEvent* logEvent = SelfMonitorServer::GetInstance()->AddTaskStatus(region);
        if (!logEvent) {
            continue;
        }
        logEvent->SetTimestamp(timestamp);
        // task info
        logEvent->SetContent(TASK_CONTENT_KEY_TASK_TYPE, TASK_TYPE_STATIC_FILE);
        if (!project.empty()) {
            logEvent->SetContent(TASK_CONTENT_KEY_PROJECT, project);
        }
        logEvent->SetContent(TASK_CONTENT_KEY_CONFIG_NAME, mConfigName);
        logEvent->SetContent(TASK_CONTENT_KEY_STATUS, StaticFileReadingStatusToString(mStatus));
        logEvent->SetContent(TASK_CONTENT_KEY_START_TIME, ToString(mStartTime));
        logEvent->SetContent(TASK_CONTENT_KEY_EXPIRE_TIME, ToString(mExpireTime));
        logEvent->SetContent("file_count", ToString(mFileCheckpoints.size()));
        logEvent->SetContent("input_idx", ToString(mInputIdx));
        if (mStatus == StaticFileReadingStatus::RUNNING) {
            logEvent->SetContent("current_file_index", ToString(mCurrentFileIndex));
        }
        if (mStatus == StaticFileReadingStatus::FINISHED || mStatus == StaticFileReadingStatus::ABORT) {
            logEvent->SetContent("finish_time", ToString(mFinishTime));
        }
        logEvent->SetContent("file_info", buildFileInfoJson(cpt));

        if (cpt.mStatus == FileStatus::FINISHED || cpt.mStatus == FileStatus::ABORT) {
            mLastSentIndex = i + 1;
        }
    }
    return true;
}

} // namespace logtail
