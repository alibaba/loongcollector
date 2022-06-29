/*
 * Copyright 2022 iLogtail Authors
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
#include <vector>
#include "common/WaitObject.h"
#include "common/Lock.h"
#include "common/Thread.h"

namespace sls_logs {
class LogGroup;
}

namespace logtail {

class MetricSender {
public:
    static void SendMetric(const sls_logs::LogGroup& logGroup) {}
};

} // namespace logtail
