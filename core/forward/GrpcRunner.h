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

#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "json/value.h"

#include "forward/loongsuite/LoongSuiteForwardService.h"
#include "grpcpp/server.h"
#include "grpcpp/support/interceptor.h"
#include "grpcpp/support/server_interceptor.h"
#include "runner/InputRunner.h"

namespace logtail {

struct GrpcListenInput {
    std::unique_ptr<grpc::Server> mServer;
    std::unique_ptr<BaseService> mService;
    std::atomic_int mInFlightCnt = 0;
    size_t mReferenceCount = 0;

    GrpcListenInput() = default;
    GrpcListenInput(GrpcListenInput&& other) noexcept
        : mServer(std::move(other.mServer)),
          mService(std::move(other.mService)),
          mInFlightCnt(other.mInFlightCnt.load()),
          mReferenceCount(other.mReferenceCount) {}
    GrpcListenInput& operator=(GrpcListenInput&& other) noexcept {
        if (this != &other) {
            mServer = std::move(other.mServer);
            mService = std::move(other.mService);
            mInFlightCnt.store(other.mInFlightCnt.load());
            mReferenceCount = other.mReferenceCount;
        }
        return *this;
    }

    GrpcListenInput(const GrpcListenInput&) = delete;
    GrpcListenInput& operator=(const GrpcListenInput&) = delete;
};

class InFlightCountInterceptor : public grpc::experimental::Interceptor {
public:
    explicit InFlightCountInterceptor(std::atomic_int* inFlightCnt) : mInFlightCnt(inFlightCnt) {}
    void Intercept(grpc::experimental::InterceptorBatchMethods* methods) override {
        if (methods->QueryInterceptionHookPoint(
                grpc::experimental::InterceptionHookPoints::POST_RECV_INITIAL_METADATA)) {
            std::cout << "Intercepting gRPC call, incrementing in-flight count." << std::endl;
            mInFlightCnt->fetch_add(1);
        }
        if (methods->QueryInterceptionHookPoint(grpc::experimental::InterceptionHookPoints::POST_RECV_CLOSE)) {
            std::cout << "Intercepting gRPC call, decrementing in-flight count." << std::endl;
            mInFlightCnt->fetch_sub(1);
        }
        methods->Proceed();
    }

private:
    std::atomic_int* mInFlightCnt;
};

class InFlightCountInterceptorFactory : public grpc::experimental::ServerInterceptorFactoryInterface {
public:
    explicit InFlightCountInterceptorFactory(std::atomic_int* inFlightCnt) : mInFlightCnt(inFlightCnt) {}
    grpc::experimental::Interceptor* CreateServerInterceptor(grpc::experimental::ServerRpcInfo* info) override {
        return new InFlightCountInterceptor(mInFlightCnt);
    }

private:
    std::atomic_int* mInFlightCnt;
};

class GrpcInputRunner : public InputRunner {
public:
    GrpcInputRunner(const GrpcInputRunner&) = delete;
    GrpcInputRunner(GrpcInputRunner&&) = delete;
    GrpcInputRunner& operator=(const GrpcInputRunner&) = delete;
    GrpcInputRunner& operator=(GrpcInputRunner&&) = delete;
    static GrpcInputRunner* GetInstance() {
        static GrpcInputRunner sInstance;
        return &sInstance;
    }

    void Init() override;
    void Stop() override;
    bool HasRegisteredPlugins() const override;

    template <typename T>
    bool UpdateListenInput(const std::string& configName, const std::string& address, const Json::Value& config);
    template <typename T>
    bool RemoveListenInput(const std::string& address, const std::string& configName);

private:
    GrpcInputRunner() = default;
    ~GrpcInputRunner() override = default;

    bool ShutdownGrpcServer(grpc::Server* server, std::atomic_int* inFlightCnt);

    mutable std::mutex mListenInputsMutex;
    std::map<std::string, GrpcListenInput> mListenInputs;

    std::atomic_bool mIsStarted = false;
};

} // namespace logtail
