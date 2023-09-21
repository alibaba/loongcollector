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

#include "AdhocFileCheckpoint.h"
#include "logger/Logger.h"

namespace logtail {

std::string TransStatusToString(FileReadStatus status) {
    switch (status) {
        case STATUS_WAITING:
            return "waiting";
        case STATUS_LOADING:
            return "loading";
        case STATUS_FINISHED:
            return "finished";
        case STATUS_LOST:
            return "lost";
        default:
            return "";
    }
}

FileReadStatus GetStatusFromString(std::string statusStr) {
    if (statusStr == "waiting") {
        return STATUS_WAITING;
    } else if (statusStr == "loading") {
        return STATUS_LOADING;
    } else if (statusStr == "finished") {
        return STATUS_FINISHED;
    } else if (statusStr == "lost") {
        return STATUS_LOST;
    } else {
        LOG_WARNING(sLogger, ("Adhoc file status wrong", statusStr));
        return STATUS_LOST;
    }
}

}