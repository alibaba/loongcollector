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

#include "plugin/PluginRegistry.h"
#include "plugin/creator/StaticFlusherCreator.h"
#include "plugin/creator/StaticInputCreator.h"
#include "plugin/creator/StaticProcessorCreator.h"
#include "plugin/instance/FlusherInstance.h"
#include "plugin/instance/InputInstance.h"
#include "plugin/instance/ProcessorInstance.h"

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
    bool Init(const Json::Value& config, uint32_t& pluginIdx, Json::Value& optionalGoPipeline) override {
        auto processor
            = PluginRegistry::GetInstance()->CreateProcessor(ProcessorInnerMock::sName, std::to_string(++pluginIdx));
        processor->Init(Json::Value(), *mContext);
        mInnerProcessors.emplace_back(std::move(processor));
        return true;
    }
    bool Start() override { return true; }
    bool Stop(bool isPipelineRemoving) override { return true; }
};

const std::string InputMock::sName = "input_mock";

class ProcessorMock : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override { return true; }
    void Process(PipelineEventGroup& logGroup) override { ++mCnt; };

    uint32_t mCnt = 0;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override { return true; };
};

const std::string ProcessorMock::sName = "processor_mock";

class FlusherMock : public Flusher {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override { return true; }
    bool Start() override { return true; }
    bool Stop(bool isPipelineRemoving) override { return true; }
};

const std::string FlusherMock::sName = "flusher_mock";

void LoadPluginMock() {
    PluginRegistry::GetInstance()->RegisterInputCreator(new StaticInputCreator<InputMock>());
    PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorInnerMock>());
    PluginRegistry::GetInstance()->RegisterProcessorCreator(new StaticProcessorCreator<ProcessorMock>());
    PluginRegistry::GetInstance()->RegisterFlusherCreator(new StaticFlusherCreator<FlusherMock>());
}

} // namespace logtail
