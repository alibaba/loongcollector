/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <json/json.h>
#include "AdhocJobCheckpoint.h"
#include "common/FileSystemUtil.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"

namespace logtail {

AdhocJobCheckpoint::AdhocJobCheckpoint(const std::string& jobName) {
    mAdhocJobName = jobName;
    mFileCount = 0;
    mCurrentFileIndex = 0;
    mLastDumpTime = 0;
}

void AdhocJobCheckpoint::AddFileCheckpoint(AdhocFileCheckpointPtr fileCheckpoint) {
    mAdhocFileCheckpointList.push_back(fileCheckpoint);
    mFileCount++;
}

AdhocFileCheckpointPtr AdhocJobCheckpoint::GetFileCheckpoint(AdhocFileKey* fileKey) {
    AdhocFileCheckpointPtr fileCheckpoint = nullptr;
    if (CheckFileConsistence(fileKey)) {
        fileCheckpoint = mAdhocFileCheckpointList[mCurrentFileIndex];
    }
    return fileCheckpoint;
}

bool AdhocJobCheckpoint::UpdateFileCheckpoint(AdhocFileKey* fileKey, AdhocFileCheckpointPtr fileCheckpoint) {
    bool dumpFlag = false;
    bool indexChangeFlag = false;
    if (!CheckFileConsistence(fileKey) || fileCheckpoint->mOffset == -1) {
        fileCheckpoint->mStatus = STATUS_LOST;
        indexChangeFlag = true;
        dumpFlag = true;
        LOG_WARNING(sLogger,
                    ("Adhoc file lost, job name",
                     mAdhocJobName)("file name", fileCheckpoint->mFileName)("file size", fileCheckpoint->mSize)(
                        "dev", fileCheckpoint->mDevInode.dev)("inode", fileCheckpoint->mDevInode.inode));
    } else {
        if (STATUS_WAITING == fileCheckpoint->mStatus) {
            fileCheckpoint->mStatus = STATUS_LOADING;
            fileCheckpoint->mStartTime = time(NULL);
            dumpFlag = true;
            LOG_INFO(sLogger,
                     ("Adhoc file start reading, job name",
                      mAdhocJobName)("file name", fileCheckpoint->mFileName)("file size", fileCheckpoint->mSize)(
                         "dev", fileCheckpoint->mDevInode.dev)("inode", fileCheckpoint->mDevInode.inode));
        }
        if (fileCheckpoint->mOffset == fileCheckpoint->mSize) {
            fileCheckpoint->mStatus = STATUS_FINISHED;
            indexChangeFlag = true;
            dumpFlag = true;
            LOG_INFO(sLogger,
                     ("Adhoc file finish reading, job name",
                      mAdhocJobName)("file name", fileCheckpoint->mFileName)("file size", fileCheckpoint->mSize)(
                         "dev", fileCheckpoint->mDevInode.dev)("inode", fileCheckpoint->mDevInode.inode));
        }
    }
    fileCheckpoint->mLastUpdateTime = time(NULL);

    if (indexChangeFlag) {
        mCurrentFileIndex++;
    }
    return dumpFlag;
}

bool AdhocJobCheckpoint::Load(const std::string& path) {
    if (CheckExistance(path)) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            LOG_ERROR(sLogger, ("open adhoc check point file error when load, file path", path));
            AlarmManager::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "open check point file failed");
            return false;
        }

        Json::Value root;
        ifs >> root;
        ifs.close();

        if (root.isMember("job_name")) {
            mAdhocJobName = root["job_name"].asString();
        } else {
            LOG_ERROR(sLogger, ("Adhoc check point file does not have field \"job_name\", file path", path));
            return false;
        }
        mFileCount = root.get("job_file_count", 0).asInt();
        mCurrentFileIndex = root.get("job_current_file_index", 0).asInt();
        if (root.isMember("job_files")) {
            Json::Value files = root["job_files"];
            for (Json::Value file : files) {
                AdhocFileCheckpointPtr fileCheckpoint = std::make_shared<AdhocFileCheckpoint>();
                fileCheckpoint->mFileName = file.get("file_name", "").asString();
                fileCheckpoint->mStatus = GetStatusFromString(file.get("status", "").asString());
                fileCheckpoint->mJobName = mAdhocJobName;

                switch (fileCheckpoint->mStatus) {
                    case STATUS_WAITING:
                        fileCheckpoint->mDevInode.dev = file.get("dev", 0).asUInt64();
                        fileCheckpoint->mDevInode.inode = file.get("inode", 0).asUInt64();
                        fileCheckpoint->mSize = file.get("size", 0).asInt64();
                        break;
                    case STATUS_LOADING:
                        fileCheckpoint->mDevInode.dev = file.get("dev", 0).asUInt64();
                        fileCheckpoint->mDevInode.inode = file.get("inode", 0).asUInt64();
                        fileCheckpoint->mOffset = file.get("offset", 0).asInt64();
                        fileCheckpoint->mSize = file.get("size", 0).asInt64();
                        fileCheckpoint->mSignatureHash = file.get("sig_hash", 0).asUInt64();
                        fileCheckpoint->mSignatureSize = file.get("sig_size", 0).asUInt();
                        fileCheckpoint->mStartTime = file.get("start_time", 0).asInt();
                        fileCheckpoint->mLastUpdateTime = file.get("update_time", 0).asInt();
                        fileCheckpoint->mRealFileName = file.get("real_file_name", "").asString();
                        break;
                    case STATUS_FINISHED:
                        fileCheckpoint->mSize = file.get("size", 0).asInt64();
                        fileCheckpoint->mStartTime = file.get("start_time", 0).asInt();
                        fileCheckpoint->mLastUpdateTime = file.get("update_time", 0).asInt();
                        fileCheckpoint->mRealFileName = file.get("real_file_name", "").asString();
                        break;
                    case STATUS_LOST:
                        fileCheckpoint->mLastUpdateTime = file.get("update_time", 0).asInt();
                        break;
                }
                mAdhocFileCheckpointList.push_back(fileCheckpoint);
            }
        } else if (0 != mFileCount) {
            LOG_ERROR(sLogger, ("Adhoc check point file lost field \"job_files\", file path", path));
            return false;
        }
        return true;
    } else {
        return false;
    }
}

