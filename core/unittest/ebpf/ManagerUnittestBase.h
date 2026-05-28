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
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "common/queue/blockingconcurrentqueue.h"
#include "coolbpf/security/type.h"
#include "ebpf/EBPFAdapter.h"
#include "ebpf/include/export.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/ProcessCacheManager.h"
#include "models/EventPool.h"
#include "unittest/Unittest.h"
#include "unittest/ebpf/MockEBPFAdapter.h"

namespace logtail::ebpf {

// Base test fixture for testing manager config pair operations
class ManagerUnittestBase : public ::testing::Test {
public:
    void SetUp() override {
        mMockEBPFAdapter = std::make_shared<MockEBPFAdapter>();
        mEventQueue = std::make_unique<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>>();
        mEventPool = std::make_unique<EventPool>(true);
        mRetryableEventCache = std::make_unique<RetryableEventCache>();

        // Mock methods are directly implemented to track plugin lifecycle
    }

    void TearDown() override {
        mMockEBPFAdapter->ClearTracking();
        mEventQueue = std::make_unique<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>>();
        mEventPool->Clear();
        mRetryableEventCache->Clear();
    }

    // Test different config names replacement scenario: add config1 -> remove config1 -> add config2 -> remove config2
    void TestDifferentConfigNamesReplacement() {
        auto manager = createAndInitManagerInstance();

        // Add first config
        CollectionPipelineContext ctx1;
        ctx1.SetConfigName("config1");
        auto result = manager->AddOrUpdateConfig(&ctx1, 0, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Remove first config
        result = manager->RemoveConfig("config1");
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 0);

        // Add second config
        CollectionPipelineContext ctx2;
        ctx2.SetConfigName("config2");
        result = manager->AddOrUpdateConfig(&ctx2, 0, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Remove second config
        result = manager->RemoveConfig("config2");
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 0);

        // Get plugin type before destroying manager
        auto pluginType = manager->GetPluginType();

        // Destroy manager to ensure complete lifecycle
        destroyManagerInstance(manager);

        // Verify plugin lifecycle
        verifyPluginPairing(pluginType);
    }

    // Test same config name update scenario: add config -> suspend -> update config and resume -> remove config
    void TestSameConfigNameUpdate() {
        auto manager = createAndInitManagerInstance();
        const std::string configName = "test-config";

        CollectionPipelineContext ctx;
        ctx.SetConfigName(configName);

        // Add config
        auto result = manager->AddOrUpdateConfig(&ctx, 0, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Suspend
        result = manager->Suspend();
        APSARA_TEST_EQUAL(result, 0);

        // Update same config (which will resume)
        result = manager->AddOrUpdateConfig(&ctx, 1, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Remove config
        result = manager->RemoveConfig(configName);
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 0);

        // Get plugin type before destroying manager
        auto pluginType = manager->GetPluginType();
        // Destroy manager to ensure complete lifecycle
        destroyManagerInstance(manager);

        // Verify plugin lifecycle
        verifyPluginPairing(pluginType);
    }

    // Test multiple configs complex scenario (only for NetworkObserverManager):
    // add config1 -> add config2 -> suspend -> update config2 and resume -> remove config2 and resume -> remove config1
    void TestMultipleConfigsComplexScenario() {
        auto manager = createAndInitManagerInstance();

        CollectionPipelineContext ctx1;
        CollectionPipelineContext ctx2;
        ctx1.SetConfigName("config1");
        ctx2.SetConfigName("config2");

        // Add first config
        auto result = manager->AddOrUpdateConfig(&ctx1, 0, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Add second config
        result = manager->AddOrUpdateConfig(&ctx2, 0, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 2);

        // Suspend
        result = manager->Suspend();
        APSARA_TEST_EQUAL(result, 0);

        // Update config2 (which will resume)
        result = manager->AddOrUpdateConfig(&ctx2, 1, nullptr, createTestOptions());
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 2);

        // Remove config2 (and resume)
        result = manager->RemoveConfig("config2");
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 1);

        // Remove config1
        result = manager->RemoveConfig("config1");
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_EQUAL(manager->RegisteredConfigCount(), 0);

        // Get plugin type before destroying manager
        auto pluginType = manager->GetPluginType();

        // Destroy manager to ensure complete lifecycle
        destroyManagerInstance(manager);

        // Verify plugin lifecycle
        verifyPluginPairing(pluginType);
    }

    // Test basic manager lifecycle: init -> add config -> suspend -> resume -> destroy
    void TestBasicConfigUpdate() {
        auto manager = createAndInitManagerInstance();

        // Test add config
        CollectionPipelineContext ctx;
        ctx.SetConfigName("test_config");
        std::variant<SecurityOptions*, ObserverNetworkOption*> options = createTestOptions();

        auto result = manager->AddOrUpdateConfig(&ctx, 0, nullptr, options);
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_TRUE(manager->IsRunning());

        // Test suspend
        result = manager->Suspend();
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_FALSE(manager->IsRunning());

        // Test update (this will automatically resume after suspend)
        result = manager->AddOrUpdateConfig(&ctx, 0, nullptr, options);
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_TRUE(manager->IsRunning());

        // Test remove config
        result = manager->RemoveConfig("test_config");
        APSARA_TEST_EQUAL(result, 0);
        APSARA_TEST_TRUE(manager->IsRunning()); // the flag is untouched

        // Test destroy
        // Get plugin type before destroying manager
        auto pluginType = manager->GetPluginType();
        destroyManagerInstance(manager);

        verifyPluginPairing(pluginType);
    }

protected:
    std::shared_ptr<AbstractManager> createAndInitManagerInstance() {
        auto manager = createManagerInstance();
        APSARA_TEST_TRUE(manager != nullptr);

        // Initialize the NetworkObserverManager after creation
        auto result = manager->Init();
        APSARA_TEST_EQUAL(result, 0);

        return manager;
    }

