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
#include <cstdint>
#include <string>

// Hash and file signature utility.
namespace logtail {

// Hash(string(@poolIn, @inputBytesNum)) => @md5.
// TODO: Same implementation in sdk module, merge them.
void DoMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t md5[16]);

bool SignatureToHash(const std::string& signature, uint64_t& sigHash, uint32_t& sigSize);
bool CheckAndUpdateSignature(const std::string& signature, uint64_t& sigHash, uint32_t& sigSize);
bool CheckFileSignature(const std::string& filePath, uint64_t sigHash, uint32_t sigSize, bool fuseMode = false);

int64_t HashString(const std::string& str);
int64_t HashSignatureString(const char* str, size_t strLen);

} // namespace logtail
