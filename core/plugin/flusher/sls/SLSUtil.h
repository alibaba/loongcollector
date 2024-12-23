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

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace logtail {

#define SHA1_INPUT_WORDS 16
#define SHA1_DIGEST_WORDS 5
#define SHA1_INPUT_BYTES (SHA1_INPUT_WORDS * sizeof(uint32_t))
#define SHA1_DIGEST_BYTES (SHA1_DIGEST_WORDS * sizeof(uint32_t))

class SHA1 {
public:
    SHA1() : bits(0) { memcpy(H, IV, sizeof(H)); }
    SHA1(const SHA1& s) {
        bits = s.bits;
        memcpy(H, s.H, sizeof(H));
        memcpy(M, s.M, sizeof(M));
    }
    void init() {
        bits = 0;
        memcpy(H, IV, sizeof(H));
    }
    void add(const uint8_t* data, size_t len);
    uint8_t* result();

private:
    uint64_t bits;
    uint32_t H[SHA1_DIGEST_WORDS];
    uint8_t M[SHA1_INPUT_BYTES];

    static const uint32_t IV[SHA1_DIGEST_WORDS];
    void transform();
};

class HMAC {
public:
    HMAC(const uint8_t* key, size_t lkey);
    HMAC(const HMAC& hm) : in(hm.in), out(hm.out) {}

    void init(const uint8_t* key, size_t lkey);

    void add(const uint8_t* data, size_t len) { in.add(data, len); }

    uint8_t* result() {
        out.add(in.result(), SHA1_DIGEST_BYTES);
        return out.result();
    }

private:
    SHA1 in, out;
};

std::string GetDateString();

std::string GetUrlSignature(const std::string& httpMethod,
                            const std::string& operationType,
                            std::map<std::string, std::string>& httpHeader,
                            const std::map<std::string, std::string>& parameterList,
                            const std::string& content,
                            const std::string& signKey);

} // namespace logtail
