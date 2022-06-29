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

#include "HashUtil.h"
#include <memory.h>
#include "FileSystemUtil.h"
#include "murmurhash3.h"
#include "LogFileOperator.h"

namespace logtail {

/////////////////////////////////////////////// MACRO //////////////////////////////////////////////////
#define SHIFT_LEFT(a, b) ((a) << (b) | (a) >> (32 - b))

/**
 * each operation
 */
#define F(b, c, d) (((b) & (c)) | ((~(b)) & (d)))
#define G(b, c, d) (((d) & (b)) | ((~(d)) & (c)))
#define H(b, c, d) ((b) ^ (c) ^ (d))
#define I(b, c, d) ((c) ^ ((b) | (~(d))))

/**
 * each round
 */
#define FF(a, b, c, d, word, shift, k) \
    { \
        (a) += F((b), (c), (d)) + (word) + (k); \
        (a) = SHIFT_LEFT((a), (shift)); \
        (a) += (b); \
    }
#define GG(a, b, c, d, word, shift, k) \
    { \
        (a) += G((b), (c), (d)) + (word) + (k); \
        (a) = SHIFT_LEFT((a), (shift)); \
        (a) += (b); \
    }
#define HH(a, b, c, d, word, shift, k) \
    { \
        (a) += H((b), (c), (d)) + (word) + (k); \
        (a) = SHIFT_LEFT((a), (shift)); \
        (a) += (b); \
    }
#define II(a, b, c, d, word, shift, k) \
    { \
        (a) += I((b), (c), (d)) + (word) + (k); \
        (a) = SHIFT_LEFT((a), (shift)); \
        (a) += (b); \
    }
////////////////////////////////////////////////////////// GLOBAL VARIABLE /////////////////////////////////////////////////////////
const uint8_t gPadding[64]
    = {0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
       0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
       0x0,  0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

//////////////////////////////////////////////////////// LOCAL DECLEARATION //////////////////////////////////////////////////////////
struct Md5Block {
    uint32_t word[16];
};
/**
 * copy a pool into a block, using little endian
 */
void CopyBytesToBlock(const uint8_t* poolIn, struct Md5Block& block) {
    uint32_t j = 0;
    for (uint32_t i = 0; i < 32; ++i, j += 4) {
        block.word[i] = ((uint32_t)poolIn[j]) | (((uint32_t)poolIn[j + 1]) << 8) | (((uint32_t)poolIn[j + 2]) << 16)
            | (((uint32_t)poolIn[j + 3]) << 24);
    }
}

/**
 * calculate md5 hash value from a block
 */
void CalMd5(struct Md5Block block, uint32_t h[4]) {
    uint32_t a = h[0];
    uint32_t b = h[1];
    uint32_t c = h[2];
    uint32_t d = h[3];

    // Round 1
    FF(a, b, c, d, block.word[0], 7, 0xd76aa478);
    FF(d, a, b, c, block.word[1], 12, 0xe8c7b756);
    FF(c, d, a, b, block.word[2], 17, 0x242070db);
    FF(b, c, d, a, block.word[3], 22, 0xc1bdceee);
    FF(a, b, c, d, block.word[4], 7, 0xf57c0faf);
    FF(d, a, b, c, block.word[5], 12, 0x4787c62a);
    FF(c, d, a, b, block.word[6], 17, 0xa8304613);
    FF(b, c, d, a, block.word[7], 22, 0xfd469501);
    FF(a, b, c, d, block.word[8], 7, 0x698098d8);
    FF(d, a, b, c, block.word[9], 12, 0x8b44f7af);
    FF(c, d, a, b, block.word[10], 17, 0xffff5bb1);
    FF(b, c, d, a, block.word[11], 22, 0x895cd7be);
    FF(a, b, c, d, block.word[12], 7, 0x6b901122);
    FF(d, a, b, c, block.word[13], 12, 0xfd987193);
    FF(c, d, a, b, block.word[14], 17, 0xa679438e);
    FF(b, c, d, a, block.word[15], 22, 0x49b40821);

    // Round 2
    GG(a, b, c, d, block.word[1], 5, 0xf61e2562);
    GG(d, a, b, c, block.word[6], 9, 0xc040b340);
    GG(c, d, a, b, block.word[11], 14, 0x265e5a51);
    GG(b, c, d, a, block.word[0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, block.word[5], 5, 0xd62f105d);
    GG(d, a, b, c, block.word[10], 9, 0x2441453);
    GG(c, d, a, b, block.word[15], 14, 0xd8a1e681);
    GG(b, c, d, a, block.word[4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, block.word[9], 5, 0x21e1cde6);
    GG(d, a, b, c, block.word[14], 9, 0xc33707d6);
    GG(c, d, a, b, block.word[3], 14, 0xf4d50d87);
    GG(b, c, d, a, block.word[8], 20, 0x455a14ed);
    GG(a, b, c, d, block.word[13], 5, 0xa9e3e905);
    GG(d, a, b, c, block.word[2], 9, 0xfcefa3f8);
    GG(c, d, a, b, block.word[7], 14, 0x676f02d9);
    GG(b, c, d, a, block.word[12], 20, 0x8d2a4c8a);

    // Round 3
    HH(a, b, c, d, block.word[5], 4, 0xfffa3942);
    HH(d, a, b, c, block.word[8], 11, 0x8771f681);
    HH(c, d, a, b, block.word[11], 16, 0x6d9d6122);
    HH(b, c, d, a, block.word[14], 23, 0xfde5380c);
    HH(a, b, c, d, block.word[1], 4, 0xa4beea44);
    HH(d, a, b, c, block.word[4], 11, 0x4bdecfa9);
    HH(c, d, a, b, block.word[7], 16, 0xf6bb4b60);
    HH(b, c, d, a, block.word[10], 23, 0xbebfbc70);
    HH(a, b, c, d, block.word[13], 4, 0x289b7ec6);
    HH(d, a, b, c, block.word[0], 11, 0xeaa127fa);
    HH(c, d, a, b, block.word[3], 16, 0xd4ef3085);
    HH(b, c, d, a, block.word[6], 23, 0x4881d05);
    HH(a, b, c, d, block.word[9], 4, 0xd9d4d039);
    HH(d, a, b, c, block.word[12], 11, 0xe6db99e5);
    HH(c, d, a, b, block.word[15], 16, 0x1fa27cf8);
    HH(b, c, d, a, block.word[2], 23, 0xc4ac5665);

    // Round 4
    II(a, b, c, d, block.word[0], 6, 0xf4292244);
    II(d, a, b, c, block.word[7], 10, 0x432aff97);
    II(c, d, a, b, block.word[14], 15, 0xab9423a7);
    II(b, c, d, a, block.word[5], 21, 0xfc93a039);
    II(a, b, c, d, block.word[12], 6, 0x655b59c3);
    II(d, a, b, c, block.word[3], 10, 0x8f0ccc92);
    II(c, d, a, b, block.word[10], 15, 0xffeff47d);
    II(b, c, d, a, block.word[1], 21, 0x85845dd1);
    II(a, b, c, d, block.word[8], 6, 0x6fa87e4f);
    II(d, a, b, c, block.word[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, block.word[6], 15, 0xa3014314);
    II(b, c, d, a, block.word[13], 21, 0x4e0811a1);
    II(a, b, c, d, block.word[4], 6, 0xf7537e82);
    II(d, a, b, c, block.word[11], 10, 0xbd3af235);
    II(c, d, a, b, block.word[2], 15, 0x2ad7d2bb);
    II(b, c, d, a, block.word[9], 21, 0xeb86d391);

    // Add to hash value
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
}


void DoMd5Little(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t hash[16]) {
    struct Md5Block block;

    ///initialize hash value
    uint32_t h[4];
    h[0] = 0x67452301;
    h[1] = 0xEFCDAB89;
    h[2] = 0x98BADCFE;
    h[3] = 0x10325476;

    ///padding and divide input data into blocks
    uint64_t fullLen = (inputBytesNum >> 6) << 6; ///complete blocked length
    uint64_t partLen = inputBytesNum & 0x3F; ///length remained

    uint32_t i;
    for (i = 0; i < fullLen; i += 64) {
        ///copy input data into block
        memcpy(block.word, &(poolIn[i]), 64);

        ///calculate Md5
        CalMd5(block, h);
    }


    if (partLen > 55) ///append two more blocks
    {
        ///copy input data into block and pad
        memcpy(block.word, &(poolIn[i]), partLen);
        memcpy(((uint8_t*)&(block.word[partLen >> 2])) + (partLen & 0x3), gPadding, (64 - partLen));

        ///calculate Md5
        CalMd5(block, h);

        ///set rest byte to 0x0
        memset(block.word, 0x0, 64);
    } else ///append one more block
    {
        ///copy input data into block and pad
        memcpy(block.word, &(poolIn[i]), partLen);
        memcpy(((uint8_t*)&(block.word[partLen >> 2])) + (partLen & 0x3), gPadding, (64 - partLen));
    }

    ///append length (bits)
    uint64_t bitsNum = inputBytesNum * 8;
    memcpy(&(block.word[14]), &bitsNum, 8);

    ///calculate Md5
    CalMd5(block, h);

    ///clear sensitive information
    memset(block.word, 0, 64);

    ///fill hash value
    memcpy(&(hash[0]), &(h[0]), 16);
} ///DoMd5Little

void DoMd5Big(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t hash[16]) {
    struct Md5Block block;
    uint8_t tempBlock[64];

    ///initialize hash value
    uint32_t h[4];
    h[0] = 0x67452301;
    h[1] = 0xEFCDAB89;
    h[2] = 0x98BADCFE;
    h[3] = 0x10325476;

    ///padding and divide input data into blocks
    uint64_t fullLen = (inputBytesNum >> 6) << 6;
    uint64_t partLen = inputBytesNum & 0x3F;

    uint32_t i;
    for (i = 0; i < fullLen; i += 64) {
        ///copy input data into block, in little endian
        CopyBytesToBlock(&(poolIn[i]), block);

        ///calculate Md5
        CalMd5(block, h);
    }

    ///append two more blocks
    if (partLen > 55) {
        ///put input data into a temporary block
        memcpy(tempBlock, &(poolIn[i]), partLen);
        memcpy(&(tempBlock[partLen]), gPadding, (64 - partLen));

        ///copy temporary data into block, in little endian
        CopyBytesToBlock(tempBlock, block);

        ///calculate Md5
        CalMd5(block, h);

        memset(tempBlock, 0x0, 64);
    }
    ///append one more block
    else {
        memcpy(tempBlock, &(poolIn[i]), partLen);
        memcpy(&(tempBlock[partLen]), gPadding, (64 - partLen));
    }
    ///append length (bits)
    uint64_t bitsNum = inputBytesNum * 8;
    memcpy(&(tempBlock[56]), &bitsNum, 8);

    ///copy temporary data into block, in little endian
    CopyBytesToBlock(tempBlock, block);

    ///calculate Md5
    CalMd5(block, h);

    ///clear sensitive information
    memset(block.word, 0, 64);
    memset(tempBlock, 0, 64);

    ///fill hash value
    memcpy(&(hash[0]), &(h[0]), 16);
} ///DoMd5Big

void DoMd5(const uint8_t* poolIn, const uint64_t inputBytesNum, uint8_t md5[16]) {
    ///detect big or little endian
    union {
        uint32_t a;
        uint8_t b;
    } symbol;

    symbol.a = 1;

    ///for little endian
    if (symbol.b == 1) {
        DoMd5Little(poolIn, inputBytesNum, md5);
    }
    ///for big endian
    else {
        DoMd5Big(poolIn, inputBytesNum, md5);
    }
} ///DoMd5

bool SignatureToHash(const std::string& signature, uint64_t& sigHash, uint32_t& sigSize) {
    sigSize = (uint32_t)signature.size();
    sigHash = (uint64_t)HashSignatureString(signature.c_str(), signature.size());
    return true;
}

bool CheckAndUpdateSignature(const std::string& signature, uint64_t& sigHash, uint32_t& sigSize) {
    if (sigSize > signature.size()) {
        sigSize = (uint32_t)signature.size();
        sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
        return false;
    }
    if (sigSize == 0) {
        sigSize = (uint32_t)signature.size();
        sigHash = (uint64_t)HashSignatureString(signature.c_str(), signature.size());
        return true;
    }
    uint64_t newSigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
    bool rst = newSigHash == sigHash;
    if (sigSize != signature.size()) {
        sigSize = (uint32_t)signature.size();
        sigHash = (uint64_t)HashSignatureString(signature.c_str(), (size_t)sigSize);
    } else if (!rst) {
        sigHash = newSigHash;
    }
    return rst;
}

bool CheckFileSignature(const std::string& filePath, uint64_t sigHash, uint32_t sigSize, bool fuseMode) {
    LogFileOperator logFileOp;
    logFileOp.Open(filePath.c_str(), fuseMode);
    if (!logFileOp.IsOpen()) {
        return false;
    }
    char sigStr[1025];
    int readSize = logFileOp.Pread(sigStr, 1, sigSize, 0);
    if (readSize < 0) {
        return false;
    }
    sigStr[readSize] = '\0';
    return CheckAndUpdateSignature(std::string(sigStr), sigHash, sigSize);
}

int64_t HashString(const std::string& data) {
    int64_t hval = (int64_t)0xcbf29ce484222325ULL;
    const char* buf = data.c_str();
    size_t len = data.size();
    const char* bp = buf;
    const char* be = bp + len;
    while (bp < be) {
        hval ^= (uint64_t)*bp++;
        hval += (hval << 1) + (hval << 4) + (hval << 5) + (hval << 7) + (hval << 8) + (hval << 40);
    }
    return hval;
}

int64_t HashSignatureString(const char* str, size_t strLen) {
    char hashVal[16];
    MurmurHash3_x64_128(str, strLen, 0xdeadbeaf, hashVal);
    return *(int64_t*)hashVal;
}

} // namespace logtail
