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

#include "plugin/flusher/sls/SLSUtil.h"

#include "app_config/AppConfig.h"
#include "common/EncodingUtil.h"
#include "common/HashUtil.h"
#include "common/TimeUtil.h"
#include "common/http/Constant.h"
#include "plugin/flusher/sls/SLSConstant.h"

using namespace std;

namespace logtail {

static string DATE_FORMAT_RFC822 = "%a, %d %b %Y %H:%M:%S GMT";

#define BIT_COUNT_WORDS 2
#define BIT_COUNT_BYTES (BIT_COUNT_WORDS * sizeof(uint32_t))

/*
 * define the rotate left (circular left shift) operation
 */
#define rotl(v, b) (((v) << (b)) | ((v) >> (32 - (b))))

/*
 * Define the basic SHA-1 functions F1 ~ F4. Note that the exclusive-OR
 * operation (^) in F1 and F3 may be replaced by a bitwise OR operation
 * (|), which produce identical results.
 *
 * F1 is used in ROUND  0~19, F2 is used in ROUND 20~39
 * F3 is used in ROUND 40~59, F4 is used in ROUND 60~79
 */
#define F1(B, C, D) (((B) & (C)) ^ (~(B) & (D)))
#define F2(B, C, D) ((B) ^ (C) ^ (D))
#define F3(B, C, D) (((B) & (C)) ^ ((B) & (D)) ^ ((C) & (D)))
#define F4(B, C, D) ((B) ^ (C) ^ (D))

/*
 * Use different K in different ROUND
 */
#define K00_19 0x5A827999
#define K20_39 0x6ED9EBA1
#define K40_59 0x8F1BBCDC
#define K60_79 0xCA62C1D6

/*
 * Another implementation of the ROUND transformation:
 * (here the T is a temp variable)
 * For t=0 to 79:
 * {
 *     T=rotl(A,5)+Func(B,C,D)+K+W[t]+E;
 *     E=D; D=C; C=rotl(B,30); B=A; A=T;
 * }
 */
#define ROUND(t, A, B, C, D, E, Func, K) \
    E += rotl(A, 5) + Func(B, C, D) + W[t] + K; \
    B = rotl(B, 30);

#define ROUND5(t, Func, K) \
    ROUND(t, A, B, C, D, E, Func, K); \
    ROUND(t + 1, E, A, B, C, D, Func, K); \
    ROUND(t + 2, D, E, A, B, C, Func, K); \
    ROUND(t + 3, C, D, E, A, B, Func, K); \
    ROUND(t + 4, B, C, D, E, A, Func, K)

#define ROUND20(t, Func, K) \
    ROUND5(t, Func, K); \
    ROUND5(t + 5, Func, K); \
    ROUND5(t + 10, Func, K); \
    ROUND5(t + 15, Func, K)

/*
 * Define constant of the initial vector
 */
const uint32_t SHA1::IV[SHA1_DIGEST_WORDS] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

/*
 * the message must be the big-endian32 (or left-most word)
 * before calling the transform() function.
 */
const static uint32_t iii = 1;
const static bool littleEndian = *(uint8_t*)&iii != 0;

inline uint32_t littleEndianToBig(uint32_t d) {
    uint8_t* data = (uint8_t*)&d;
    return data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
}

inline void make_big_endian32(uint32_t* data, unsigned n) {
    if (!littleEndian) {
        return;
    }
    for (; n > 0; ++data, --n) {
        *data = littleEndianToBig(*data);
    }
}

inline size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

void SHA1::transform() {
    uint32_t W[80];
    memcpy(W, M, SHA1_INPUT_BYTES);
    memset((uint8_t*)W + SHA1_INPUT_BYTES, 0, sizeof(W) - SHA1_INPUT_BYTES);
    for (unsigned t = 16; t < 80; t++) {
        W[t] = rotl(W[t - 16] ^ W[t - 14] ^ W[t - 8] ^ W[t - 3], 1);
    }

    uint32_t A = H[0];
    uint32_t B = H[1];
    uint32_t C = H[2];
    uint32_t D = H[3];
    uint32_t E = H[4];

    ROUND20(0, F1, K00_19);
    ROUND20(20, F2, K20_39);
    ROUND20(40, F3, K40_59);
    ROUND20(60, F4, K60_79);

    H[0] += A;
    H[1] += B;
    H[2] += C;
    H[3] += D;
    H[4] += E;
}

void SHA1::add(const uint8_t* data, size_t data_len) {
    unsigned mlen = (unsigned)((bits >> 3) % SHA1_INPUT_BYTES);
    bits += (uint64_t)data_len << 3;
    unsigned use = (unsigned)min((size_t)(SHA1_INPUT_BYTES - mlen), data_len);
    memcpy(M + mlen, data, use);
    mlen += use;

    while (mlen == SHA1_INPUT_BYTES) {
        data_len -= use;
        data += use;
        make_big_endian32((uint32_t*)M, SHA1_INPUT_WORDS);
        transform();
        use = (unsigned)min((size_t)SHA1_INPUT_BYTES, data_len);
        memcpy(M, data, use);
        mlen = use;
    }
}

uint8_t* SHA1::result() {
    unsigned mlen = (unsigned)((bits >> 3) % SHA1_INPUT_BYTES), padding = SHA1_INPUT_BYTES - mlen;
    M[mlen++] = 0x80;
    if (padding > BIT_COUNT_BYTES) {
        memset(M + mlen, 0x00, padding - BIT_COUNT_BYTES);
        make_big_endian32((uint32_t*)M, SHA1_INPUT_WORDS - BIT_COUNT_WORDS);
    } else {
        memset(M + mlen, 0x00, SHA1_INPUT_BYTES - mlen);
        make_big_endian32((uint32_t*)M, SHA1_INPUT_WORDS);
        transform();
        memset(M, 0x00, SHA1_INPUT_BYTES - BIT_COUNT_BYTES);
    }

    uint64_t temp = littleEndian ? bits << 32 | bits >> 32 : bits;
    memcpy(M + SHA1_INPUT_BYTES - BIT_COUNT_BYTES, &temp, BIT_COUNT_BYTES);
    transform();
    make_big_endian32(H, SHA1_DIGEST_WORDS);
    return (uint8_t*)H;
}

template <typename T>
inline void axor(T* p1, const T* p2, size_t len) {
    for (; len != 0; --len)
        *p1++ ^= *p2++;
}

HMAC::HMAC(const uint8_t* key, size_t lkey) {
    init(key, lkey);
}

void HMAC::init(const uint8_t* key, size_t lkey) {
    in.init();
    out.init();

    uint8_t ipad[SHA1_INPUT_BYTES];
    uint8_t opad[SHA1_INPUT_BYTES];
    memset(ipad, 0x36, sizeof(ipad));
    memset(opad, 0x5c, sizeof(opad));

    if (lkey <= SHA1_INPUT_BYTES) {
        axor(ipad, key, lkey);
        axor(opad, key, lkey);
    } else {
        SHA1 tmp;
        tmp.add(key, lkey);
        const uint8_t* key2 = tmp.result();
        axor(ipad, key2, SHA1_DIGEST_BYTES);
        axor(opad, key2, SHA1_DIGEST_BYTES);
    }

    in.add((uint8_t*)ipad, sizeof(ipad));
    out.add((uint8_t*)opad, sizeof(opad));
}

string GetDateString() {
    time_t now_time;
    time(&now_time);
    if (AppConfig::GetInstance()->EnableLogTimeAutoAdjust()) {
        now_time += GetTimeDelta();
    }
    char buffer[128] = {'\0'};
    tm timeInfo;
#if defined(__linux__)
    gmtime_r(&now_time, &timeInfo);
#elif defined(_MSC_VER)
    gmtime_s(&timeInfo, &now_time);
#endif
    strftime(buffer, 128, DATE_FORMAT_RFC822.c_str(), &timeInfo);
    return string(buffer);
}

static bool StartWith(const std::string& input, const std::string& pattern) {
    if (input.length() < pattern.length()) {
        return false;
    }

    size_t i = 0;
    while (i < pattern.length() && input[i] == pattern[i]) {
        i++;
    }

    return i == pattern.length();
}

static std::string CalcSHA1(const std::string& message, const std::string& key) {
    HMAC hmac(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    hmac.add(reinterpret_cast<const uint8_t*>(message.data()), message.size());
    return string(reinterpret_cast<const char*>(hmac.result()), SHA1_DIGEST_BYTES);
}

string GetUrlSignature(const string& httpMethod,
                       const string& operationType,
                       map<string, string>& httpHeader,
                       const map<string, string>& parameterList,
                       const string& content,
                       const string& signKey) {
    string contentMd5;
    string signature;
    string osstream;
    if (!content.empty()) {
        contentMd5 = CalcMD5(content);
    }
    string contentType;
    map<string, string>::iterator iter = httpHeader.find(CONTENT_TYPE);
    if (iter != httpHeader.end()) {
        contentType = iter->second;
    }
    map<string, string> endingMap;
    osstream.append(httpMethod);
    osstream.append("\n");
    osstream.append(contentMd5);
    osstream.append("\n");
    osstream.append(contentType);
    osstream.append("\n");
    osstream.append(httpHeader[DATE]);
    osstream.append("\n");
    for (map<string, string>::const_iterator iter = httpHeader.begin(); iter != httpHeader.end(); ++iter) {
        if (StartWith(iter->first, LOG_OLD_HEADER_PREFIX)) {
            string key = iter->first;
            endingMap.insert(make_pair(key.replace(0, LOG_OLD_HEADER_PREFIX.size(), LOG_HEADER_PREFIX), iter->second));
        } else if (StartWith(iter->first, LOG_HEADER_PREFIX) || StartWith(iter->first, ACS_HEADER_PREFIX)) {
            endingMap.insert(make_pair(iter->first, iter->second));
        }
    }
    for (map<string, string>::const_iterator it = endingMap.begin(); it != endingMap.end(); ++it) {
        osstream.append(it->first);
        osstream.append(":");
        osstream.append(it->second);
        osstream.append("\n");
    }
    osstream.append(operationType);
    if (parameterList.size() > 0) {
        osstream.append("?");
        for (map<string, string>::const_iterator iter = parameterList.begin(); iter != parameterList.end(); ++iter) {
            if (iter != parameterList.begin()) {
                osstream.append("&");
            }
            osstream.append(iter->first);
            osstream.append("=");
            osstream.append(iter->second);
        }
    }

    signature = Base64Enconde(CalcSHA1(osstream, signKey));

    return signature;
}

} // namespace logtail
