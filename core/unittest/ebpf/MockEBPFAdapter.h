// Copyright 2025 iLogtail Authors
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

#include <gmock/gmock.h>

#include <memory>
#include <string>
#include <vector>

#include "ebpf/EBPFAdapter.h"
#include "ebpf/include/export.h"
#include "unittest/Unittest.h"

namespace logtail::ebpf {

class MockEBPFAdapter : public EBPFAdapter {
public:
    MockEBPFAdapter() = default;
    virtual ~MockEBPFAdapter() = default;

    MOCK_METHOD0(Init, void());

    bool StartPlugin(PluginType pluginType, [[maybe_unused]] std::unique_ptr<PluginConfig> conf) override {
        mStartedPlugins.push_back(pluginType);
        return true;
    }

    bool StopPlugin(PluginType pluginType) override {
        mStoppedPlugins.push_back(pluginType);
        return true;
    }

    bool SuspendPlugin(PluginType pluginType) override {
        mSuspendedPlugins.push_back(pluginType);
        return true;
    }

    bool UpdatePlugin(PluginType pluginType, [[maybe_unused]] std::unique_ptr<PluginConfig> conf) override {
        mUpdatedPlugins.push_back(pluginType);
        return true;
    }

    bool ResumePlugin(PluginType pluginType, [[maybe_unused]] std::unique_ptr<PluginConfig> conf) override {
        mResumedPlugins.push_back(pluginType);
        return true;
    }

    MOCK_METHOD4(PollPerfBuffers, int32_t(PluginType, int32_t, int32_t*, int));

    MOCK_METHOD1(ConsumePerfBufferData, int32_t(PluginType pluginType));

    MOCK_METHOD1(GetPerfBufferEpollFds, std::vector<int>(PluginType pluginType));

    MOCK_METHOD2(SetNetworkObserverConfig, bool(int32_t key, int32_t value));

    MOCK_METHOD3(SetNetworkObserverCidFilter, bool(const std::string&, bool update, uint64_t cidKey));

    MOCK_METHOD5(BPFMapUpdateElem,
                 bool(PluginType pluginType, const std::string& mapName, void* key, void* value, uint64_t flag));

    // Add tracking methods for testing
    std::vector<PluginType> GetStartedPlugins() const { return mStartedPlugins; }
    std::vector<PluginType> GetStoppedPlugins() const { return mStoppedPlugins; }
    std::vector<PluginType> GetSuspendedPlugins() const { return mSuspendedPlugins; }
    std::vector<PluginType> GetResumedPlugins() const { return mResumedPlugins; }
    std::vector<PluginType> GetUpdatedPlugins() const { return mUpdatedPlugins; }


    void ClearTracking() {
        mStartedPlugins.clear();
        mStoppedPlugins.clear();
        mSuspendedPlugins.clear();
        mResumedPlugins.clear();
        mUpdatedPlugins.clear();
    }

    // Check if plugins are properly paired
    void AssertStartStopPaired(PluginType pluginType) const {
        size_t startCount = std::count(mStartedPlugins.begin(), mStartedPlugins.end(), pluginType);
        size_t stopCount = std::count(mStoppedPlugins.begin(), mStoppedPlugins.end(), pluginType);
        APSARA_TEST_EQUAL_FATAL(startCount, stopCount);
    }

    void AssertSuspendResumePaired(PluginType pluginType) const {
        size_t suspendCount = std::count(mSuspendedPlugins.begin(), mSuspendedPlugins.end(), pluginType);
        size_t resumeCount = std::count(mResumedPlugins.begin(), mResumedPlugins.end(), pluginType);
        APSARA_TEST_EQUAL_FATAL(suspendCount, resumeCount);
    }

    // Get counts for specific plugin type
    size_t GetStartCount(PluginType pluginType) const {
        return std::count(mStartedPlugins.begin(), mStartedPlugins.end(), pluginType);
    }

    size_t GetStopCount(PluginType pluginType) const {
        return std::count(mStoppedPlugins.begin(), mStoppedPlugins.end(), pluginType);
    }

    size_t GetSuspendCount(PluginType pluginType) const {
        return std::count(mSuspendedPlugins.begin(), mSuspendedPlugins.end(), pluginType);
    }

    size_t GetResumeCount(PluginType pluginType) const {
        return std::count(mResumedPlugins.begin(), mResumedPlugins.end(), pluginType);
    }

    size_t GetUpdateCount(PluginType pluginType) const {
        return std::count(mUpdatedPlugins.begin(), mUpdatedPlugins.end(), pluginType);
    }

private:
    std::vector<PluginType> mStartedPlugins;
    std::vector<PluginType> mStoppedPlugins;
    std::vector<PluginType> mSuspendedPlugins;
    std::vector<PluginType> mResumedPlugins;
    std::vector<PluginType> mUpdatedPlugins;
};

} // namespace logtail::ebpf
