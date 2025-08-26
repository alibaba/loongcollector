/*
 * Copyright 2024 iLogtail Authors
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

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "_thirdparty/DCGM/dcgmlib/dcgm_agent.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_fields.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_structs.h"
#include "common/DynamicLibHelper.h"
#include "host_monitor/SystemInterface.h"

namespace logtail {

bool CheckGPUDeveice();

template <typename Signature>
class FunctionProxy;

template <typename R, typename... Args>
class FunctionProxy<R(Args...)> {
public:
    FunctionProxy() = default;
    FunctionProxy(DynamicLibLoader& libLoader, const char* name);

    std::optional<R> operator()(Args... args) const;
    bool IsBound() const;

private:
    std::string mName;
    std::function<R(Args...)> mFunction;
};

class DCGMCollector {
public:
    explicit DCGMCollector(const std::string& dcgmLib);
    ~DCGMCollector();

    bool Initialize(const FieldMap& fieldMap);
    bool Collect(GPUInformation& outData);

    bool IsLibraryLoaded() const;
    bool IsFullyInitialized() const;
    bool CanInitialize() const;

private:
    enum class ResourceState {
        UNINITIALIZED,
        INIT_CALLED,
        EMBEDDED_STARTED,
        FIELD_GROUP_CREATED,
        WATCHING_FIELDS,
        FULLY_INITIALIZED
    };

    struct CallbackContext {
        GPUInformation* gpuInfo;
        const FieldMap* fieldMap;
        std::unordered_map<unsigned int, size_t> gpuIdToIndex;
    };

    bool TransitionToState(ResourceState newState, const std::vector<unsigned short>& fieldsToWatch = {});
    void CleanupResources(ResourceState targetState = ResourceState::UNINITIALIZED);
    bool GetLatestValues(dcgmHandle_t pDcgmHandle, dcgmGpuGrp_t groupId, dcgmFieldGrp_t fieldGroupId, void* userData);
    bool StartEmbedded(dcgmOperationMode_t opMode, dcgmHandle_t* dcgmHandle);

    template <typename Func, typename... Args>
    bool DcgmCall(const Func& funcToCall, Args&&... args);

    void InitializeFunctionProxies();

    static int DcgmDataCallbackV1(unsigned int gpuId, dcgmFieldValue_v1* values, int numValues, void* userData);
    static int DcgmDataCallbackV2(dcgm_field_entity_group_t entityGroupId,
                                  dcgm_field_eid_t entityId,
                                  dcgmFieldValue_v1* values,
                                  int numValues,
                                  void* userData);

    std::atomic<ResourceState> mResourceState{ResourceState::UNINITIALIZED};
    DynamicLibLoader mDcgmLibLoader;
    dcgmHandle_t mDcgmHandle = 0;
    dcgmFieldGrp_t mFieldGroupId = 0;
    FieldMap mFieldMap;
    dcgmGpuGrp_t mWatchedGroupId = DCGM_GROUP_ALL_GPUS;
    long long mUpdateFreq = 0;
    double mMaxKeepAge = 0.0;
    int mMaxKeepSamples = 0;

    FunctionProxy<decltype(dcgmInit)> mDcgmInit;
    FunctionProxy<decltype(dcgmShutdown)> mDcgmShutdown;
    FunctionProxy<decltype(errorString)> mDcgmErrorString;
    FunctionProxy<decltype(dcgmStartEmbedded)> mDcgmStartEmbeddedV1;
    FunctionProxy<decltype(dcgmStartEmbedded_v2)> mDcgmStartEmbeddedV2;
    FunctionProxy<decltype(dcgmStopEmbedded)> mDcgmStopEmbedded;
    FunctionProxy<decltype(dcgmFieldGroupCreate)> mDcgmFieldGroupCreate;
    FunctionProxy<decltype(dcgmFieldGroupDestroy)> mDcgmFieldGroupDestroy;
    FunctionProxy<decltype(dcgmWatchFields)> mDcgmWatchFields;
    FunctionProxy<decltype(dcgmUnwatchFields)> mDcgmUnwatchFields;
    FunctionProxy<decltype(dcgmUpdateAllFields)> mDcgmUpdateAllFields;
    FunctionProxy<decltype(dcgmGetLatestValues)> mDcgmGetLatestValuesV1;
    FunctionProxy<decltype(dcgmGetLatestValues_v2)> mDcgmGetLatestValuesV2;
};

} // namespace logtail
