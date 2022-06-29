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
#if defined(__linux__)
#include <errno.h>
#include <string.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#include <string>
#elif defined(__APPLE__)
#include <sys/unistd.h>
#include <sys/errno.h>
#endif

// Errno utility.
// The rule for Windows:
// - If you call an POSIX API such as _mkdir, errno and GetLastError will be set to corresponding
//   errnos which represent same meaning although with different values, such as
//   (errno == EACCES) <=> (GetLastError() == ERROR_ACCESS_DENIED).
// - However, if you call Windows API, only GetLastError() will be set.
namespace logtail {

inline int GetErrno() {
#if defined(__linux__) || defined(__APPLE__)
    return errno;
#elif defined(_MSC_VER)
    return GetLastError();
#endif
}

#if defined(__linux__) || defined(__APPLE__)
inline const char* ErrnoToString(int e) {
    return strerror(e);
}
#elif defined(_MSC_VER)
inline std::string ErrnoToString(int e) {
    return std::to_string(e);
}
#endif

} // namespace logtail
