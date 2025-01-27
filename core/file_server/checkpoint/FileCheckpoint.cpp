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

#include "file_server/checkpoint/FileCheckpoint.h"

using namespace std;

namespace logtail {

const string& FileStatusToString(FileStatus status) {
    switch (status) {
        case FileStatus::WAITING:
            static const string waitingStr = "waiting";
            return waitingStr;
        case FileStatus::READING:
            static const string readingStr = "reading";
            return readingStr;
        case FileStatus::FINISHED:
            static const string finishedStr = "finished";
            return finishedStr;
        case FileStatus::ABORT:
            static const string abortStr = "abort";
            return abortStr;
        default:
            static const string emptyStr = "";
            return emptyStr;
    }
}

FileStatus GetFileStatusFromString(const string& statusStr) {
    if (statusStr == "reading") {
        return FileStatus::READING;
    } else if (statusStr == "finished") {
        return FileStatus::FINISHED;
    } else if (statusStr == "abort") {
        return FileStatus::ABORT;
    } else {
        return FileStatus::WAITING;
    }
}

} // namespace logtail
