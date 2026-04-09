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
#include <future>
#include <memory>
#include <mutex>
#include <vector>

#include "logger/Logger.h"
#include "plugin/flusher/opentelemetry/FlusherOTLPNative.h"
#include "runner/FlusherRunner.h"
#include "runner/sink/grpc/GrpcSink.h"

namespace logtail {

class GrpcSinkMock : public GrpcSink {
public:
    GrpcSinkMock(const GrpcSinkMock&) = delete;
    GrpcSinkMock& operator=(const GrpcSinkMock&) = delete;

    static GrpcSinkMock* GetInstance() {
        static GrpcSinkMock instance;
        return &instance;
    }

    bool Init() override {
        mIsFlush = false;
        mThreadRes = std::async(std::launch::async, &GrpcSinkMock::Run, this);
        return true;
    }

    void Stop() override {
        mIsFlush = true;
        mCV.notify_all();
        if (!mThreadRes.valid()) {
            return;
        }
        std::future_status s = mThreadRes.wait_for(std::chrono::seconds(1));
        if (s == std::future_status::ready) {
            LOG_INFO(sLogger, ("grpc sink mock", "stopped successfully"));
        } else {
            LOG_WARNING(sLogger, ("grpc sink mock", "forced to stopped"));
        }
        ClearRequests();
    }

    void AddRequest(std::unique_ptr<OTLPGrpcCallContext>&& ctx) override {
        std::lock_guard<std::mutex> lock(mMutex);
        mPendingRequests.push_back(std::move(ctx));
        mCV.notify_one();
    }

    void Run() {
        LOG_INFO(sLogger, ("grpc sink mock", "started"));
        while (true) {
            std::vector<std::unique_ptr<OTLPGrpcCallContext>> items;
            {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mPendingRequests.empty()) {
                    if (mIsFlush) {
                        break;
                    }
                    mCV.wait_for(lock, std::chrono::milliseconds(100));
                    continue;
                }
                items = std::move(mPendingRequests);
            }

            for (auto& ctx : items) {
                auto* rawCtx = ctx.release();
                auto* flusher = static_cast<FlusherOTLPNative*>(rawCtx->item->mFlusher);

                grpc::Status status(grpc::StatusCode::OK, "OK");
                {
                    std::lock_guard<std::mutex> lock(mRequestsMutex);
                    mRequests.push_back(*rawCtx->item);
                }

                // Match real GrpcSink behavior: IncInFlight before dispatch
                flusher->IncInFlight();

                // Simulate async callback
                rawCtx->context = std::make_unique<grpc::ClientContext>();
                flusher->HandleGrpcCallback(std::move(status), rawCtx);
            }

            if (mIsFlush) {
                std::unique_lock<std::mutex> lock(mMutex);
                if (mPendingRequests.empty()) {
                    break;
                }
            }
        }
    }

    std::vector<SenderQueueItem> GetRequests() {
        std::lock_guard<std::mutex> lock(mRequestsMutex);
        return mRequests;
    }

    void ClearRequests() {
        std::lock_guard<std::mutex> lock(mRequestsMutex);
        mRequests.clear();
    }

private:
    GrpcSinkMock() = default;
    ~GrpcSinkMock() = default;

    std::atomic_bool mIsFlush{false};
    std::mutex mMutex;
    std::condition_variable mCV;
    std::vector<std::unique_ptr<OTLPGrpcCallContext>> mPendingRequests;

    std::mutex mRequestsMutex;
    std::vector<SenderQueueItem> mRequests;

    friend class GrpcSink;
};

} // namespace logtail
