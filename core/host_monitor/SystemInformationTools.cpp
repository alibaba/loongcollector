/*
 * Copyright 2025 iLogtail Authors
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

#include "host_monitor/SystemInformationTools.h"

#include <algorithm>
#include <boost/process.hpp>
#include <boost/system.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "_thirdparty/DCGM/dcgmlib/dcgm_agent.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_api_export.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_errors.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_fields.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_helpers.h"
#include "_thirdparty/DCGM/dcgmlib/dcgm_structs.h"
#include "app_config/AppConfig.h"
#include "common/DynamicLibHelper.h"
#include "common/FileSystemUtil.h"
#include "constants/EntityConstants.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"


namespace logtail {

bool CheckGPUDevice() {
#if defined(__linux__)
    if (!std::filesystem::exists(NVIDIACTL)) {
        LOG_WARNING(sLogger, ("GPU check failed", "NVIDIA control device not found")("device", NVIDIACTL));
        return false;
    }

    auto binary_path = boost::process::search_path(NVSMI);
    if (binary_path.empty()) {
        LOG_WARNING(sLogger, ("GPU check failed", "nvidia-smi not found in PATH"));
        return false;
    }

    boost::process::ipstream pipe_stream;
    std::string cmd = "nvidia-smi -L";
    std::error_code ec;

    int exit_code = boost::process::system(cmd, boost::process::std_out > pipe_stream, ec);
    if (ec || exit_code != 0) {
        LOG_WARNING(sLogger,
                    ("GPU check failed",
                     "nvidia-smi execution error")("command", cmd)("error", ec.message())("exit_code", exit_code));
        return false;
    }

    std::string line;
    bool has_output = false;
    while (std::getline(pipe_stream, line)) {
        if (!line.empty()) {
            has_output = true;
            LOG_DEBUG(sLogger, ("GPU detected", line));
        }
    }

    if (!has_output) {
        LOG_WARNING(sLogger, ("GPU check failed", "no GPU devices found"));
        return false;
    }

    LOG_INFO(sLogger, ("GPU check successful", "GPU monitoring available"));
    return true;
#elif defined(_MSC_VER)
    // GPU detection implementation for Windows platform
    // On Windows, GPU detection can be implemented via WMI or DirectX API
    // Currently returns false, indicating GPU monitoring is not supported on Windows yet
    LOG_INFO(sLogger, ("GPU check", "Windows platform GPU monitoring not implemented yet"));
    return false;
#else
    LOG_INFO(sLogger, ("GPU check", "Unsupported platform for GPU monitoring"));
    return false;
#endif
}

template <typename R, typename... Args>
FunctionProxy<R(Args...)>::FunctionProxy(DynamicLibLoader& libLoader, const char* name) : mName(name) {
    std::string error;
    void* function = libLoader.LoadMethod(name, error);
    if (!error.empty()) {
        LOG_ERROR(sLogger, ("DCGM Library load function failed", name)("error", error));
        return;
    }
    mFunction = reinterpret_cast<R (*)(Args...)>(function);
}

template <typename R, typename... Args>
std::optional<R> FunctionProxy<R(Args...)>::operator()(Args... args) const {
    if (!IsBound())
        return std::nullopt;
    try {
        if constexpr (std::is_same_v<R, void>) {
            mFunction(std::forward<Args>(args)...);
            return std::make_optional(true);
        } else {
            return mFunction(std::forward<Args>(args)...);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(sLogger, ("failed to call function", e.what()));
        return std::nullopt;
    }
}

template <typename R, typename... Args>
bool FunctionProxy<R(Args...)>::IsBound() const {
    return static_cast<bool>(mFunction);
}

DCGMCollector::DCGMCollector(const std::string& dcgmLib) {
    std::string error;
    if (!mDcgmLibLoader.LoadDynLib(dcgmLib, error, AppConfig::GetInstance()->GetWorkingDir())) {
        LOG_ERROR(sLogger, ("DCGM library load failed", error));
        return;
    }
    InitializeFunctionProxies();
}

DCGMCollector::~DCGMCollector() {
    CleanupResources(ResourceState::UNINITIALIZED);
    LOG_INFO(sLogger, ("DCGM collector shutdown completed", "all resources cleaned"));

    void* libHandle = mDcgmLibLoader.Release();
    if (libHandle) {
        DynamicLibLoader::CloseLib(libHandle);
        LOG_INFO(sLogger, ("DCGM library closed", "dynamic library unloaded"));
    }
}

bool DCGMCollector::Initialize(const FieldMap& fieldMap) {
    mFieldMap = fieldMap;
    int numFields = fieldMap.int64Fields.size() + fieldMap.stringFields.size() + fieldMap.doubleFields.size();
    if (numFields == 0) {
        LOG_ERROR(sLogger, ("DCGM initialization failed", "no fields to watch"));
        return false;
    }
    if (numFields > DCGM_MAX_FIELD_IDS_PER_FIELD_GROUP) {
        LOG_ERROR(sLogger,
                  ("DCGM initialization failed",
                   "too many fields")("count", numFields)("max", DCGM_MAX_FIELD_IDS_PER_FIELD_GROUP));
        return false;
    }

    std::vector<unsigned short> fieldsToWatch;
    fieldsToWatch.reserve(numFields);
    for (const auto& pair : fieldMap.int64Fields) {
        fieldsToWatch.push_back(pair.first);
    }
    for (const auto& pair : fieldMap.doubleFields) {
        fieldsToWatch.push_back(pair.first);
    }
    for (const auto& pair : fieldMap.stringFields) {
        fieldsToWatch.push_back(pair.first);
    }

    if (!TransitionToState(ResourceState::INIT_CALLED)) {
        return false;
    }
    if (!TransitionToState(ResourceState::EMBEDDED_STARTED)) {
        return false;
    }
    if (!TransitionToState(ResourceState::FIELD_GROUP_CREATED, fieldsToWatch)) {
        return false;
    }
    if (!TransitionToState(ResourceState::WATCHING_FIELDS)) {
        return false;
    }
    if (!TransitionToState(ResourceState::FULLY_INITIALIZED)) {
        return false;
    }

    LOG_INFO(sLogger, ("DCGM initialization successful", "manual mode")("field_count", numFields));
    return true;
}

bool DCGMCollector::Collect(GPUInformation& outData) {
    if (!IsFullyInitialized()) {
        LOG_ERROR(sLogger, ("GPU data collection failed", "DCGM not fully initialized"));
        return false;
    }

    if (!DcgmCall(mDcgmUpdateAllFields, mDcgmHandle, 1)) { // wait for the update loop to complete before return
        LOG_ERROR(sLogger, ("GPU data collection failed", "update all fields failed"));
        return false;
    }

    outData.stats.clear();
    CallbackContext context = {&outData, &mFieldMap, {}, {}};

    if (!GetLatestValues(mDcgmHandle, DCGM_GROUP_ALL_GPUS, mFieldGroupId, &context)) {
        LOG_ERROR(sLogger, ("GPU data collection failed", "get latest values failed"));
        return false;
    }

    // Remove all GPUs that were marked as invalid during callbacks (from back to front to maintain index validity)
    if (!context.invalidGpuIds.empty()) {
        std::vector<size_t> indicesToRemove;
        for (unsigned int invalidGpuId : context.invalidGpuIds) {
            auto it = context.gpuIdToIndex.find(invalidGpuId);
            if (it != context.gpuIdToIndex.end()) {
                indicesToRemove.push_back(it->second);
            }
        }

        std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<size_t>());
        for (size_t index : indicesToRemove) {
            if (index < outData.stats.size()) {
                outData.stats.erase(outData.stats.begin() + index);
                LOG_DEBUG(sLogger,
                          ("DCGM invalid GPU data removed", "GPU data removed after collection")("index", index));
            }
        }

        LOG_WARNING(sLogger, ("DCGM invalid GPUs removed", "removed count")("count", context.invalidGpuIds.size()));
    }

    LOG_DEBUG(sLogger, ("GPU data collection successful", "data retrieved")("gpu_count", outData.stats.size()));
    return true;
}

bool DCGMCollector::IsLibraryLoaded() const {
    return mDcgmInit.IsBound() && mDcgmShutdown.IsBound() && mDcgmErrorString.IsBound()
        && mDcgmStartEmbeddedV1.IsBound() && mDcgmStartEmbeddedV2.IsBound() && mDcgmStopEmbedded.IsBound()
        && mDcgmFieldGroupCreate.IsBound() && mDcgmFieldGroupDestroy.IsBound() && mDcgmWatchFields.IsBound()
        && mDcgmUnwatchFields.IsBound() && mDcgmUpdateAllFields.IsBound() && mDcgmGetLatestValuesV1.IsBound()
        && mDcgmGetLatestValuesV2.IsBound();
}

bool DCGMCollector::IsFullyInitialized() const {
    return mResourceState.load() == ResourceState::FULLY_INITIALIZED;
}

bool DCGMCollector::CanInitialize() const {
    return IsLibraryLoaded() && mResourceState.load() == ResourceState::UNINITIALIZED;
}

bool DCGMCollector::TransitionToState(ResourceState newState, const std::vector<unsigned short>& fieldsToWatch) {
    ResourceState currentState = mResourceState.load();

    switch (newState) {
        case ResourceState::INIT_CALLED:
            if (currentState != ResourceState::UNINITIALIZED)
                return false;
            if (!DcgmCall(mDcgmInit)) {
                LOG_ERROR(sLogger, ("DCGM initialization failed", "dcgmInit call failed"));
                return false;
            }
            mResourceState.store(ResourceState::INIT_CALLED);
            return true;

        case ResourceState::EMBEDDED_STARTED:
            if (currentState != ResourceState::INIT_CALLED)
                return false;
            if (!StartEmbedded(DCGM_OPERATION_MODE_MANUAL, &mDcgmHandle)) {
                LOG_ERROR(sLogger, ("DCGM initialization failed", "start embedded engine failed"));
                return false;
            }
            mResourceState.store(ResourceState::EMBEDDED_STARTED);
            return true;

        case ResourceState::FIELD_GROUP_CREATED: {
            if (currentState != ResourceState::EMBEDDED_STARTED)
                return false;
            if (fieldsToWatch.empty())
                return false;

            std::string fieldGroupName = "LOONGCOLLECTOR_FIELD_GROUP";
            int numFields = fieldsToWatch.size();
            if (!DcgmCall(mDcgmFieldGroupCreate,
                          mDcgmHandle,
                          numFields,
                          fieldsToWatch.data(),
                          fieldGroupName.c_str(),
                          &mFieldGroupId)) {
                LOG_ERROR(sLogger, ("DCGM initialization failed", "field group creation failed"));
                return false;
            }
            mResourceState.store(ResourceState::FIELD_GROUP_CREATED);
            return true;
        }

        case ResourceState::WATCHING_FIELDS: {
            if (currentState != ResourceState::FIELD_GROUP_CREATED)
                return false;

            mWatchedGroupId = DCGM_GROUP_ALL_GPUS;
            mUpdateFreq = 10000; // Invalid value in manual mode
            mMaxKeepAge = 0.0;
            mMaxKeepSamples = 0;

            if (!DcgmCall(mDcgmWatchFields,
                          mDcgmHandle,
                          mWatchedGroupId,
                          mFieldGroupId,
                          mUpdateFreq,
                          mMaxKeepAge,
                          mMaxKeepSamples)) {
                LOG_ERROR(sLogger, ("DCGM initialization failed", "watch fields failed"));
                return false;
            }
            mResourceState.store(ResourceState::WATCHING_FIELDS);
            return true;
        }

        case ResourceState::FULLY_INITIALIZED: {
            if (currentState != ResourceState::WATCHING_FIELDS)
                return false;
            mResourceState.store(ResourceState::FULLY_INITIALIZED);
            return true;
        }

        case ResourceState::UNINITIALIZED:
            return currentState == ResourceState::UNINITIALIZED;

        default:
            return false;
    }
    return false;
}

void DCGMCollector::CleanupResources(ResourceState targetState) {
    ResourceState currentState = mResourceState.load();

    while (currentState != targetState && currentState != ResourceState::UNINITIALIZED) {
        switch (currentState) {
            case ResourceState::FULLY_INITIALIZED:
            case ResourceState::WATCHING_FIELDS:
                if (mFieldGroupId != 0 && mDcgmUnwatchFields.IsBound()) {
                    DcgmCall(mDcgmUnwatchFields, mDcgmHandle, mWatchedGroupId, mFieldGroupId);
                }
                currentState = ResourceState::FIELD_GROUP_CREATED;
                mResourceState.store(currentState);
                break;

            case ResourceState::FIELD_GROUP_CREATED:
                if (mFieldGroupId != 0 && mDcgmFieldGroupDestroy.IsBound()) {
                    DcgmCall(mDcgmFieldGroupDestroy, mDcgmHandle, mFieldGroupId);
                    mFieldGroupId = 0;
                }
                currentState = ResourceState::EMBEDDED_STARTED;
                mResourceState.store(currentState);
                break;

            case ResourceState::EMBEDDED_STARTED:
                if (mDcgmHandle != 0 && mDcgmStopEmbedded.IsBound()) {
                    DcgmCall(mDcgmStopEmbedded, mDcgmHandle);
                    mDcgmHandle = 0;
                }
                currentState = ResourceState::INIT_CALLED;
                mResourceState.store(currentState);
                break;

            case ResourceState::INIT_CALLED:
                if (mDcgmShutdown.IsBound()) {
                    DcgmCall(mDcgmShutdown);
                }
                currentState = ResourceState::UNINITIALIZED;
                mResourceState.store(currentState);
                break;

            default:
                currentState = ResourceState::UNINITIALIZED;
                mResourceState.store(currentState);
                break;
        }
    }

    if (targetState == ResourceState::UNINITIALIZED) {
        LOG_INFO(sLogger, ("DCGM shutdown completed", "all resources cleaned"));
    }
}

bool DCGMCollector::GetLatestValues(dcgmHandle_t pDcgmHandle,
                                    dcgmGpuGrp_t groupId,
                                    dcgmFieldGrp_t fieldGroupId,
                                    void* userData) {
    if (mDcgmGetLatestValuesV2.IsBound()) {
        return DcgmCall(
            mDcgmGetLatestValuesV2, pDcgmHandle, groupId, fieldGroupId, &DCGMCollector::DcgmDataCallbackV2, userData);
    }
    if (mDcgmGetLatestValuesV1.IsBound()) {
        return DcgmCall(
            mDcgmGetLatestValuesV1, pDcgmHandle, groupId, fieldGroupId, &DCGMCollector::DcgmDataCallbackV1, userData);
    }
    return false;
}

bool DCGMCollector::StartEmbedded(dcgmOperationMode_t opMode, dcgmHandle_t* dcgmHandle) {
    dcgmStartEmbeddedV2Params_v1 params = {
        .version = dcgmStartEmbeddedV2Params_version1,
        .opMode = opMode,
        .dcgmHandle = *dcgmHandle,
        .logFile = nullptr,
        .severity = DcgmLoggingSeverityError,
        .denyListCount = 0,
    };

    if (mDcgmStartEmbeddedV2.IsBound()) {
        if (DcgmCall(mDcgmStartEmbeddedV2, &params)) {
            *dcgmHandle = params.dcgmHandle;
            return true;
        }
        return false;
    }
    if (mDcgmStartEmbeddedV1.IsBound()) {
        return DcgmCall(mDcgmStartEmbeddedV1, opMode, dcgmHandle);
    }
    return false;
}

template <typename Func, typename... Args>
bool DCGMCollector::DcgmCall(const Func& funcToCall, Args&&... args) {
    auto result = funcToCall(std::forward<Args>(args)...);
    if (!result.has_value()) {
        LOG_ERROR(sLogger, ("DCGM API call failed", "function not bound or library unloaded"));
        return false;
    }
    dcgmReturn_t ret = result.value();
    if (ret != DCGM_ST_OK) {
        std::string errMsg = "N/A";
        if (auto errStrOpt = mDcgmErrorString(ret)) {
            if (errStrOpt.value() != nullptr)
                errMsg = errStrOpt.value();
        }
        LOG_ERROR(sLogger, ("DCGM API call failed", errMsg)("error_code", ret));
        return false;
    }
    return true;
}

void DCGMCollector::InitializeFunctionProxies() {
    mDcgmInit = FunctionProxy<decltype(dcgmInit)>(mDcgmLibLoader, "dcgmInit");
    mDcgmShutdown = FunctionProxy<decltype(dcgmShutdown)>(mDcgmLibLoader, "dcgmShutdown");
    mDcgmErrorString = FunctionProxy<decltype(errorString)>(mDcgmLibLoader, "errorString");
    mDcgmStartEmbeddedV1 = FunctionProxy<decltype(dcgmStartEmbedded)>(mDcgmLibLoader, "dcgmStartEmbedded");
    mDcgmStartEmbeddedV2 = FunctionProxy<decltype(dcgmStartEmbedded_v2)>(mDcgmLibLoader, "dcgmStartEmbedded_v2");
    mDcgmStopEmbedded = FunctionProxy<decltype(dcgmStopEmbedded)>(mDcgmLibLoader, "dcgmStopEmbedded");
    mDcgmFieldGroupCreate = FunctionProxy<decltype(dcgmFieldGroupCreate)>(mDcgmLibLoader, "dcgmFieldGroupCreate");
    mDcgmFieldGroupDestroy = FunctionProxy<decltype(dcgmFieldGroupDestroy)>(mDcgmLibLoader, "dcgmFieldGroupDestroy");
    mDcgmWatchFields = FunctionProxy<decltype(dcgmWatchFields)>(mDcgmLibLoader, "dcgmWatchFields");
    mDcgmUnwatchFields = FunctionProxy<decltype(dcgmUnwatchFields)>(mDcgmLibLoader, "dcgmUnwatchFields");
    mDcgmUpdateAllFields = FunctionProxy<decltype(dcgmUpdateAllFields)>(mDcgmLibLoader, "dcgmUpdateAllFields");
    mDcgmGetLatestValuesV1 = FunctionProxy<decltype(dcgmGetLatestValues)>(mDcgmLibLoader, "dcgmGetLatestValues");
    mDcgmGetLatestValuesV2 = FunctionProxy<decltype(dcgmGetLatestValues_v2)>(mDcgmLibLoader, "dcgmGetLatestValues_v2");
}

int DCGMCollector::DcgmDataCallbackV1(unsigned int gpuId, dcgmFieldValue_v1* values, int numValues, void* userData) {
    auto* context = static_cast<CallbackContext*>(userData);
    if (!context || !context->gpuInfo || !context->fieldMap)
        return 0;

    auto it = context->gpuIdToIndex.find(gpuId);
    if (it == context->gpuIdToIndex.end()) {
        size_t newIndex = context->gpuInfo->stats.size();
        context->gpuInfo->stats.emplace_back();
        context->gpuIdToIndex[gpuId] = newIndex;
        it = context->gpuIdToIndex.find(gpuId);
    }
    auto& currentStat = context->gpuInfo->stats[it->second];

    bool hasInvalidValue = false;
    for (int i = 0; i < numValues; ++i) {
        const auto& fieldValue = values[i];
        if (fieldValue.status != DCGM_ST_OK) {
            hasInvalidValue = true;
            LOG_DEBUG(sLogger,
                      ("DCGM data callback V1 invalid value detected",
                       "status not OK")("gpu_id", gpuId)("field_id", fieldValue.fieldId)("status", fieldValue.status));
            break;
        }

        switch (fieldValue.fieldType) {
            case DCGM_FT_INT64: {
                if (DCGM_INT64_IS_BLANK(fieldValue.value.i64)) {
                    hasInvalidValue = true;
                    LOG_DEBUG(sLogger,
                              ("DCGM data callback V1 invalid value detected",
                               "blank int64 value")("gpu_id", gpuId)("field_id", fieldValue.fieldId));
                    break;
                }
                auto it = context->fieldMap->int64Fields.find(fieldValue.fieldId);
                if (it != context->fieldMap->int64Fields.end())
                    currentStat.*(it->second) = fieldValue.value.i64;
                break;
            }
            case DCGM_FT_STRING: {
                if (DCGM_STR_IS_BLANK(fieldValue.value.str)) {
                    hasInvalidValue = true;
                    LOG_DEBUG(sLogger,
                              ("DCGM data callback V1 invalid value detected",
                               "blank string value")("gpu_id", gpuId)("field_id", fieldValue.fieldId));
                    break;
                }
                auto it = context->fieldMap->stringFields.find(fieldValue.fieldId);
                if (it != context->fieldMap->stringFields.end())
                    currentStat.*(it->second) = fieldValue.value.str;
                break;
            }
            case DCGM_FT_DOUBLE: {
                if (DCGM_FP64_IS_BLANK(fieldValue.value.dbl)) {
                    hasInvalidValue = true;
                    LOG_DEBUG(sLogger,
                              ("DCGM data callback V1 invalid value detected",
                               "blank double value")("gpu_id", gpuId)("field_id", fieldValue.fieldId));
                    break;
                }
                auto it = context->fieldMap->doubleFields.find(fieldValue.fieldId);
                if (it != context->fieldMap->doubleFields.end())
                    currentStat.*(it->second) = fieldValue.value.dbl;
                break;
            }
        }
        if (hasInvalidValue)
            break;
    }

    if (hasInvalidValue) {
        // Mark this GPU as invalid, will be removed after all callbacks complete
        context->invalidGpuIds.insert(gpuId);
        LOG_WARNING(sLogger,
                    ("DCGM data callback V1 invalid value detected", "GPU marked as invalid")("gpu_id", gpuId));
        return 0;
    }
    return 0;
}

int DCGMCollector::DcgmDataCallbackV2(dcgm_field_entity_group_t entityGroupId,
                                      dcgm_field_eid_t entityId,
                                      dcgmFieldValue_v1* values,
                                      int numValues,
                                      void* userData) {
    // we only request all GPU entity group data
    if (entityGroupId != DCGM_FE_GPU) {
        return -1;
    }
    return DcgmDataCallbackV1(entityId, values, numValues, userData);
}

} // namespace logtail
