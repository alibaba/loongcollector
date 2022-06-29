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

#include "RuntimeUtil.h"
#if defined(__linux__)
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#include <Psapi.h>
#endif
#include <errno.h>
#include <cstdio>
#include <sstream>
#include "logger/Logger.h"
#include "LogtailCommonFlags.h"
#include "FileSystemUtil.h"

namespace logtail {

// TODO: In ConfigManager.cpp, some places use / to concat path, which might fail on Windows,
// replace them with PATH_SEPARATOR.
std::string GetProcessExecutionDir(void) {
#if defined(__linux__)
    char exePath[PATH_MAX + 1] = {0};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    std::string fullPath(exePath);
    size_t index = fullPath.rfind(PATH_SEPARATOR);
    if (index == std::string::npos) {
        return "";
    }
    return fullPath.substr(0, index + 1);
#elif defined(_MSC_VER)
    auto fullPath = GetBinaryName();
    auto index = fullPath.rfind(PATH_SEPARATOR);
    return (std::string::npos == index) ? "" : fullPath.substr(0, index + 1);
#endif
}

std::string GetBinaryName(void) {
#if defined(__linux__)
    char exePath[PATH_MAX + 1] = {0};
    readlink("/proc/self/exe", exePath, sizeof(exePath));
    std::string fullPath(exePath);
    return fullPath;
#elif defined(_MSC_VER)
    TCHAR filename[MAX_PATH];
    if (0 == GetModuleFileName(NULL, filename, MAX_PATH)) {
        LOG_ERROR(sLogger, ("GetModuleFileName failed", GetLastError()));
        return "";
    }
    return std::string(filename);
#endif
}

// only ilogtail_config.json will be rebuild from memory
bool RebuildExecutionDir(const std::string& ilogtailConfigJson,
                         std::string& errorMessage,
                         const std::string& executionDir) {
    std::string path = executionDir.empty() ? GetProcessExecutionDir() : executionDir;
    if (CheckExistance(path))
        return true;
    if (!Mkdir(path)) {
        std::stringstream ss;
        ss << "create execution dir failed, errno is " << errno;
        errorMessage = ss.str();
        return false;
    }

    if (ilogtailConfigJson.empty())
        return true;

    FILE* pFile = fopen((path + STRING_FLAG(ilogtail_config)).c_str(), "w");
    if (pFile == NULL) {
        std::stringstream ss;
        ss << "open " << STRING_FLAG(ilogtail_config) << " to write failed, errno is " << errno;
        errorMessage = ss.str();
        return false;
    }

    fwrite(ilogtailConfigJson.c_str(), 1, ilogtailConfigJson.size(), pFile);
    fclose(pFile);
    return true;
}

} // namespace logtail
