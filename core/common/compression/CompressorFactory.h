/*
 * Copyright 2024 iLogtail Authors
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

#include <json/json.h>

#include <memory>
#include <string>

#include "pipeline/PipelineContext.h"
#include "common/compression/CompressType.h"
#include "common/compression/Compressor.h"

namespace logtail {

const std::string& CompressTypeToString(CompressType type);

class CompressorFactory {
public:
    CompressorFactory(const CompressorFactory&) = delete;
    CompressorFactory& operator=(const CompressorFactory&) = delete;

    static CompressorFactory* GetInstance() {
        static CompressorFactory instance;
        return &instance;
    }

    std::unique_ptr<Compressor> Create(const Json::Value& config,
                                       const PipelineContext& ctx,
                                       const std::string& pluginType,
                                       const std::string& flusherId,
                                       CompressType defaultType);
    std::unique_ptr<Compressor> Create(CompressType type);

private:
    CompressorFactory() = default;
    ~CompressorFactory() = default;
};

} // namespace logtail
