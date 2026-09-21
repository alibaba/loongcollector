// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <exception>
#include <future>
#include <memory>
#include <system_error>
#include <thread>
#include <utility>

namespace logtail {

void HandleThreadCreateFailure(const char* threadName, const std::exception& ex);

#ifdef APSARA_UNIT_TEST_MAIN
using ThreadCreateFailHandler = void (*)(const char* threadName, const std::exception& ex);
void SetThreadCreateFailHandlerForTest(ThreadCreateFailHandler handler);
void SetForceThreadCreateFailureForTest(bool enable);
bool IsForceThreadCreateFailureForTest();

inline std::system_error ForcedThreadCreateError() {
    return std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
}
#endif

template <typename R, typename F, typename... Args>
void LaunchAsync(std::future<R>& dest, const char* name, F&& f, Args&&... args) {
    try {
#ifdef APSARA_UNIT_TEST_MAIN
        if (IsForceThreadCreateFailureForTest()) {
            throw ForcedThreadCreateError();
        }
#endif
        dest = std::async(std::launch::async, std::forward<F>(f), std::forward<Args>(args)...);
    } catch (const std::system_error& e) {
        dest = std::future<R>();
        HandleThreadCreateFailure(name, e);
    }
}

template <typename F, typename... Args>
void LaunchStdThread(std::thread& dest, const char* name, F&& f, Args&&... args) {
    try {
#ifdef APSARA_UNIT_TEST_MAIN
        if (IsForceThreadCreateFailureForTest()) {
            throw ForcedThreadCreateError();
        }
#endif
        dest = std::thread(std::forward<F>(f), std::forward<Args>(args)...);
    } catch (const std::system_error& e) {
        HandleThreadCreateFailure(name, e);
    }
}

template <typename F, typename... Args>
std::unique_ptr<std::thread> MakeStdThread(const char* name, F&& f, Args&&... args) {
    std::thread t;
    LaunchStdThread(t, name, std::forward<F>(f), std::forward<Args>(args)...);
    if (!t.joinable()) {
        return nullptr;
    }
    return std::make_unique<std::thread>(std::move(t));
}

} // namespace logtail
