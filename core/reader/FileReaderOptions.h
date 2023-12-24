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

#include <cstdint>
#include <string>
#include <utility>

#include <json/json.h>

#include "pipeline/PipelineContext.h"

namespace logtail {
struct FileReaderOptions {
    enum class Encoding { UTF8, UTF16, GBK };

    Encoding mFileEncoding = Encoding::UTF8;
    bool mTailingAllMatchedFiles = false;
    uint32_t mTailSizeKB;
    uint32_t mFlushTimeoutSecs;
    uint32_t mReadDelaySkipThresholdBytes = 0;
    uint32_t mReadDelayAlertThresholdBytes;
    uint32_t mCloseUnusedReaderIntervalSec;
    uint32_t mRotatorQueueSize;
    // This param is compound since it controls both reader option and parser option. For simplicity, we put it in
    // reader option. If option controlling parser is separated from this, the separated option should be placed in
    // input.
    bool mAppendingLogPositionMeta = false;

    FileReaderOptions();

    bool Init(const Json::Value& config, const PipelineContext& ctx, const std::string& pluginName);
};

using FileReaderConfig = std::pair<const FileReaderOptions*, const PipelineContext*>;

} // namespace logtail
