#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include "common/Thread.h"
#include "common/Lock.h"

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
