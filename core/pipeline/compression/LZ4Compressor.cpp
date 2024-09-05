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

#include "pipeline/compression/LZ4Compressor.h"

#include <lz4/lz4.h>

#include "common/StringTools.h"

using namespace std;

namespace logtail {

bool LZ4Compressor::Compress(const string& input, string& output, string& errorMsg) {
    int encodingSize = LZ4_compressBound(input.size());
    if (encodingSize <= 0) {
        errorMsg = "input size is incorrect";
        return false;
    }
    output.resize(static_cast<size_t>(encodingSize));
    try {
        encodingSize = static_cast<size_t>(
            LZ4_compress_default(input.c_str(), const_cast<char*>(output.c_str()), input.size(), encodingSize));
        if (encodingSize <= 0) {
            errorMsg = "error code: " + ToString(encodingSize);
            return false;
        }
        output.resize(static_cast<size_t>(encodingSize));
        return true;
    } catch (...) {
    }
    return false;
}

#ifdef APSARA_UNIT_TEST_MAIN
bool LZ4Compressor::UnCompress(const string& input, string& output, string& errorMsg) {
    try {
        int length = LZ4_decompress_safe(input.c_str(), const_cast<char*>(output.c_str()), input.size(), output.size());
        if (length <= 0) {
            errorMsg = "error code: " + ToString(length);
            return false;
        }
        return true;
    } catch (...) {
    }
    return false;
}
#endif

} // namespace logtail
