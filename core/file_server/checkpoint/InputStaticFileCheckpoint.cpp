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

#include "common/JsonUtil.h"
#include "common/ParamExtractor.h"
#include "common/TimeUtil.h"
#include "logger/Logger.h"
#include "monitor/TaskStatusManager.h"
#include "monitor/profile_sender/ProfileSender.h"
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
                                                     vector<FileCheckpoint>&& fileCpts)
    : mConfigName(configName), mInputIdx(idx), mFileCheckpoints(std::move(fileCpts)) {
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
        LOG_INFO(sLogger, ("all files read done, config", mConfigName)("input idx", mInputIdx));
    }
    return true;
}

bool InputStaticFileCheckpoint::GetCurrentFileFingerprint(FileFingerprint* cpt) {
    if (!cpt) {
        // should not happen
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
    root["status"] = StaticFileReadingStatusToString(mStatus);
    if (mStatus == StaticFileReadingStatus::RUNNING) {
        root["current_file_index"] = mCurrentFileIndex;
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
        cpt.mFilePath = filepath;

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

bool InputStaticFileCheckpoint::SerializeToLogEvents() const {
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
    for (const auto& cpt : mFileCheckpoints) {
        LogEvent* logEvent = TaskStatusManager::GetInstance()->AddTaskStatus(TASK_TYPE_STATIC_FILE, region);
        if (!logEvent) {
            continue;
        }
        logEvent->SetTimestamp(timestamp);
        // task info
        logEvent->SetContent("config_name", mConfigName);
        logEvent->SetContent("input_idx", ToString(mInputIdx));
        logEvent->SetContent("file_count", ToString(mFileCheckpoints.size()));
        logEvent->SetContent("status", StaticFileReadingStatusToString(mStatus));
        if (mStatus == StaticFileReadingStatus::RUNNING) {
            logEvent->SetContent("current_file_index", ToString(mCurrentFileIndex));
        }
        // file info
        logEvent->SetContent("filepath", cpt.mFilePath.string());
        logEvent->SetContent("status", FileStatusToString(cpt.mStatus));
        switch (cpt.mStatus) {
            case FileStatus::WAITING:
                logEvent->SetContent("dev", ToString(cpt.mDevInode.dev));
                logEvent->SetContent("inode", ToString(cpt.mDevInode.inode));
                logEvent->SetContent("sig_hash", ToString(cpt.mSignatureHash));
                logEvent->SetContent("sig_size", ToString(cpt.mSignatureSize));
                break;
            case FileStatus::READING:
                logEvent->SetContent("dev", ToString(cpt.mDevInode.dev));
                logEvent->SetContent("inode", ToString(cpt.mDevInode.inode));
                logEvent->SetContent("sig_hash", ToString(cpt.mSignatureHash));
                logEvent->SetContent("sig_size", ToString(cpt.mSignatureSize));
                logEvent->SetContent("size", ToString(cpt.mSize));
                logEvent->SetContent("offset", ToString(cpt.mOffset));
                logEvent->SetContent("start_time", ToString(cpt.mStartTime));
                logEvent->SetContent("last_read_time", ToString(cpt.mLastUpdateTime));
                break;
            case FileStatus::FINISHED:
                logEvent->SetContent("size", ToString(cpt.mSize));
                logEvent->SetContent("start_time", ToString(cpt.mStartTime));
                logEvent->SetContent("finish_time", ToString(cpt.mLastUpdateTime));
                break;
            case FileStatus::ABORT:
                logEvent->SetContent("abort_time", ToString(cpt.mLastUpdateTime));
                break;
            default:
                // should not happen
                break;
        }
    }
    return true;
}

} // namespace logtail
