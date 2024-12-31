/*
 * Copyright 2023 iLogtail Authors
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

#include "models/LogEvent.h"
#include "plugin/interface/Processor.h"
#include "K8sMetadata.h"

namespace logtail {

    class LabelingK8sMetadata {
    public:
        void AddLabelToLogGroup(PipelineEventGroup& logGroup);
        bool ProcessEvent(PipelineEventPtr& e,  std::vector<std::string>& container_vec,  std::vector<std::string>& remote_ip_vec);
        // 声明模板函数
        template <typename Event>
        bool AddLabels(Event& e, std::vector<std::string>& containerVec, std::vector<std::string>& remoteIpVec);
    protected:
        bool IsSupportedEvent(const PipelineEventPtr& e) const;   
    };

} // namespace logtail
