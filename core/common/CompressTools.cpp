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

#include <lz4/lz4.h>
#ifdef __ANDROID__
#include <zlib.h>
#else
#include <zlib/zlib.h>
#endif
#include <zstd/zstd.h>

#include <cstring>

#include "protobuf/sls/sls_logs.pb.h"

namespace logtail {

const int32_t ZSTD_DEFAULT_LEVEL = 1;

bool UncompressData(sls_logs::SlsCompressType compressType,
                    const std::string& src,
                    uint32_t rawSize,
                    std::string& dst) {
    switch (compressType) {
        case sls_logs::SLS_CMP_NONE:
            dst = src;
            return true;
        case sls_logs::SLS_CMP_LZ4:
            return UncompressLz4(src, rawSize, dst);
        case sls_logs::SLS_CMP_DEFLATE:
            return UncompressDeflate(src, rawSize, dst);
        case sls_logs::SLS_CMP_ZSTD:
            return UncompressZstd(src, rawSize, dst);
        default:
            return false;
    }
}

bool CompressData(sls_logs::SlsCompressType compressType, const std::string& src, std::string& dst) {
    switch (compressType) {
        case sls_logs::SLS_CMP_NONE:
            dst = src;
            return true;
        case sls_logs::SLS_CMP_LZ4:
            return CompressLz4(src, dst);
        case sls_logs::SLS_CMP_DEFLATE:
            return CompressDeflate(src, dst);
        case sls_logs::SLS_CMP_ZSTD:
            return CompressZstd(src, dst, ZSTD_DEFAULT_LEVEL);
        default:
            return false;
    }
}

bool CompressData(sls_logs::SlsCompressType compressType, const char* src, uint32_t size, std::string& dst) {
    switch (compressType) {
        case sls_logs::SLS_CMP_NONE: {
            dst.assign(src, size);
            return true;
        }
        case sls_logs::SLS_CMP_LZ4:
            return CompressLz4(src, size, dst);
        case sls_logs::SLS_CMP_DEFLATE:
            return CompressDeflate(src, size, dst);
        case sls_logs::SLS_CMP_ZSTD:
            return CompressZstd(src, size, dst, ZSTD_DEFAULT_LEVEL);
        default:
            return false;
    }
}

bool UncompressLz4(const std::string& src, const uint32_t rawSize, char* dst) {
    uint32_t length = 0;
    try {
        length = LZ4_decompress_safe(src.c_str(), dst, src.length(), rawSize);
    } catch (...) {
        return false;
    }
    if (length != rawSize) {
        return false;
    }
    return true;
}

bool UncompressLz4(const char* srcPtr, const uint32_t srcSize, const uint32_t rawSize, std::string& dst) {
    dst.resize(rawSize);
    char* unCompressed = const_cast<char*>(dst.c_str());
    uint32_t length = 0;
    try {
        length = LZ4_decompress_safe(srcPtr, unCompressed, srcSize, rawSize);
    } catch (...) {
        return false;
    }
    if (length != rawSize) {
        return false;
    }
    return true;
}
bool CompressDeflate(const char* srcPtr, const uint32_t srcSize, std::string& dst) {
    int64_t dstLen = compressBound(srcSize);
    dst.resize(dstLen);
    if (compress((Bytef*)(dst.c_str()), (uLongf*)&dstLen, (const Bytef*)srcPtr, srcSize) == Z_OK) {
        dst.resize(dstLen);
        return true;
    }
    return false;
}

bool CompressDeflate(const std::string& src, std::string& dst) {
    int64_t dstLen = compressBound(src.size());
    dst.resize(dstLen);
    if (compress((Bytef*)(dst.c_str()), (uLongf*)&dstLen, (const Bytef*)(src.c_str()), src.size()) == Z_OK) {
        dst.resize(dstLen);
        return true;
    }
    return false;
}

bool UncompressDeflate(const char* srcPtr, const uint32_t srcSize, const int64_t rawSize, std::string& dst) {
    static const int64_t MAX_UMCOMPRESS_SIZE = 128 * 1024 * 1024;
    if (rawSize > MAX_UMCOMPRESS_SIZE) {
        return false;
    }
    dst.resize(rawSize);
    if (uncompress((Bytef*)(dst.c_str()), (uLongf*)&rawSize, (const Bytef*)(srcPtr), srcSize) != Z_OK) {
        return false;
    }
    return true;
}


bool UncompressDeflate(const std::string& src, const int64_t rawSize, std::string& dst) {
    return UncompressDeflate(src.c_str(), src.size(), rawSize, dst);
}


bool UncompressLz4(const std::string& src, const uint32_t rawSize, std::string& dst) {
    return UncompressLz4(src.c_str(), src.length(), rawSize, dst);
}
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

bool UncompressZstd(const std::string& src, const uint32_t rawSize, std::string& dst) {
    return UncompressZstd(src.c_str(), src.length(), rawSize, dst);
}

bool UncompressZstd(const char* srcPtr, const uint32_t srcSize, const uint32_t rawSize, std::string& dst) {
    dst.resize(rawSize);
    char* unCompressed = const_cast<char*>(dst.c_str());
    uint32_t length = 0;
    try {
        length = ZSTD_decompress(unCompressed, rawSize, srcPtr, srcSize);
    } catch (...) {
        return false;
    }
    if (length != rawSize) {
        return false;
    }
    return true;
}

bool CompressZstd(const char* srcPtr, const uint32_t srcSize, std::string& dst, int32_t level) {
    uint32_t encodingSize = ZSTD_compressBound(srcSize);
    dst.resize(encodingSize);
    char* compressed = const_cast<char*>(dst.c_str());
    try {
        size_t const cmp_size = ZSTD_compress(compressed, encodingSize, srcPtr, srcSize, level);
        if (ZSTD_isError(cmp_size)) {
            return false;
        }
        dst.resize(cmp_size);
        return true;
    } catch (...) {
    }
    return false;
}

bool CompressZstd(const std::string& src, std::string& dst, int32_t level) {
    return CompressZstd(src.c_str(), src.length(), dst, level);
}

} // namespace logtail
