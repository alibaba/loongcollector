/*
 * Copyright 2025 iLogtail Authors
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

#include <string>
#include <unordered_map>

#include "collection_pipeline/plugin/interface/Input.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "file_server/FileDiscoveryOptions.h"
#include "file_server/FileTagOptions.h"
#include "file_server/MultilineOptions.h"
#include "file_server/checkpoint/FileCheckpoint.h"
#include "file_server/reader/FileReaderOptions.h"
#include "monitor/metric_models/ReentrantMetricsRecord.h"

namespace logtail {

class InputStaticFile : public Input {
public:
    static const std::string sName;

    static bool DeduceAndSetContainerBaseDir(ContainerInfo& containerInfo,
                                             const CollectionPipelineContext*,
                                             const FileDiscoveryOptions*);

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config, Json::Value& optionalGoPipeline) override;
    bool Start() override;
    bool Stop(bool isPipelineRemoving) override;
    QueueType GetProcessQueueType() const override { return QueueType::COUNT_BOUNDED; }

    FileDiscoveryOptions mFileDiscovery;
    bool mEnableContainerDiscovery = false;
    ContainerDiscoveryOptions mContainerDiscovery;
    FileReaderOptions mFileReader;
    MultilineOptions mMultiline;
    FileTagOptions mFileTag;

private:
    PluginMetricManagerPtr mPluginMetricManager;
    IntGaugePtr mMonitorFileTotal;

    // 文件路径到容器元信息的映射（容器场景下使用）
    std::unordered_map<std::string, FileCheckpoint::ContainerMeta> mFileContainerMetaMap;

    bool CreateInnerProcessors();

#ifdef APSARA_UNIT_TEST_MAIN
    friend class InputStaticFileUnittest;
#endif
};

} // namespace logtail
