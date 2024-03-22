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
#include <time.h>

#include <deque>
#include <map>
#include <unordered_map>

#include "reader/LogFileReader.h"

namespace logtail {

class Event;

struct RenameInfo {
    std::string mOldName;
    std::string mNewName;
    std::string mPath;
    int32_t mUpdateTime;
};

class EventHandler {
public:
    virtual void Handle(const Event& event) = 0;
    virtual void HandleTimeOut() = 0;
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag) = 0;
    virtual bool IsAllFileRead() { return true; }
    virtual ~EventHandler() {}
};

class TimeoutHandler : public EventHandler {
public:
    TimeoutHandler() {}

    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag) { return true; }
};

class ModifyHandler : public EventHandler {
private:
    typedef std::unordered_map<std::string, LogFileReaderPtrArray> NameLogFileReaderMap;
    typedef std::unordered_map<DevInode, LogFileReaderPtr, DevInodeHash, DevInodeEqual> DevInodeLogFileReaderMap;

    NameLogFileReaderMap mNameReaderMap;
    DevInodeLogFileReaderMap mDevInodeReaderMap;
    DevInodeLogFileReaderMap mRotatorReaderMap;
    uint64_t mReadFileTimeSlice;
    std::string mConfigName;
    int32_t mLastOverflowErrorTime;

    void DeleteTimeoutReader();
    void DeleteTimeoutReader(int32_t timeoutInterval);
    void DeleteRollbackReader();
    void MakeSpaceForNewReader();


    static bool CompareReaderByUpdateTime(const LogFileReader* left, const LogFileReader* right) {
        return left->GetLastUpdateTime() < right->GetLastUpdateTime();
    }

    LogFileReaderPtr CreateLogFileReaderPtr(const std::string& path,
                                            const std::string& name,
                                            const DevInode& devInode,
                                            const FileReaderConfig& readerConfig,
                                            const MultilineConfig& multilineConfig,
                                            const FileDiscoveryConfig& discoveryConfig,
                                            uint32_t exactlyonceConcurrency = 0,
                                            bool forceBeginingFlag = false);

    int32_t PushLogToProcessor(LogFileReaderPtr reader, LogBuffer* logBuffer);

    void ForceReadLogAndPush(LogFileReaderPtr reader);

    // no copy
    ModifyHandler(const ModifyHandler&);
    ModifyHandler& operator=(const ModifyHandler&);

public:
    ModifyHandler(const std::string& configName, const FileDiscoveryConfig& pConfig);
    virtual ~ModifyHandler();
    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag);
    bool IsAllFileRead() override;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ConfigUpdatorUnittest;
    friend class EventDispatcherTest;
    friend class SenderUnittest;
    friend class ModifyHandlerUnittest;
#endif
};

// handler for creation of directory
class CreateHandler : public EventHandler {
public:
    CreateHandler() {}
    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();

    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag) { return true; }
};

class NormalEventHandler : public EventHandler {
private:
    CreateHandler* mCreateHandlerPtr;

    // no copy
    NormalEventHandler(const NormalEventHandler&);
    NormalEventHandler& operator=(const NormalEventHandler&);

public:
    virtual ~NormalEventHandler() { delete mCreateHandlerPtr; }
    NormalEventHandler() : mCreateHandlerPtr(new CreateHandler()) {}

    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag) { return true; }
};

class CreateModifyHandler : public EventHandler {
private:
    CreateHandler* mCreateHandlerPtr;
    typedef std::unordered_map<std::string, ModifyHandler*> ModifyHandlerMap;
    ModifyHandlerMap mModifyHandlerPtrMap;

    // no copy
    CreateModifyHandler(const CreateModifyHandler&);
    CreateModifyHandler& operator=(const CreateModifyHandler&);

public:
    CreateModifyHandler(CreateHandler* createHandler) : mCreateHandlerPtr(createHandler) {}

    virtual ~CreateModifyHandler();
    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool isRotatorReader, bool checkConfigFlag);
    bool IsAllFileRead() override;

    ModifyHandler* GetOrCreateModifyHandler(const std::string& configName, const FileDiscoveryConfig& pConfig);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CreateModifyHandlerUnittest;
    friend class ConfigUpdatorUnittest;
    friend class EventDispatcherTest;
    friend class SenderUnittest;
#endif
};

} // namespace logtail
