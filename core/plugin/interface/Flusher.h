/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "plugin/interface/Plugin.h"
// #include "table/Table.h"
#include "json/json.h"

namespace logtail {
class Flusher : public Plugin {
public:
    virtual ~Flusher() = default;

    // virtual bool Init(const Table& config) = 0;
    virtual bool Init(const Json::Value& config) = 0;
    virtual bool Start() = 0;
    // 单例flusher: 只修改元信息
    // 独立flusher：停止线程
    virtual bool Stop(bool isPipelineRemoving) = 0;
};

} // namespace logtail
