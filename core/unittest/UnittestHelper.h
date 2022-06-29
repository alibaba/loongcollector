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

#include <sys/utsname.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string>
#include "logger/Logger.h"

namespace logtail {

class UnitTestHelper {
public:
    static bool GetKernelVersion(int& mainVersion, int& subVersion) {
        struct utsname buf;
        if (uname(&buf) != 0) {
            LOG_ERROR(sLogger, ("failed to get linux kernel version, errno", errno));
            return false;
        }

        std::string releaseVersion(buf.release);
        size_t pos = releaseVersion.find_first_of('-');
        if (pos == std::string::npos) {
            LOG_ERROR(sLogger, ("failed to get linux kernel version, release info", releaseVersion));
            return false;
        }

        size_t mainPos = releaseVersion.find_first_of('.');
        if (mainPos != std::string::npos) {
            mainVersion = atoi(releaseVersion.substr(0, mainPos).c_str());
        }
        size_t subPos = releaseVersion.find_first_of('.', mainPos + 1);
        if (subPos != std::string::npos) {
            subVersion = atoi(releaseVersion.substr(mainPos + 1, subPos - mainPos).c_str());
        }
        return true;
    }
};

} // namespace logtail
