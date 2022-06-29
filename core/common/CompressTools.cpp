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
#include <zlib/zlib.h>
#include <lz4/lz4.h>
#include <cstring>

namespace logtail {

bool RawCompress(std::string& data) {
    int64_t length = compressBound(data.length());
    char* compressed = new char[length];
    if (compress((Bytef*)(compressed), (uLongf*)&length, (const Bytef*)(data.c_str()), data.length()) == Z_OK) {
        data.assign(compressed, length);
        delete compressed;
        return true;
    } else {
        delete compressed;
        return false;
    }
}
bool Compress(std::string& data) {
    int64_t srcLen = data.length();
    int64_t length = compressBound(data.length());
    int32_t offset = sizeof(length);
    char* compressed = new char[length + offset];
    memcpy((void*)compressed, (const void*)&srcLen, offset);
    if (compress((Bytef*)(compressed + offset), (uLongf*)&length, (const Bytef*)(data.c_str()), data.length())
        == Z_OK) {
        data.assign(compressed, length + offset);
        delete compressed;
        return true;
    } else {
        delete compressed;
        return false;
    }
}
bool Compress(const char* data, int64_t srcLen, std::string& compressedData) {
    int64_t dstLen = compressBound(srcLen);

    int32_t offset = sizeof(srcLen);
    compressedData.resize(dstLen + offset);

    char* compressed = &(compressedData[0]);
    memcpy((void*)compressed, (const void*)&srcLen, offset);
    if (compress((Bytef*)(compressed + offset), (uLongf*)&dstLen, (const Bytef*)(data), srcLen) == Z_OK) {
        dstLen += offset;
        compressedData.resize(dstLen);
        return true;
    } else {
        return false;
    }
}
char* Compress(const char* data, int64_t srcLen, int64_t& dstLen) {
    dstLen = compressBound(srcLen);
    int32_t offset = sizeof(srcLen);
    char* compressed = new char[dstLen + offset];
    memcpy((void*)compressed, (const void*)&srcLen, offset);
    if (compress((Bytef*)(compressed + offset), (uLongf*)&dstLen, (const Bytef*)(data), srcLen) == Z_OK) {
        dstLen += offset;
        return compressed;
    } else {
        delete compressed;
        return NULL;
    }
}
bool Uncompress(const std::string& src, std::string& dst) {
    int64_t length, errorCode;
    memcpy((void*)&length, (const void*)(src.c_str()), sizeof(length));
    static const int64_t MAX_UMCOMPRESS_SIZE = 128 * 1024 * 1024;
    if (length > MAX_UMCOMPRESS_SIZE) {
        return false;
    }

    dst.assign(length, '\0');
    if ((errorCode = uncompress((Bytef*)(dst.c_str()),
                                (uLongf*)&length,
                                (const Bytef*)(src.c_str() + sizeof(length)),
                                src.length() - sizeof(length)))
        != Z_OK) {
        return false;
    }
    return true;
}
char* Uncompress(const char* src, int64_t srcLen, int64_t& dstLen) {
    int64_t errorCode;
    memcpy((void*)&dstLen, (const void*)(src), sizeof(dstLen));
    static const int64_t MAX_UMCOMPRESS_SIZE = 128 * 1024 * 1024;
    if (dstLen > MAX_UMCOMPRESS_SIZE) {
        return NULL;
    }
    char* dst = new char[dstLen];
    memset(dst, 0, dstLen);
    if ((errorCode
         = uncompress((Bytef*)(dst), (uLongf*)&dstLen, (const Bytef*)(src + sizeof(dstLen)), srcLen - sizeof(dstLen)))
        != Z_OK) {
        delete dst;
        return NULL;
    }
    return dst;
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
        encodingSize = LZ4_compress(srcPtr, compressed, srcSize);
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
