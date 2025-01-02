// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "CompressTools.h"

#include "lz4/lz4.h"

namespace logtail {

bool CompressLz4(const char* srcPtr, const uint32_t srcSize, std::string& dst) {
    uint32_t encodingSize = LZ4_compressBound(srcSize);
    dst.resize(encodingSize);
    char* compressed = const_cast<char*>(dst.c_str());
    try {
        encodingSize = LZ4_compress_default(srcPtr, compressed, srcSize, encodingSize);
        if (encodingSize) {
            dst.resize(encodingSize);
            return true;
        }
    } catch (...) {
    }
    return false;
}

bool CompressLz4(const std::string& src, std::string& dst) {
    return CompressLz4(src.c_str(), src.length(), dst);
}

} // namespace logtail
