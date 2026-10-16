// Copyright 2026 iLogtail Authors
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

#include <cstdint>

#include <atomic>

namespace logtail::ebpf {

/// Byte budget for AgentSight events held on the C++ side: accepted in the FFI callbacks and not
/// yet emitted into the process queue (i.e. sitting in EBPFServer::mCommonEventQueue as
/// AgentsightLlmRecord / AgentsightHttpsRecord). This is a pipeline-buffer budget, NOT a process
/// RSS cap — allocator slack (pages tcmalloc has freed but not returned) and fixed record overhead
/// beyond the estimated payload are not accounted for.
///
/// limit == 0 means disabled: TryAcquire reports kDisabled without touching any counter, so the
/// feature-off path is bit-identical to the pre-feature behaviour.
///
/// Thread model: the PRODUCER side (OnLlmCallback / OnHttpsCallback) is serialised — callbacks only
/// run inside handle_read while the caller holds mLibMutex — and the CONSUMER side (record
/// destructors on the EBPFServer handler thread) only calls Release. Atomics are sufficient; no
/// lock may be taken here (callbacks must not lock mLibMutex, see AgentsightManager.cpp).
///
/// Lifetime: owned via shared_ptr — records enqueue into a queue shared by all eBPF plugins and may
/// outlive the AgentsightManager that created the budget, so the last Release can happen after the
/// manager is gone.
class AgentsightByteBudget {
public:
    enum class AcquireResult {
        kDisabled, // limit == 0: no accounting, caller proceeds as if the feature did not exist
        kAcquired, // n bytes reserved; caller MUST stamp them onto the record for later Release
        kRejected, // over limit: caller drops the event (counters already bumped here)
    };

    /// Reserve n bytes. Uses fetch_add-then-roll-back instead of a CAS loop: the producer side is
    /// serialised, so the only concurrency is producer(+)/consumer(-); within the roll-back window
    /// Used() is transiently high by at most one event, which can only cause one extra drop — the
    /// safe direction for a memory cap.
    AcquireResult TryAcquire(uint64_t n) {
        const uint64_t limit = mLimitBytes.load(std::memory_order_relaxed);
        if (limit == 0) {
            return AcquireResult::kDisabled;
        }
        const uint64_t prev = mUsedBytes.fetch_add(n, std::memory_order_acq_rel);
        if (prev + n <= limit) {
            return AcquireResult::kAcquired;
        }
        mUsedBytes.fetch_sub(n, std::memory_order_acq_rel);
        mDroppedEvents.fetch_add(1, std::memory_order_relaxed);
        mDroppedBytes.fetch_add(n, std::memory_order_relaxed);
        return AcquireResult::kRejected;
    }

    /// Give n bytes back. Saturating: a stray or duplicated Release can never underflow the counter.
    void Release(uint64_t n) {
        uint64_t prev = mUsedBytes.load(std::memory_order_relaxed);
        while (!mUsedBytes.compare_exchange_weak(
            prev, prev > n ? prev - n : 0, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        }
    }

    /// Update the cap. NEVER touches mUsedBytes: on hot config updates in-flight records still hold
    /// their bytes and will Release them through their destructors; clearing here would double-count
    /// and corrupt the accounting.
    void SetLimit(uint64_t limitBytes) { mLimitBytes.store(limitBytes, std::memory_order_relaxed); }

    uint64_t Used() const { return mUsedBytes.load(std::memory_order_relaxed); }
    uint64_t Limit() const { return mLimitBytes.load(std::memory_order_relaxed); }
    uint64_t DroppedEvents() const { return mDroppedEvents.load(std::memory_order_relaxed); }
    uint64_t DroppedBytes() const { return mDroppedBytes.load(std::memory_order_relaxed); }

private:
    std::atomic<uint64_t> mUsedBytes{0};
    std::atomic<uint64_t> mLimitBytes{0};
    std::atomic<uint64_t> mDroppedEvents{0};
    std::atomic<uint64_t> mDroppedBytes{0};
};

} // namespace logtail::ebpf
