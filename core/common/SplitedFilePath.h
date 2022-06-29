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

#ifndef _LOG_SPLITED_FILE_PATH_H__
#define _LOG_SPLITED_FILE_PATH_H__

#include <string>
#include "FileSystemUtil.h"

namespace logtail {

struct SplitedFilePath {
    std::string mFileDir;
    std::string mFileName;

    SplitedFilePath() {}
    SplitedFilePath(const std::string fileFullPath) {
        size_t lastPos = fileFullPath.rfind(PATH_SEPARATOR[0]);
        if (lastPos == std::string::npos || lastPos == (size_t)0) {
            return;
        }
        mFileDir = fileFullPath.substr(0, lastPos);
        mFileName = fileFullPath.substr(lastPos + 1, fileFullPath.size() - lastPos - 1);
    }
    SplitedFilePath(const std::string& fileDir, const std::string fileName) : mFileDir(fileDir), mFileName(fileName) {}

    bool operator==(const SplitedFilePath& o) const { return mFileDir == o.mFileDir && mFileName == o.mFileName; }

    bool operator<(const SplitedFilePath& o) const {
        if (mFileDir < o.mFileDir) {
            return true;
        } else if (mFileDir == o.mFileDir) {
            return mFileName < o.mFileName;
        } else {
            return false;
        }
    }
};

} // namespace logtail

#endif //_LOG_SPLITED_FILE_PATH_H__