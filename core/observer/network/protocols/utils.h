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

#include <endian.h>
#include <vector>
#include <iostream>
#include <arpa/inet.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <map>
#include <cstring>

namespace logtail {
struct SlsStringPiece {
    const char* mPtr;
    size_t mLen;

    SlsStringPiece(const char* ptr, size_t len) : mPtr(ptr), mLen(len) {}

    explicit SlsStringPiece(const std::string& s) : mPtr(s.data()), mLen(s.size()) {}

    SlsStringPiece() : mPtr(nullptr), mLen(0) {}

    inline bool operator==(const std::string& targetStr) const {
        return (this->mLen == targetStr.size()) && (memcmp(this->mPtr, targetStr.data(), this->mLen) == 0);
    }

    inline bool operator==(const SlsStringPiece& targetStr) const {
        return (this->mLen == targetStr.mLen) && (memcmp(this->mPtr, targetStr.mPtr, this->mLen) == 0);
    }

    inline uint32_t Size() { return this->mLen; }

    inline char operator[](size_t pos) const { return this->mPtr[pos]; }


    inline bool StartWith(const std::string& prefix) const {
        return (prefix.length() <= mLen) && (memcmp(this->mPtr, prefix.data(), prefix.length()) == 0);
    }

    std::string TrimToString() const {
        if (mPtr == nullptr) {
            return {};
        }
        const char* start = mPtr;
        int preEmptyCount = 0;
        for (uint32_t i = 0; i < mLen; i++) {
            if (start[i] != ' ' && start[i] != '\t') {
                start = start + i;
                break;
            }
            preEmptyCount++;
        }
        int postEmptyCount = 0;
        for (uint32_t i = mLen - 1; i > 0; i--) {
            if (mPtr[i] != ' ' && mPtr[i] != '\t') {
                break;
            }
            postEmptyCount++;
        }
        return std::string(start, mLen - preEmptyCount - postEmptyCount);
    }

    int Find(char c) const {
        if (mPtr == nullptr) {
            return -1;
        }
        for (int i = 0; i < (int)mLen; ++i) {
            if (mPtr[i] == c) {
                return i;
            }
        }
        return -1;
    }

    std::string ToString() const {
        if (mPtr == nullptr) {
            return {};
        }
        return {mPtr, mLen};
    }

    bool operator<(const SlsStringPiece& other) const {
        if (mLen == 0 || other.mLen == 0) {
            return mLen < other.mLen;
        }
        uint32_t i = 0;
        while (mPtr[i] == other.mPtr[i]) {
            i++;
            if (i == mLen || i == other.mLen) {
                return mLen < other.mLen;
            }
        }
        return mPtr[i] < other.mPtr[i];
    }
};

inline void hexstring_to_bin(std::string s, std::vector<uint8_t>& dest) {
    auto p = s.data();
    auto end = p + s.length();

    while (p < end) {
        const char hexbytestr[] = {*p, *(p + 1), 0};
        uint8_t val = strtol(hexbytestr, NULL, 16);
        dest.push_back(val);
        p += 2;
    }
}

inline std::string charToHexString(const char* payload, const int32_t len, const int32_t limit) {
    std::stringstream ioss;
    std::string hexstring;
    for (int i = 0; i < len && i < limit; i++) {
        ioss << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)payload[i];
    }
    ioss >> hexstring;
    return hexstring;
}

class ProtoParser {
public:
    ProtoParser(const char* payload, const size_t pktSize, bool bigByteOrder)
        : currPostion(0), isParseFail(false), payload(payload), pktSize(pktSize), bigByteOrder(bigByteOrder) {
        parserFailMsg = std::string();
        //  needByteOrderCovert = isCPUBigByteOrder() != bigByteOrder;
    }

    ~ProtoParser() {}

    bool OK() { return !isParseFail; }

    uint64_t readUint64(bool commit = true) {
        if (this->currPostion + 8 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return 0;
        }
        uint64_t data = *((uint64_t*)(payload + this->currPostion));
        if (commit) {
            this->currPostion += 8;
        }

        if (bigByteOrder) {
            return be64toh(data);
        }
        return le64toh(data);
    }

    uint32_t readUint32(bool commit = true) {
        if (this->currPostion + 4 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return 0;
        }
        uint32_t data = *((uint32_t*)(payload + this->currPostion));
        if (commit) {
            this->currPostion += 4;
        }

        if (bigByteOrder) {
            return be32toh(data);
        }
        return le32toh(data);
    }

    uint16_t readUint16(bool commit = true) {
        if (this->currPostion + 2 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return 0;
        }
        uint16_t data = *((uint16_t*)(payload + this->currPostion));
        if (commit) {
            this->currPostion += 2;
        }

        if (bigByteOrder) {
            return be16toh(data);
        }
        return le16toh(data);
    }

    uint8_t readUint8(bool commit = true) {
        if (this->currPostion + 1 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return 0;
        }
        uint8_t data = *((uint8_t*)(payload + this->currPostion));
        if (commit) {
            this->currPostion += 1;
        }
        return data;
    }

    const char* readChar(bool commit = true) {
        if (this->currPostion + 1 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return 0;
        }
        const char* data = payload + this->currPostion;
        if (commit) {
            this->currPostion += 1;
        }
        return data;
    }

    SlsStringPiece readUntil(char flag, bool commit = true) {
        if (this->currPostion + 1 > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return {};
        }
        size_t len = 0;
        uint32_t pos = currPostion;
        while (pos + len + 1 <= this->pktSize && *(payload + pos + len) != flag) {
            len++;
        }
        if (commit && pos + 1 <= this->pktSize) {
            this->currPostion = pos + len + 1;
        }
        return {payload + pos, len};
    }


    void positionCommit(uint64_t step) {
        if (this->currPostion + step > this->pktSize) {
            this->setParseFail("unexcepted eof");
            return;
        }
        this->currPostion += step;
    }

    bool isNextEof() {
        if (this->currPostion + 1 >= this->pktSize) {
            return true;
        } else {
            return false;
        }
    }

    int32_t getLeftSize() { return pktSize - this->currPostion; }

protected:
    void setParseFail(const std::string& failMsg) {
        this->isParseFail = true;
        this->parserFailMsg = failMsg;
        throw std::runtime_error("unexcepted eof");
    }

protected:
    // common fields

    uint32_t currPostion;

    std::string parserFailMsg;

    bool isParseFail;

    const char* payload;

    const size_t pktSize;

    // bool needByteOrderCovert;
    bool bigByteOrder;
};

} // end of namespace logtail