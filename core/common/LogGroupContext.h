/*
 * Copyright 2022 iLogtail Authors
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

#pragma once
#include "config/IntegrityConfig.h"
#include "profiler/LogtailAlarm.h"
#include "FileInfo.h"
#include "RangeCheckpoint.h"

namespace logtail {

// store context info according to log group, may be used in closure callback
struct LogGroupContext {
    LogGroupContext(const std::string& region = "",
                    const std::string& project = "",
                    const std::string& logStore = "",
                    const FileInfoPtr& infoPtr = FileInfoPtr(),
                    const IntegrityConfigPtr& integrityConfigPtr = IntegrityConfigPtr(),
                    const LineCountConfigPtr& lineCountConfigPtr = LineCountConfigPtr(),
                    int64_t seqNum = -1,
                    bool fusemode = false,
                    bool markOffsetFlag = false,
                    const RangeCheckpointPtr& exactlyOnceCheckpoint = RangeCheckpointPtr())
        : mRegion(region),
          mProjectName(project),
          mLogStore(logStore),
          mFileInfoPtr(infoPtr),
          mIntegrityConfigPtr(integrityConfigPtr),
          mLineCountConfigPtr(lineCountConfigPtr),
          mSeqNum(seqNum),
          mFuseMode(fusemode),
          mMarkOffsetFlag(markOffsetFlag),
          mExactlyOnceCheckpoint(exactlyOnceCheckpoint) {}

    void SendAlarm(LogtailAlarmType type, const std::string& msg) const {
        LogtailAlarm::GetInstance()->SendAlarm(type, msg, mProjectName, mLogStore, mRegion);
    }

    std::string mRegion;
    std::string mProjectName;
    std::string mLogStore;

    FileInfoPtr mFileInfoPtr;

    IntegrityConfigPtr mIntegrityConfigPtr;
    LineCountConfigPtr mLineCountConfigPtr;

    int64_t mSeqNum;
    bool mFuseMode;
    bool mMarkOffsetFlag;

    RangeCheckpointPtr mExactlyOnceCheckpoint;
};

} // namespace logtail
