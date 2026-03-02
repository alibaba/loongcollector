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

#include "CpuProfiler.h"
#include "Log.h"

namespace logtail {
namespace ebpf {

void CpuProfiler::Start(livetrace_profiler_read_cb_ctx_t handler, void* ctx, std::optional<std::string> hostRootPath) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mProfiler == nullptr) {
        livetrace_enable_tracing();
        mProfiler = livetrace_profiler_create();
        if (mProfiler == nullptr) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "[CpuProfiler][Start] failed to create profiler");
            return;
        }
        mHandler = handler;
        mCtx = ctx;
        if (hostRootPath != std::nullopt) {
            livetrace_set_host_root_path(hostRootPath.value().c_str());
        }
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[CpuProfiler][Start] create profiler, handler: %p ctx: %p",
                 handler,
                 ctx);
    }
}

void CpuProfiler::Stop() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mProfiler != nullptr) {
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "[CpuProfiler][Stop] destroy profiler");
        livetrace_profiler_destroy(mProfiler);
        mProfiler = nullptr;
    }
    mPids.clear();
    mHandler = nullptr;
    mCtx = nullptr;
}

void CpuProfiler::UpdatePids(std::unordered_set<uint32_t> newPids) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mProfiler == nullptr) {
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[CpuProfiler][UpdatePids] profiler is not initialized, cannot update pids");
        return;
    }

    std::unordered_set<uint32_t> toAdd, toRemove;
    compareSets(newPids, toAdd, toRemove);

    if (toAdd.empty() && toRemove.empty()) {
        return; // No changes
    }

    if (!toAdd.empty()) {
        std::string pidsToAdd = pidsToString(toAdd);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[CpuProfiler][UpdatePids] add pids: %s",
                 pidsToAdd.c_str());
        livetrace_profiler_ctrl(mProfiler, LivetraceCtrlOp::LIVETRACE_ADD, pidsToAdd.c_str());
    }

    if (!toRemove.empty()) {
        std::string pidsToRemove = pidsToString(toRemove);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[CpuProfiler][UpdatePids] remove pids: %s",
                 pidsToRemove.c_str());
        livetrace_profiler_ctrl(mProfiler, LivetraceCtrlOp::LIVETRACE_REMOVE, pidsToRemove.c_str());
    }

    mPids = std::move(newPids);
}

void CpuProfiler::Poll() {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mProfiler == nullptr || mHandler == nullptr) {
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                 "[CpuProfiler][Poll] profiler is not initialized or handler is null, cannot poll");
        return;
    }
    if (mPids.empty()) {
        return;
    }

    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "[CpuProfiler][Poll] poll");
    livetrace_profiler_read(mProfiler, handler_without_ctx);
}

std::string CpuProfiler::pidsToString(const std::unordered_set<uint32_t>& pids) {
    std::string result;
    for (const auto& pid : pids) {
        if (!result.empty()) {
            result += ",";
        }
        result += std::to_string(pid);
    }
    return result;
}

void CpuProfiler::compareSets(const std::unordered_set<uint32_t>& newPids,
                 std::unordered_set<uint32_t>& toAdd,
                 std::unordered_set<uint32_t>& toRemove) {
    for (const auto& pid : newPids) {
        if (mPids.find(pid) == mPids.end()) {
            toAdd.insert(pid);
        }
    }

    for (const auto& pid : mPids) {
        if (newPids.find(pid) == newPids.end()) {
            toRemove.insert(pid);
        }
    }
}

} // namespace ebpf
} // namespace logtail
