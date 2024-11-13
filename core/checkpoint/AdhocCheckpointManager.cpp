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

#include "app_config/AppConfig.h"
#include "AdhocCheckpointManager.h"
#include "common/FileSystemUtil.h"
#include "common/Flags.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"
#include "common/Thread.h"
#include "common/HashUtil.h"

DEFINE_FLAG_INT32(adhoc_checkpoint_dump_thread_wait_interval, "microseconds", 5 * 1000);

namespace logtail {

AdhocJobCheckpointPtr AdhocCheckpointManager::GetAdhocJobCheckpoint(const std::string& jobName) {
    AdhocJobCheckpointPtr jobCheckpoint = nullptr;
    auto it = mAdhocJobCheckpointMap.find(jobName);
    if (it != mAdhocJobCheckpointMap.end()) {
        jobCheckpoint = it->second;
    } else {
        LOG_WARNING(sLogger, ("Get AdhocJobCheckpoint fail, job checkpoint doesn't exist, job name", jobName));
    }

    return jobCheckpoint;
}

AdhocJobCheckpointPtr
AdhocCheckpointManager::CreateAdhocJobCheckpoint(const std::string& jobName,
                                                 std::vector<AdhocFileCheckpointPtr>& fileCheckpointList) {
    AdhocJobCheckpointPtr jobCheckpoint = GetAdhocJobCheckpoint(jobName);

    if (nullptr == jobCheckpoint) {
        jobCheckpoint = std::make_shared<AdhocJobCheckpoint>(jobName);
        for (AdhocFileCheckpointPtr fileCheckpoint : fileCheckpointList) {
            jobCheckpoint->AddFileCheckpoint(fileCheckpoint);
        }
        mAdhocJobCheckpointMap[jobName] = jobCheckpoint;

        LOG_INFO(sLogger, ("Create AdhocJobCheckpoint success, job name", jobName));

    } else {
        LOG_INFO(sLogger, ("Job checkpoint already exists, job name", jobName));
    }

    return jobCheckpoint;
}

AdhocFileCheckpointPtr AdhocCheckpointManager::CreateAdhocFileCheckpoint(const std::string& jobName,
                                                                         const std::string& filePath) {
    fsutil::PathStat buf;
    if (fsutil::PathStat::stat(filePath.c_str(), buf)) {
        uint64_t fileSignatureHash = 0;
        uint32_t fileSignatureSize = 0;
        char firstLine[1025];
        int fd = open(filePath.c_str(), O_RDONLY);
        // Check if the file handle is opened successfully.
        if (fd == -1) {
            LOG_ERROR(sLogger, ("fail to open file", filePath));
            return nullptr;
        }
        int nbytes = pread(fd, firstLine, 1024, 0);
        if (nbytes < 0) {
            close(fd);
            LOG_ERROR(sLogger, ("fail to read file", filePath)("nbytes", nbytes)("job name", jobName));
            return nullptr;
        }
        firstLine[nbytes] = '\0';
        CheckAndUpdateSignature(std::string(firstLine), fileSignatureHash, fileSignatureSize);
        AdhocFileCheckpointPtr fileCheckpoint
            = std::make_shared<AdhocFileCheckpoint>(filePath, // file path
                                                    buf.GetFileSize(), // file size
                                                    0, // offset
                                                    fileSignatureSize, // signatureSize
                                                    fileSignatureHash, // signatureHash
                                                    buf.GetDevInode(), // DevInode
                                                    STATUS_WAITING, // Status
                                                    jobName, // job name
                                                    filePath); // real file path

        close(fd);
        return fileCheckpoint;
    } else {
        LOG_WARNING(sLogger, ("Create file checkpoint fail, job name", jobName)("file path", filePath));
        return nullptr;
    }
}

void AdhocCheckpointManager::DeleteAdhocJobCheckpoint(const std::string& jobName) {
    auto jobCheckpoint = mAdhocJobCheckpointMap.find(jobName);
    if (jobCheckpoint != mAdhocJobCheckpointMap.end()) {
        remove(GetJobCheckpointPath(jobName).c_str());
        mAdhocJobCheckpointMap.erase(jobName);
        LOG_INFO(sLogger, ("Delete AdhocJobCheckpoint success, job name", jobName));
    } else {
        LOG_WARNING(sLogger, ("Delete AdhocJobCheckpoint fail, job checkpoint doesn't exist, job name", jobName));
    }
}

AdhocFileCheckpointPtr AdhocCheckpointManager::GetAdhocFileCheckpoint(const std::string& jobName,
                                                                      AdhocFileKey* fileKey) {
    AdhocJobCheckpointPtr jobCheckpoint = GetAdhocJobCheckpoint(jobName);
    if (nullptr != jobCheckpoint) {
        return jobCheckpoint->GetFileCheckpoint(fileKey);
    } else {
        return nullptr;
    }
}

void AdhocCheckpointManager::UpdateAdhocFileCheckpoint(const std::string& jobName,
                                                       AdhocFileKey* fileKey,
                                                       AdhocFileCheckpointPtr fileCheckpoint) {
    AdhocJobCheckpointPtr jobCheckpoint = GetAdhocJobCheckpoint(jobName);
    if (nullptr != jobCheckpoint) {
        if (jobCheckpoint->UpdateFileCheckpoint(fileKey, fileCheckpoint)) {
            jobCheckpoint->Dump(GetJobCheckpointPath(jobName), false);
        }
    }
}

void AdhocCheckpointManager::DumpAdhocCheckpoint() {
    // dump checkpoint per 5s
    if (time(NULL) - mLastDumpTime > INT32_FLAG(adhoc_checkpoint_dump_thread_wait_interval)) {
        for (auto& p : mAdhocJobCheckpointMap) {
            p.second->Dump(GetJobCheckpointPath(p.second->GetJobName()), true);
        }
        mLastDumpTime = time(NULL);
    }
}

void AdhocCheckpointManager::LoadAdhocCheckpoint() {
    std::string adhocCheckpointDir = GetAdhocCheckpointDirPath();
    if (CheckExistance(adhocCheckpointDir)) {
        LOG_INFO(sLogger, ("Open adhoc checkpoint dir", "success"));
        std::vector<std::string> jobList;
        if (!GetAllFiles(adhocCheckpointDir, "*", jobList)) {
            LOG_WARNING(sLogger, ("get all adhoc checkpoint files", "failed"));
            AlarmManager::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "Load adhoc check point files failed");
            return;
        }

        for (std::string job : jobList) {
            AdhocJobCheckpointPtr jobCheckpoint = std::make_shared<AdhocJobCheckpoint>(job);
            if (jobCheckpoint->Load(GetJobCheckpointPath(job))) {
                mAdhocJobCheckpointMap[job] = jobCheckpoint;
            }
        }
    } else if (!Mkdir(adhocCheckpointDir)) {
        LOG_WARNING(sLogger, ("Create adhoc checkpoint dir", "failed"));
        AlarmManager::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "Create adhoc check point dir failed");
    }
}

std::string AdhocCheckpointManager::GetJobCheckpointPath(const std::string& jobName) {
    std::string path = GetAdhocCheckpointDirPath();
#if defined(__linux__)
    path += "/" + jobName;
#elif defined(_MSC_VER)
    path += "\\" + jobName;
#endif
    return path;
}

} // namespace logtail