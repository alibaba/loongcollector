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

#include <string>

#include "pipeline/Pipeline.h"
#include "pipeline/plugin/PluginRegistry.h"
#include "pipeline/plugin/creator/StaticFlusherCreator.h"
#include "pipeline/plugin/creator/StaticInputCreator.h"
#include "pipeline/plugin/creator/StaticProcessorCreator.h"
#include "pipeline/plugin/interface/Flusher.h"
#include "pipeline/plugin/interface/HttpFlusher.h"
#include "pipeline/plugin/interface/Input.h"
#include "pipeline/plugin/interface/Processor.h"
#include "pipeline/queue/SLSSenderQueueItem.h"
#include "pipeline/queue/SenderQueueManager.h"
#include "plugin/flusher/sls/FlusherSLS.h"
#include "task_pipeline/Task.h"
#include "task_pipeline/TaskRegistry.h"

namespace logtail {

class ProcessorInnerMock : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override { return true; }
    void Process(PipelineEventGroup& logGroup) override { ++mCnt; };

    uint32_t mCnt = 0;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override { return true; };
};

const std::string ProcessorInnerMock::sName = "processor_inner_mock";

class InputMock : public Input {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override {
        if (config.isMember("SupportAck")) {
            mSupportAck = config["SupportAck"].asBool();
        }
        auto processor = PluginRegistry::GetInstance()->CreateProcessor(
            ProcessorInnerMock::sName, mContext->GetPipeline().GenNextPluginMeta(false));
        processor->Init(Json::Value(), *mContext);
        mInnerProcessors.emplace_back(std::move(processor));
        return true;
    }
    bool Start() override { return true; }
    bool Stop(bool isPipelineRemoving) override {
        while (mBlockFlag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            LOG_DEBUG(sLogger, ("input mock", "block"));
        }
        return true;
    }
    bool SupportAck() const override { return mSupportAck; }

    void Block() { mBlockFlag = true; }
    void Unblock() { mBlockFlag = false; }

    bool mSupportAck = true;

private:
    std::atomic_bool mBlockFlag = false;
};

const std::string InputMock::sName = "input_mock";

class InputFileMock : public InputMock {
public:
    static const std::string sName;
};

const std::string InputFileMock::sName = "input_file_mock";

class InputFileMock2 : public InputMock {
public:
    static const std::string sName;
};

const std::string InputFileMock2::sName = "input_file_mock2";

class ProcessorMock : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override {
        std::string errorMsg;
        bool block = false;
        if (GetOptionalBoolParam(config, "Block", block, errorMsg)) {
            mBlockFlag = block;
        }
        return true;
    }
    void Process(PipelineEventGroup& logGroup) override {
        while (mBlockFlag) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            LOG_DEBUG(sLogger,
                      ("processor mock", "block")("cnt", mCnt)("data", logGroup.GetEvents()[0]->ToJsonString()));
        }
        ++mCnt;
    };

    void Block() { mBlockFlag.store(true); }
    void Unblock() { mBlockFlag.store(false); }

    uint32_t mCnt = 0;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override { return true; };

    std::atomic_bool mBlockFlag = false;
};

const std::string ProcessorMock::sName = "processor_mock";

class ProcessorMock2 : public ProcessorMock {
public:
    static const std::string sName;
};

const std::string ProcessorMock2::sName = "processor_mock2";

class FlusherMock : public Flusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override {
        GenerateQueueKey("mock");
        SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, *mContext);
        return true;
    }
    bool Send(PipelineEventGroup&& g) override { return mIsValid; }
    bool Flush(size_t key) override {
        mFlushedQueues.push_back(key);
        return true;
    }
    bool FlushAll() override { return mIsValid; }

    bool mIsValid = true;
    std::vector<size_t> mFlushedQueues;
};

const std::string FlusherMock::sName = "flusher_mock";

class FlusherHttpMock : public HttpFlusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override {
        GenerateQueueKey("mock");
        SenderQueueManager::GetInstance()->CreateQueue(mQueueKey, mPluginID, *mContext);
        return true;
    }
    bool Send(PipelineEventGroup&& g) override { return mIsValid; }
    bool Flush(size_t key) override {
        mFlushedQueues.push_back(key);
        return true;
    }
    bool FlushAll() override { return mIsValid; }
    bool BuildRequest(SenderQueueItem* item, std::unique_ptr<HttpSinkRequest>& req, bool* keepItem) const override {
        if (item->mData == "invalid_keep") {
            *keepItem = true;
            return false;
        }
        if (item->mData == "invalid_discard") {
            *keepItem = false;
            return false;
        }
        req = std::make_unique<HttpSinkRequest>(
            "", false, "", 80, "", "", std::map<std::string, std::string>(), "", nullptr);
        return true;
    }
    void OnSendDone(const HttpResponse& response, SenderQueueItem* item) override {}

    bool mIsValid = true;
    std::vector<size_t> mFlushedQueues;
};

const std::string FlusherHttpMock::sName = "flusher_http_mock";

class FlusherSLSMock : public FlusherSLS {
public:
    static const std::string sName;

    bool BuildRequest(SenderQueueItem* item, std::unique_ptr<HttpSinkRequest>& req, bool* keepItem) const override {
        auto data = static_cast<SLSSenderQueueItem*>(item);
        std::map<std::string, std::string> header;
        req = std::make_unique<HttpSinkRequest>(
            "POST", false, "test-host", 80, "/test-operation", "", header, data->mData, item);
        return true;
    }
};

const std::string FlusherSLSMock::sName = "flusher_sls_mock";

class FlusherSLSMock2 : public FlusherSLSMock {
public:
    static const std::string sName;
};

const std::string FlusherSLSMock2::sName = "flusher_sls_mock2";

class TaskMock : public Task {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override {
        if (config.isMember("Valid")) {
            return config["Valid"].asBool();
        }
        return true;
    }
    void Start() override { mIsRunning = true; }
    void Stop(bool isRemoving) { mIsRunning = false; }

    bool mIsRunning = false;
};

const std::string TaskMock::sName = "task_mock";

void LoadPluginMock() {
    PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputMock>());
    PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorInnerMock>());
    PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorMock>());
    PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherMock>());
    PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherHttpMock>());
    PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLSMock>());
    PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputFileMock>());
    PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputFileMock2>());
    PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorMock2>());
    PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherSLSMock2>());
}

void LoadTaskMock() {
    TaskRegistry::GetInstance()->RegisterCreator(TaskMock::sName, []() { return std::make_unique<TaskMock>(); });
}

} // namespace logtail
