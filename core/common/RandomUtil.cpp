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

#include "RandomUtil.h"
#include <sstream>
#if defined(__linux__)
#include <uuid/uuid.h>
#endif
#include "util.h"

namespace logtail {

namespace detail {

    size_t countBitSize(size_t maxVal) {
        if (0 == maxVal) {
            return 1;
        }

        size_t count = 0;
        while (maxVal != 0) {
            count++;
            maxVal = maxVal >> 1;
        }
        return count;
    }

    inline char valToHex(size_t val) { return val < 10 ? ('0' + val) : ('A' + val - 10); }

    inline size_t hexToVal(char hex) { return hex >= 'A' ? 10 + hex - 'A' : hex - '0'; }

} // namespace detail

std::string GenerateRandomHashKey() {
    std::string hashKey;
    hashKey.reserve(32);
#if defined(__linux__)
    uuid_t in;
    uuid_generate_random(in);
    const int kBufferSize = 128;
    char out[kBufferSize];
    uuid_unparse_upper(in, out);
    for (int i = 0; i < kBufferSize && out[i] != '\0'; ++i) {
        if (out[i] != '-') {
            hashKey += out[i];
        }
    }
    return hashKey;
#elif defined(_MSC_VER)
    for (auto v : CalculateRandomUUID()) {
        if (v != '-') {
            hashKey += v;
        }
    }
    return hashKey;
#endif
}

std::string GenerateHashKey(const std::string& baseHashKey, size_t bucketIndex, size_t bucketNum) {
    const size_t kBitSizePerHex = 4;
    const auto bitSize = detail::countBitSize(bucketNum - 1);
    const auto requiredHexCount = bitSize / kBitSizePerHex + (bitSize % kBitSizePerHex != 0 ? 1 : 0);
    const auto baseVal = bucketIndex << (requiredHexCount * kBitSizePerHex - bitSize);

    std::string hashKey = baseHashKey;
    const size_t k1111 = 15;
    for (size_t idx = 0; idx < requiredHexCount; ++idx) {
        // Extract 4 bits from baseVal.
        auto val = baseVal;
        auto moveBitSize = (requiredHexCount - idx - 1) * kBitSizePerHex;
        if (moveBitSize > 0) {
            val = (val & (k1111 << moveBitSize)) >> moveBitSize;
        } else {
            val &= k1111;
        }

        // Calculate mask size, override hashKey[idx] with val.
        auto maskBitSize = bitSize - (idx * kBitSizePerHex);
        if (maskBitSize >= kBitSizePerHex) {
            // Override whole hex.
            hashKey[idx] = detail::valToHex(val);
        } else {
            // First maskBitSize bits is overriden by val.
            hashKey[idx]
                = detail::valToHex(val + (((detail::hexToVal(hashKey[idx]) << maskBitSize) & k1111) >> maskBitSize));
        }
    }
    return hashKey;
}

} // namespace logtail
