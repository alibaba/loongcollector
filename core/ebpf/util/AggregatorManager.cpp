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

#include "AggregatorManager.h"

#include <chrono>

#include <array>
#include <atomic>
#include <map>
#include <queue>
#include <unordered_map>

#include "AggregateTree.h"
#include "common/timer/Timer.h"


namespace logtail {
namespace ebpf {

// void AggregatorManager::Init() {
//     if (mInited) {
//         return;
//     }

//     mTimer.Init();
//     mInited = true;
// }

// void AggregatorManager::AddAggTree() {
//     // sliding window ...
//     // mTimer.PushEvent();
// }

} // namespace ebpf
} // namespace logtail
