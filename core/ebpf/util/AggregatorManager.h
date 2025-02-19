// Copyright 2023 iLogtail Authors
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

#include <array>
#include <atomic>
#include <map>
#include <queue>
#include <unordered_map>

#include "AggregateTree.h"
#include "common/timer/Timer.h"
#include "ebpf/type/table/DataTable.h"


namespace logtail {
namespace ebpf {


class FixedInvervalEvent : public TimerEvent {
public:
    using FlushFunc = std::function<void()>;
    FixedInvervalEvent(int intervalSec, FlushFunc fn)
        : TimerEvent(std::chrono::steady_clock::now() + std::chrono::seconds(intervalSec)),
          mIntervalSec(intervalSec),
          mFunc(fn) {}

    bool IsValid() const override { return mIsValid; }
    bool Execute() {
        // get tree
        mFunc();
        return true;
    }

private:
    bool mIsValid = false;
    std::chrono::seconds mIntervalSec;
    FlushFunc mFunc;
};

template <typename Data, typename Value, typename FlushType>
class AggregatorManager {
public:
    AggregatorManager(size_t max_nodes,
                      const std::function<void(std::unique_ptr<Data>&, const Value&)>& aggregate,
                      const std::function<std::unique_ptr<Data>(const Value& n)>& generate,
                      int interval_sec)
        : mTree(max_nodes, aggregate, generate), mIntervalSec(interval_sec) {}

    ~AggregatorManager();

    void Init() {
        mTimer.PushEvent(std::make_unique<FixedInvervalEvent>(mIntervalSec, [&]() {
            // mTree.NodeCount();
            // 1. generate measurebatch ==> eventgroup
            // 2. push to process queue ...
            // 3. swap agg tree
            // mTimer.PushEvent();
        }));
    }

    void Aggregate(const std::array<size_t, kMaxAggregationLevel>& key, const Value& value) {
        mTree.Aggregate(key, value);
    }

    void Flush() {}

    // AggregatorManager(const AggregatorManager&) = delete;
    // AggregatorManager& operator=(const AggregatorManager&) = delete;
private:
    Timer mTimer;
    // volatile bool mInited = false;
    SIZETAggTree<Data, Value> mTree;
    int mIntervalSec;
};


} // namespace ebpf
} // namespace logtail