    void destroyManagerInstance(std::shared_ptr<AbstractManager>& manager) {
        if (manager) {
            // Destroy the manager to trigger plugin cleanup
            manager->Destroy();
            manager.reset();
        }
    }

    // Factory method to create manager instances - override in derived classes
    virtual std::shared_ptr<AbstractManager> createManagerInstance() = 0;

    // Factory method to create test options - override in derived classes
    virtual std::variant<SecurityOptions*, ObserverNetworkOption*> createTestOptions() = 0;

    // Verify plugin lifecycle pairing
    void verifyPluginPairing(PluginType pluginType) {
        // For most managers, we expect start/stop to be paired
        mMockEBPFAdapter->AssertStartStopPaired(pluginType);
        mMockEBPFAdapter->AssertSuspendResumePaired(pluginType);
    }

    // Get EBPFAdapter reference for ProcessCacheManager constructor
    std::shared_ptr<EBPFAdapter>& getEbpfAdapterRef() {
        mEBPFAdapterForProcessCache = std::static_pointer_cast<EBPFAdapter>(mMockEBPFAdapter);
        return mEBPFAdapterForProcessCache;
    }

    std::shared_ptr<EBPFAdapter> mEBPFAdapterForProcessCache;

    std::shared_ptr<MockEBPFAdapter> mMockEBPFAdapter;
    std::unique_ptr<moodycamel::BlockingConcurrentQueue<std::shared_ptr<CommonEvent>>> mEventQueue;
    std::unique_ptr<EventPool> mEventPool;
    std::unique_ptr<RetryableEventCache> mRetryableEventCache;
};

// Test fixture for managers that need ProcessCacheManager
class ManagerUnittestWithProcessCacheManager : public ManagerUnittestBase {
protected:
    void SetUp() override {
        ManagerUnittestBase::SetUp();

        // Create ProcessCacheManager for managers that need it
        DynamicMetricLabels dynamicLabels;
        WriteMetrics::GetInstance()->CreateMetricsRecordRef(
            mProcessCacheRef,
            MetricCategory::METRIC_CATEGORY_RUNNER,
            {{METRIC_LABEL_KEY_RUNNER_NAME, METRIC_LABEL_VALUE_RUNNER_NAME_EBPF_SERVER}},
            std::move(dynamicLabels));
        auto pollProcessEventsTotal = mProcessCacheRef.CreateCounter(METRIC_RUNNER_EBPF_POLL_PROCESS_EVENTS_TOTAL);
        auto lossProcessEventsTotal = mProcessCacheRef.CreateCounter(METRIC_RUNNER_EBPF_LOSS_PROCESS_EVENTS_TOTAL);
        auto processCacheMissTotal = mProcessCacheRef.CreateCounter(METRIC_RUNNER_EBPF_PROCESS_CACHE_MISS_TOTAL);
        auto processCacheSize = mProcessCacheRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_CACHE_SIZE);
        auto processDataMapSize = mProcessCacheRef.CreateIntGauge(METRIC_RUNNER_EBPF_PROCESS_DATA_MAP_SIZE);
        auto retryableEventCacheSize = mProcessCacheRef.CreateIntGauge(METRIC_RUNNER_EBPF_RETRYABLE_EVENT_CACHE_SIZE);
        WriteMetrics::GetInstance()->CommitMetricsRecordRef(mProcessCacheRef);

        mProcessCacheManager = std::make_shared<ProcessCacheManager>(getEbpfAdapterRef(),
                                                                     "test_host",
                                                                     "/",
                                                                     *mEventQueue,
                                                                     pollProcessEventsTotal,
                                                                     lossProcessEventsTotal,
                                                                     processCacheMissTotal,
                                                                     processCacheSize,
                                                                     processDataMapSize,
                                                                     *mRetryableEventCache);
    }

    void TearDown() override { ManagerUnittestBase::TearDown(); }

protected:
    MetricsRecordRef mProcessCacheRef;
    std::shared_ptr<ProcessCacheManager> mProcessCacheManager;
};

// Test fixture specifically for security managers
class SecurityManagerUnittestBase : public ManagerUnittestWithProcessCacheManager {
protected:
    std::variant<SecurityOptions*, ObserverNetworkOption*> createTestOptions() override {
        static SecurityOptions options;
        options.mOptionList.push_back(SecurityOption{{"test_option"}, std::monostate{}});
        return &options;
    }
};

// Test fixture specifically for network observer managers
class NetworkObserverManagerUnittestBase : public ManagerUnittestWithProcessCacheManager {
protected:
    std::variant<SecurityOptions*, ObserverNetworkOption*> createTestOptions() override {
        static ObserverNetworkOption options;
        options.mApmConfig = {.mWorkspace = "test-ws",
                              .mAppName = "test-app",
                              .mAppId = "test-app-id",
                              .mLanguage = "",
                              .mServiceId = "test-service-id"};
        options.mL4Config.mEnable = true;
        options.mL7Config.mEnable = true;
        options.mSelectors = {{"ns", "kind", "name"}};
        return &options;
    }
};


} // namespace logtail::ebpf
