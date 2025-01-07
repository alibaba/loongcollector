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

#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include "boost/thread.hpp"

namespace logtail {

class Thread {
    // do not use std::thread, this has a bug before GCC8.0. More details: https://zhuanlan.zhihu.com/p/59554240
    boost::thread thread_;

public:
    template <class Function, class... Args>
    explicit Thread(Function&& f, Args&&... args) : thread_(std::forward<Function>(f), std::forward<Args>(args)...) {}

    ~Thread() { GetValue(1000 * 100); }

    void GetValue(unsigned long long microseconds) {
        // TODO: Add timeout.
        if (thread_.joinable())
            thread_.join();
    }

    void Wait(unsigned long long microseconds) { GetValue(microseconds); }

    int GetState() {
        // TODO:
        return 0;
    }
};

using ThreadPtr = std::shared_ptr<Thread>;

template <class Function, class... Args>
ThreadPtr CreateThread(Function&& f, Args&&... args) {
    return ThreadPtr(new Thread(std::forward<Function>(f), std::forward<Args>(args)...));
}

class JThread {
public:
    JThread() noexcept = default;
    template <typename Callable, typename... Args>
    explicit JThread(Callable&& func, Args&&... args) : t(std::forward<Callable>(func), std::forward<Args>(args)...) {}
    explicit JThread(std::thread t_) noexcept : t(std::move(t_)) {}
    ~JThread() noexcept {
        if (Joinable()) {
            Join();
        }
    }

    JThread(JThread&& other) noexcept : t(std::move(other.t)) {}
    JThread& operator=(JThread&& other) noexcept {
        if (Joinable()) {
            Join();
        }
        t = std::move(other.t);
        return *this;
    }
    JThread& operator=(std::thread other) noexcept {
        if (Joinable()) {
            Join();
        }
        t = std::move(other);
        return *this;
    }

    bool Joinable() const noexcept { return t.joinable(); }
    void Join() { t.join(); }
    void Detach() { t.detach(); }
    std::thread& Get() noexcept { return t; }
    const std::thread& Get() const noexcept { return t; }

private:
    std::thread t;
};

} // namespace logtail
