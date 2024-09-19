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

#include "common/compression/ZstdCompressor.h"

#include <zstd/zstd.h>

using namespace std;

namespace logtail {

bool ZstdCompressor::Compress(const string& input, string& output, string& errorMsg) {
    size_t encodingSize = ZSTD_compressBound(input.size());
    output.resize(encodingSize);
    try {
        encodingSize = ZSTD_compress(
            const_cast<char*>(output.c_str()), encodingSize, input.c_str(), input.size(), mCompressionLevel);
        if (ZSTD_isError(encodingSize)) {
            errorMsg = ZSTD_getErrorName(encodingSize);
            return false;
        }
        output.resize(encodingSize);
        return true;
    } catch (...) {
    }
    return false;
}

#ifdef APSARA_UNIT_TEST_MAIN
bool ZstdCompressor::UnCompress(const string& input, string& output, string& errorMsg) {
    try {
        size_t length
            = ZSTD_decompress(const_cast<char*>(output.c_str()), output.size(), input.c_str(), input.size());
        if (ZSTD_isError(length)) {
            errorMsg = ZSTD_getErrorName(length);
            return false;
        }
        return true;
    } catch (...) {
    }
    return false;
}
#endif

} // namespace logtail
