/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "common/Lock.h"
#include "common/Thread.h"

namespace logtail {

class Application {
public:
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    static Application* GetInstance() {
        static Application instance;
        return &instance;
    }

    void Init();
    void Start();
    void SetSigTermSignalFlag(bool flag) { mSigTermSignalFlag = flag; }
    bool IsExiting() { return mSigTermSignalFlag; }

    std::string GetInstanceId() { return mInstanceId; }
    bool TryGetUUID();
    std::string GetUUID() {
        mUUIDLock.lock();
        std::string uuid(mUUID);
        mUUIDLock.unlock();
        return uuid;
    }

private:
    Application();
    ~Application() = default;

    void Exit();
    void CheckCriticalCondition(int32_t curTime);

    bool GetUUIDThread();
    void SetUUID(std::string uuid) {
        mUUIDLock.lock();
        mUUID = uuid;
        mUUIDLock.unlock();
        return;
    }

    std::string mInstanceId;
    int32_t mStartTime;
    std::atomic_bool mSigTermSignalFlag = false;
    JThread mUUIDThread;
    SpinLock mUUIDLock;
    std::string mUUID;
};

} // namespace logtail
