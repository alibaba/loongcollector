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

#include "pipeline/compression/Compressor.h"

namespace logtail {

class ZstdCompressor : public Compressor {
public:
    ZstdCompressor(CompressType type, int32_t level = 1) : Compressor(type), mCompressionLevel(level){};

    bool Compress(const std::string& input, std::string& output, std::string& errorMsg) override;

#ifdef APSARA_UNIT_TEST_MAIN
    bool UnCompress(const std::string& input, std::string& output, std::string& errorMsg) override;
#endif

private:
    int32_t mCompressionLevel = 1;
};

} // namespace logtail
