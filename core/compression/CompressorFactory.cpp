// Copyright 2024 iLogtail Authors
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

#include "compression/CompressorFactory.h"

#include "common/ParamExtractor.h"
#include "compression/LZ4Compressor.h"
#include "compression/ZstdCompressor.h"

using namespace std;

namespace logtail {

unique_ptr<Compressor> CompressorFactory::Create(const Json::Value& config,
                                                      const PipelineContext& ctx,
                                                      const string& pluginType,
                                                      CompressType defaultType) {
    string compressType, errorMsg;
    if (!GetOptionalStringParam(config, "CompressType", compressType, errorMsg)) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              errorMsg,
                              CompressTypeToString(defaultType),
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
        return Create(defaultType);
    } else if (compressType == "lz4") {
        return Create(CompressType::LZ4);
    } else if (compressType == "zstd") {
        return Create(CompressType::ZSTD);
    } else if (compressType == "none") {
        return nullptr;
    } else if (!compressType.empty()) {
        PARAM_WARNING_DEFAULT(ctx.GetLogger(),
                              ctx.GetAlarm(),
                              "string param CompressType is not valid",
                              CompressTypeToString(defaultType),
                              pluginType,
                              ctx.GetConfigName(),
                              ctx.GetProjectName(),
                              ctx.GetLogstoreName(),
                              ctx.GetRegion());
        return Create(defaultType);
    } else {
        return Create(defaultType);
    }
}

unique_ptr<Compressor> CompressorFactory::Create(CompressType type) {
    switch (type) {
        case CompressType::LZ4:
            return make_unique<LZ4Compressor>(type);
        case CompressType::ZSTD:
            return make_unique<ZstdCompressor>(type);
        default:
            return nullptr;
    }
}

const string& CompressTypeToString(CompressType type) {
    switch (type) {
        case CompressType::LZ4:
            static string lz4 = "lz4";
            return lz4;
        case CompressType::ZSTD:
            static string zstd = "zstd";
            return zstd;
        case CompressType::NONE:
            static string none = "none";
            return none;
        default:
            static string unknown = "unknown";
            return unknown;
    }
}

} // namespace logtail
