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

#include "forward/GrpcRunner.h"

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include "common/Flags.h"
#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "logger/Logger.h"
#ifdef APSARA_UNIT_TEST_MAIN
#include "unittest/forward/MockServiceImpl.h"
#endif

DEFINE_FLAG_INT32(grpc_server_stop_timeout, "grpc server stop timeout, second", 3);

namespace logtail {

void GrpcInputRunner::Init() {
    if (mIsStarted.exchange(true)) {
        return;
    }
    LOG_INFO(sLogger, ("GrpcInputRunner", "Start"));
}

void GrpcInputRunner::Stop() {
    if (!mIsStarted.exchange(false)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mListenInputsMutex);
        for (auto& it : mListenInputs) {
            if (it.second.mServer) {
                ShutdownGrpcServer(it.second.mServer.get(), &it.second.mInFlightCnt);
            }
        }
        mListenInputs.clear();
    }
    LOG_INFO(sLogger, ("GrpcInputRunner", "Stop"));
}

bool GrpcInputRunner::HasRegisteredPlugins() const {
    std::lock_guard<std::mutex> lock(mListenInputsMutex);
    return !mListenInputs.empty();
}

template <typename T>
bool GrpcInputRunner::UpdateListenInput(const std::string& configName,
                                        const std::string& address,
                                        const Json::Value& config) {
    std::lock_guard<std::mutex> lock(mListenInputsMutex);
    auto it = mListenInputs.find(address);
    if (it != mListenInputs.end()) {
        if (it->second.mServer == nullptr || it->second.mService == nullptr) {
            // should never happen
            LOG_ERROR(
                sLogger,
                ("GrpcInputRunner", "address exists but server or service is not initialized, should never happen")(
                    "address", address)("service", T::Name()));
            return false;
        } else if (it->second.mService->Name() != T::Name()) {
            // Address already exists, check if the service type matches
            LOG_ERROR(sLogger,
                      ("GrpcInputRunner", "address already exists with a different service type")("address", address)(
                          "existing service", it->second.mService->Name())("new service", T::Name()));
            return false;
        } else {
            if (!it->second.mService->Update(configName, config)) {
                return false;
            }
        }
    } else {
        GrpcListenInput input;
        auto result = mListenInputs.insert(std::make_pair(address, std::move(input)));
        if (!result.second) {
            LOG_ERROR(sLogger,
                      ("GrpcInputRunner", "failed to insert new address into listen inputs")("address", address));
            return false;
        }
        LOG_INFO(
            sLogger,
            ("GrpcInputRunner", "new address inserted into listen inputs")("address", address)("service", T::Name()));
        it = result.first;
        std::unique_ptr<T> service = std::make_unique<T>();
        if (!service->Update(configName, config)) {
            LOG_ERROR(sLogger,
                      ("GrpcInputRunner", "failed to update service configuration")("service", T::Name())(
                          "config", config.toStyledString()));
            return false;
        }
        grpc::ServerBuilder builder;
        std::vector<std::unique_ptr<grpc::experimental::ServerInterceptorFactoryInterface>> factories;
        factories.emplace_back(std::make_unique<InFlightCountInterceptorFactory>(&it->second.mInFlightCnt));
        builder.experimental().SetInterceptorCreators(std::move(factories));

        builder.AddListeningPort(address, grpc::InsecureServerCredentials());
        // TODO: multi-service server is complex and lacks isolation, only support one service per server for now
        builder.RegisterService(service.get());
        auto server = builder.BuildAndStart();
        if (!server) {
            LOG_ERROR(sLogger,
                      ("GrpcInputRunner", "failed to start gRPC server")("address", address)("service", T::Name())(
                          "config name", configName));
            return false;
        }
        it->second.mServer = std::move(server);
        it->second.mService = std::move(service);
    }

    it->second.mReferenceCount++;
    return true;
}

template <typename T>
bool GrpcInputRunner::RemoveListenInput(const std::string& address, const std::string& configName) {
    std::lock_guard<std::mutex> lock(mListenInputsMutex);
    auto it = mListenInputs.find(address);
    if (it != mListenInputs.end()) {
        if (it->second.mService && it->second.mService->Remove(configName)) {
            it->second.mReferenceCount--;
        }
    } else {
        LOG_ERROR(sLogger,
                  ("GrpcInputRunner", "listen input not found")("address", address)("config name", configName));
        return false;
    }
    if (it->second.mReferenceCount <= 0) {
        if (!ShutdownGrpcServer(it->second.mServer.get(), &it->second.mInFlightCnt)) {
            LOG_ERROR(sLogger, ("GrpcInputRunner", "failed to shutdown gRPC server gracefully")("address", address));
        }
        mListenInputs.erase(it);
        LOG_INFO(sLogger, ("GrpcInputRunner", "removed listen input")("address", address));
    }
    return true;
}

bool GrpcInputRunner::ShutdownGrpcServer(grpc::Server* server, std::atomic_int* inFlightCnt) {
    if (server) {
        auto shutdownStartTime = std::chrono::system_clock::now();
        auto deadline = shutdownStartTime + std::chrono::seconds(INT32_FLAG(grpc_server_stop_timeout));
        server->Shutdown(deadline);
        // Shutdown will release the server resources and new server can start
        // but cannot forcefully stop the in-flight requests.
        while (inFlightCnt->load() > 0) {
            auto now = std::chrono::system_clock::now();
            if (now >= deadline) {
                return false; // Timeout, cannot guarantee all requests are completed
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return true;
    }
    return false;
}

template bool GrpcInputRunner::UpdateListenInput<LoongSuiteForwardServiceImpl>(const std::string&,
                                                                               const std::string&,
                                                                               const Json::Value&);
template bool GrpcInputRunner::RemoveListenInput<LoongSuiteForwardServiceImpl>(const std::string&, const std::string&);

#ifdef APSARA_UNIT_TEST_MAIN
template bool
GrpcInputRunner::UpdateListenInput<MockServiceImpl>(const std::string&, const std::string&, const Json::Value&);
template bool GrpcInputRunner::RemoveListenInput<MockServiceImpl>(const std::string&, const std::string&);
#endif

} // namespace logtail
