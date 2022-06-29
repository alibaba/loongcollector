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
#include "reader/LogFileReader.h"
#include <time.h>
#include <map>
#include <deque>
#include <unordered_map>

namespace logtail {

class Config;
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
    virtual bool DumpReaderMeta(bool checkConfigFlag = false) = 0;
    virtual ~EventHandler() {}
};

class TimeoutHandler : public EventHandler {
public:
    TimeoutHandler() {}

    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool checkConfigFlag = false) { return true; }
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
                                            Config* pConfig,
                                            const DevInode& devInode,
                                            bool forceBeginingFlag = false);

    //no copy
    ModifyHandler(const ModifyHandler&);
    ModifyHandler& operator=(const ModifyHandler&);

public:
    ModifyHandler(const std::string& configName, Config* pConfig);
    virtual ~ModifyHandler();
    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool checkConfigFlag = false);

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

    virtual bool DumpReaderMeta(bool checkConfigFlag = false) { return true; }
};

class NormalEventHandler : public EventHandler {
private:
    CreateHandler* mCreateHandlerPtr;

    //no copy
    NormalEventHandler(const NormalEventHandler&);
    NormalEventHandler& operator=(const NormalEventHandler&);

public:
    virtual ~NormalEventHandler() { delete mCreateHandlerPtr; }
    NormalEventHandler() : mCreateHandlerPtr(new CreateHandler()) {}

    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool checkConfigFlag = false) { return true; }
};

class CreateModifyHandler : public EventHandler {
private:
    CreateHandler* mCreateHandlerPtr;
    typedef std::unordered_map<std::string, ModifyHandler*> ModifyHandlerMap;
    ModifyHandlerMap mModifyHandlerPtrMap;

    //no copy
    CreateModifyHandler(const CreateModifyHandler&);
    CreateModifyHandler& operator=(const CreateModifyHandler&);

public:
    CreateModifyHandler(CreateHandler* createHandler) : mCreateHandlerPtr(createHandler) {}

    virtual ~CreateModifyHandler();
    virtual void Handle(const Event& event);
    virtual void HandleTimeOut();
    virtual bool DumpReaderMeta(bool checkConfigFlag = false);

    ModifyHandler* GetOrCreateModifyHandler(const std::string& configName, Config* pConfig = NULL);

#ifdef APSARA_UNIT_TEST_MAIN
    friend class CreateModifyHandlerUnittest;
    friend class ConfigUpdatorUnittest;
    friend class EventDispatcherTest;
    friend class SenderUnittest;
#endif
};

} // namespace logtail
