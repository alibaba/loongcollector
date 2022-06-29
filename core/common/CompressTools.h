/*
 * Copyright 2022 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <string>
#include <cstdint>

namespace logtail {

bool UncompressDeflate(const std::string& src, const int64_t rawSize, std::string& dst);
bool UncompressDeflate(const char* srcPtr, const uint32_t srcSize, const int64_t rawSize, std::string& dst);

bool CompressDeflate(const std::string& src, std::string& dst);
bool CompressDeflate(const char* srcPtr, const uint32_t srcSize, std::string& dst);

bool UncompressLz4(const std::string& src, const uint32_t rawSize, std::string& dst);
bool UncompressLz4(const std::string& src, const uint32_t rawSize, char* dst);
bool UncompressLz4(const char* srcPtr, const uint32_t srcSize, const uint32_t rawSize, std::string& dst);

bool CompressLz4(const std::string& src, std::string& dst);
bool CompressLz4(const char* srcPtr, const uint32_t srcSize, std::string& dest);

// old mode , with 8 bytes leading raw size
bool RawCompress(std::string& data);
bool Compress(std::string& data);
bool Compress(const char* data, int64_t srcLen, std::string& compressedData);
char* Compress(const char* data, int64_t srcLen, int64_t& dstLen);
bool Uncompress(const std::string& src, std::string& dst);
char* Uncompress(const char* src, int64_t srcLen, int64_t& dstLen);

} // namespace logtail