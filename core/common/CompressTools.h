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
#include "protobuf/sls/sls_logs.pb.h"

namespace logtail {

extern const int32_t ZSTD_DEFAULT_LEVEL;

bool UncompressData(sls_logs::SlsCompressType compressType, const std::string& src, uint32_t rawSize, std::string& dst);

bool CompressData(sls_logs::SlsCompressType compressType, const std::string& src, std::string& dst);
bool CompressData(sls_logs::SlsCompressType compressType, const char* src, uint32_t size, std::string& dst);

bool UncompressDeflate(const std::string& src, const int64_t rawSize, std::string& dst);
bool UncompressDeflate(const char* srcPtr, const uint32_t srcSize, const int64_t rawSize, std::string& dst);

bool CompressDeflate(const std::string& src, std::string& dst);
bool CompressDeflate(const char* srcPtr, const uint32_t srcSize, std::string& dst);

bool UncompressLz4(const std::string& src, const uint32_t rawSize, std::string& dst);
bool UncompressLz4(const std::string& src, const uint32_t rawSize, char* dst);
bool UncompressLz4(const char* srcPtr, const uint32_t srcSize, const uint32_t rawSize, std::string& dst);

bool CompressLz4(const std::string& src, std::string& dst);
bool CompressLz4(const char* srcPtr, const uint32_t srcSize, std::string& dest);

bool UncompressZstd(const std::string& src, const uint32_t rawSize, std::string& dst);
bool UncompressZstd(const char* srcPtr, const uint32_t srcSize, const uint32_t rawSize, std::string& dst);

bool CompressZstd(const char* srcPtr, const uint32_t srcSize, std::string& dst, int32_t level);
bool CompressZstd(const std::string& src, std::string& dst, int32_t level);

} // namespace logtail