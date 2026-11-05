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

#include <string>
#include <vector>

#include "host_monitor/collector/BaseCollector.h"

namespace logtail {

struct FsStatInfo {
    std::string device;
    std::string mountPoint;
    uint64_t bsize = 0;
    uint64_t blocks = 0;
    uint64_t bfree = 0;
    uint64_t bavail = 0;
    uint64_t files = 0;
    uint64_t ffree = 0;
};

class FsStatCollector : public BaseCollector {
public:
    FsStatCollector() = default;
    ~FsStatCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    bool ReadMountPoints(std::vector<FsStatInfo>& fsStats);
};

} // namespace logtail
