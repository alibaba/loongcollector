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

#include "PollingCache.h"
#include "common/Flags.h"

DEFINE_FLAG_INT32(max_file_not_exist_times, "treate as deleted when file stat failed XX times, default", 10);

namespace logtail {

void ModifyCheckCache::UpdateFileProperty(uint64_t fileSize, timespec modifyTime) {
    mFileSize = fileSize;
    mModifyTime = modifyTime;
    mNotExistTimes = 0;
}

bool ModifyCheckCache::UpdateFileNotExist() {
    ++mNotExistTimes;
    if (mNotExistTimes >= INT32_FLAG(max_file_not_exist_times)) {
        return true;
    }
    return false;
}

void ModifyCheckCache::UpdateFileProperty(uint64_t dev, uint64_t inode, uint64_t fileSize, timespec modifyTime) {
    mDev = dev;
    mInode = inode;
    mFileSize = fileSize;
    mModifyTime = modifyTime;
    mNotExistTimes = 0;
}

} // namespace logtail