void AdhocJobCheckpoint::Dump(const std::string& path, bool isAutoDump) {
    if (isAutoDump && (time(NULL) - mLastDumpTime < 5)) {
        return;
    }
    mLastDumpTime = time(NULL);

    if (!Mkdirs(ParentPath(path))) {
        LOG_ERROR(sLogger, ("open adhoc check point file dir error when dump, file path", path));
        AlarmManager::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "open adhoc check point file dir failed");
        return;
    }

    Json::Value root;
    root["job_name"] = mAdhocJobName;
    root["job_file_count"] = mFileCount;
    root["job_current_file_index"] = mCurrentFileIndex;
    Json::Value files(Json::arrayValue);

    for (AdhocFileCheckpointPtr fileCheckpoint : mAdhocFileCheckpointList) {
        Json::Value file;
        file["file_name"] = fileCheckpoint->mFileName;
        file["status"] = TransStatusToString(fileCheckpoint->mStatus);

        switch (fileCheckpoint->mStatus) {
            case STATUS_WAITING:
                file["dev"] = fileCheckpoint->mDevInode.dev;
                file["inode"] = fileCheckpoint->mDevInode.inode;
                file["size"] = fileCheckpoint->mSize;
                break;
            case STATUS_LOADING:
                file["dev"] = fileCheckpoint->mDevInode.dev;
                file["inode"] = fileCheckpoint->mDevInode.inode;
                file["offset"] = fileCheckpoint->mOffset;
                file["size"] = fileCheckpoint->mSize;
                file["sig_hash"] = fileCheckpoint->mSignatureHash;
                file["sig_size"] = fileCheckpoint->mSignatureSize;
                file["start_time"] = fileCheckpoint->mStartTime;
                file["update_time"] = fileCheckpoint->mLastUpdateTime;
                file["real_file_name"] = fileCheckpoint->mRealFileName;
                break;
            case STATUS_FINISHED:
                file["size"] = fileCheckpoint->mSize;
                file["start_time"] = fileCheckpoint->mStartTime;
                file["update_time"] = fileCheckpoint->mLastUpdateTime;
                file["real_file_name"] = fileCheckpoint->mRealFileName;
                break;
            case STATUS_LOST:
                file["update_time"] = fileCheckpoint->mLastUpdateTime;
                break;
        }

        files.append(file);
    }
    root["job_files"] = files;

    Json::StreamWriterBuilder writerBuilder;
    std::string jsonString = Json::writeString(writerBuilder, root);
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        LOG_ERROR(sLogger, ("open adhoc check point file error, file path", path));
        AlarmManager::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "open adhoc check point file failed");
        return;
    }
    ofs << jsonString;
    ofs.close();
}

bool AdhocJobCheckpoint::CheckFileConsistence(AdhocFileKey* fileKey) {
    if (mCurrentFileIndex >= mFileCount) {
        LOG_WARNING(sLogger, ("Get AdhocFileCheckpoint fail, job is finished, job name", mAdhocJobName));
        return false;
    }
    AdhocFileCheckpointPtr fileCheckpoint = mAdhocFileCheckpointList[mCurrentFileIndex];
    if (fileCheckpoint->mSignatureHash == fileKey->mSignatureHash
        && fileCheckpoint->mSignatureSize == fileKey->mSignatureSize
        && fileCheckpoint->mDevInode == fileKey->mDevInode) {
        return true;
    } else {
        LOG_WARNING(sLogger,
                    ("Get AdhocFileCheckpoint fail", "current FileCheckpoint in Job is not same as AdhocFileKey")(
                        "AdhocFileCheckpoint's name", fileCheckpoint->mFileName)(
                        "AdhocFileCheckpoint's Inode", fileCheckpoint->mDevInode.inode)("AdhocFileKey's Inode",
                                                                                        fileKey->mDevInode.inode));
        return false;
    }
}

int32_t AdhocJobCheckpoint::GetCurrentFileIndex() {
    return mCurrentFileIndex;
}

std::string AdhocJobCheckpoint::GetJobName() {
    return mAdhocJobName;
}

} // namespace logtail