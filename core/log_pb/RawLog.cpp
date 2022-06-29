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

#include "RawLog.h"
#include "common/Flags.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(raw_log_init_buffer_size, "", 256);

namespace logtail
{

// 1+5( 1 --->  header;  5 ---> uint32)
#define INIT_LOG_SIZE_BYTES 6L

/**
 * Return the number of bytes required to store a variable-length unsigned
 * 32-bit integer in base-128 varint encoding.
 *
 * \param v
 *      Value to encode.
 * \return
 *      Number of bytes required.
 */
static inline size_t uint32_size(uint32_t v)
{
    if (v < (1UL << 7)) {
        return 1;
    } else if (v < (1UL << 14)) {
        return 2;
    } else if (v < (1UL << 21)) {
        return 3;
    } else if (v < (1UL << 28)) {
        return 4;
    } else {
        return 5;
    }
}

/**
 * Pack an unsigned 32-bit integer in base-128 varint encoding and return the
 * number of bytes written, which must be 5 or less.
 *
 * \param value
 *      Value to encode.
 * \param[out] out
 *      Packed value.
 * \return
 *      Number of bytes written to `out`.
 */
static inline size_t uint32_pack(uint32_t value, uint8_t *out)
{
    unsigned rv = 0;

    if (value >= 0x80) {
        out[rv++] = value | 0x80;
        value >>= 7;
        if (value >= 0x80) {
            out[rv++] = value | 0x80;
            value >>= 7;
            if (value >= 0x80) {
                out[rv++] = value | 0x80;
                value >>= 7;
                if (value >= 0x80) {
                    out[rv++] = value | 0x80;
                    value >>= 7;
                }
            }
        }
    }
    /* assert: value<128 */
    out[rv++] = value;
    return rv;
}

static inline uint32_t parse_uint32(unsigned len, const uint8_t *data)
{
    uint32_t rv = data[0] & 0x7f;
    if (len > 1) {
        rv |= ((uint32_t) (data[1] & 0x7f) << 7);
        if (len > 2) {
            rv |= ((uint32_t) (data[2] & 0x7f) << 14);
            if (len > 3) {
                rv |= ((uint32_t) (data[3] & 0x7f) << 21);
                if (len > 4)
                    rv |= ((uint32_t) (data[4]) << 28);
            }
        }
    }
    return rv;
}

static unsigned scan_varint(unsigned len, const uint8_t *data)
{
    unsigned i;
    if (len > 10)
        len = 10;
    for (i = 0; i < len; i++)
        if ((data[i] & 0x80) == 0)
            break;
    if (i == len)
        return 0;
    return i + 1;
}

RawLog *RawLog::AddLogFull(uint32_t logTime, std::vector<std::string> keys,
                           boost::match_results<const char *> subMathValues)
{
    // limit logTime's min value, ensure varint size is 5
    if (logTime < 463563523)
    {
        logTime = 463563523;
    }

    int32_t i = 0;
    uint32_t logSize = 6;
    int32_t pair_count = (int32_t)keys.size();
    for (; i < pair_count; ++i)
    {
        auto keyLen = uint32_t(keys[i].size());
        auto valLen = uint32_t(subMathValues[i+1].length());
        uint32_t contSize = uint32_size(keyLen) + uint32_size(valLen) + keyLen + valLen + 2u;
        logSize += 1 + uint32_size(contSize) + contSize;
    }
    uint32_t headerSize = 1 + uint32_size(logSize);
    int32_t deltaSize = INIT_LOG_SIZE_BYTES - headerSize;
    uint32_t totalBufferSize = logSize + headerSize;

    auto * rawLog = new RawLog();
    rawLog->mRawLen = totalBufferSize + deltaSize;
    rawLog->mRawBuffer = (uint8_t *)malloc(totalBufferSize + deltaSize);
    rawLog->mMaxLen = totalBufferSize + deltaSize;

    uint8_t * buf = (uint8_t*)rawLog->mRawBuffer + deltaSize;

    *buf++ = 0x0A;
    buf += uint32_pack(logSize, buf);

    // time
    *buf++=0x08;
    buf += uint32_pack(logTime, buf);

    // Content
    // header
    i = 0;
    for (; i < pair_count; ++i)
    {
        auto keyLen = uint32_t(keys[i].size());
        auto valLen = uint32_t(subMathValues[i+1].length());
        *buf++ = 0x12;
        buf += uint32_pack(uint32_size(keyLen) + uint32_size(valLen) + 2 + keyLen + valLen, buf);
        *buf++ = 0x0A;
        buf += uint32_pack(keyLen, buf);
        memcpy(buf, keys[i].c_str(), keyLen);
        buf += keyLen;
        *buf++ = 0x12;
        buf += uint32_pack(valLen, buf);
        memcpy(buf, subMathValues[i+1].first, valLen);
        buf += valLen;
    }
    rawLog->mMallocDelta = deltaSize;
    assert(buf - rawLog->mRawBuffer == totalBufferSize + rawLog->mMallocDelta);
    rawLog->mNowBuffer = buf;
    return rawLog;
}

void RawLog::AddTime(uint32_t logTime)
{
    // limit logTime's min value, ensure varint size is 5
    if (logTime < 463563523)
    {
        logTime = 463563523;
    }
    // 1 header and 5 time
    if (mRawLen + 6 > mMaxLen)
    {
        // reset log_now_buffer
        adjustBuffer(6);
    }

    // time
    *mNowBuffer++=0x08;
    mNowBuffer += uint32_pack(logTime, mNowBuffer);
    mRawLen += 6;
}

uint32_t RawLog::AddKeyValue(const char *key, size_t key_len, const char *value,
                         size_t value_len)
{
    // sum total size
    uint32_t kv_size = sizeof(char) * (key_len + value_len) + uint32_size((uint32_t)key_len) + uint32_size((uint32_t)value_len) + 2;
    kv_size += 1 + uint32_size(kv_size);
    // ensure buffer size
    if (mRawLen + kv_size > mMaxLen)
    {
        // reset log_now_buffer
        adjustBuffer(kv_size);
    }
    mRawLen += kv_size;
    uint8_t * buf = mNowBuffer;
    // key_value header
    *buf++ = 0x12;
    buf += uint32_pack(uint32_size(key_len) + uint32_size(value_len) + 2 + key_len + value_len, buf);
    // key len
    *buf++ = 0x0A;
    buf += uint32_pack(key_len, buf);
    // key
    memcpy(buf, key, key_len);
    buf += key_len;
    // value len
    *buf++ = 0x12;
    buf += uint32_pack(value_len, buf);
    // value
    memcpy(buf, value, value_len);
    buf += value_len;
    mNowBuffer = (uint8_t *)buf;
    return kv_size;
}


void RawLog::AddLogStart(uint32_t logTime)
{
    if (mRawLen + INIT_LOG_SIZE_BYTES > mMaxLen)
    {
        adjustBuffer(INIT_LOG_SIZE_BYTES);
    }
    mRawLen = INIT_LOG_SIZE_BYTES;
    mNowBuffer += INIT_LOG_SIZE_BYTES;
    AddTime(logTime);
}

void RawLog::AddLogDone()
{
    uint32_t log_size = mRawLen - INIT_LOG_SIZE_BYTES;
    // check total size and uint32_size(total size)

    int32_t header_size = uint32_size(log_size) + 1;

    if (header_size != INIT_LOG_SIZE_BYTES)
    {
        mMallocDelta = (int8_t)(INIT_LOG_SIZE_BYTES - header_size);
    }
    // set log header
    uint8_t * buf = (mRawBuffer + mMallocDelta);
    *buf++ = 0x0A;
    uint32_pack(log_size, buf);
}

void RawLog::adjustBuffer(uint32_t newLen)
{
    if (mRawBuffer == NULL)
    {
        mRawBuffer = (uint8_t *)malloc(INT32_FLAG(raw_log_init_buffer_size));
        mNowBuffer = mRawBuffer;
        mMaxLen = INT32_FLAG(raw_log_init_buffer_size);
        return;
    }
    uint32_t new_buffer_len = mMaxLen << 1;

    if (new_buffer_len < mMaxLen + newLen)
    {
        new_buffer_len = mMaxLen + newLen;
    }

    mRawBuffer = (uint8_t *)realloc(mRawBuffer, new_buffer_len);
    mMaxLen = new_buffer_len;
    mNowBuffer = mRawBuffer + mRawLen;
}

void RawLog::AppendToString(std::string *output) const
{
    output->append((const char *)mRawBuffer+mMallocDelta, mRawLen-mMallocDelta);
}

bool RawLog::NextKeyValue(RawLog::iterator &iter, const char *&key, uint32_t &keyLen,
                     const char *&value, uint32_t &valueLen)
{
    if (iter.mNowOffset >= (int32_t)mRawLen)
    {
        return false;
    }
    assert(mRawBuffer[iter.mNowOffset] == 0x12);
    iter.mLastOffset = iter.mNowOffset;
    // skip log content
    ++iter.mNowOffset;
    uint32_t keyValueLenSize = scan_varint(5, mRawBuffer + iter.mNowOffset);
    iter.mNowOffset += keyValueLenSize;
    // parse key
    ++iter.mNowOffset;
    uint32_t keyLenSize = scan_varint(5, mRawBuffer + iter.mNowOffset);
    keyLen = parse_uint32(keyLenSize, mRawBuffer + iter.mNowOffset);
    iter.mNowOffset += keyLenSize;
    key = (const char *)mRawBuffer + iter.mNowOffset;
    iter.mNowOffset += keyLen;
    // parse value
    ++iter.mNowOffset;
    uint32_t valueLenSize = scan_varint(5, mRawBuffer + iter.mNowOffset);
    valueLen = parse_uint32(valueLenSize, mRawBuffer + iter.mNowOffset);
    iter.mNowOffset += valueLenSize;
    value = (const char *)mRawBuffer + iter.mNowOffset;
    iter.mNowOffset += valueLen;
    return true;
}

bool RawLog::DeleteKeyValue(RawLog::iterator &iter)
{
    if (iter.mLastOffset == 0 || iter.mLastOffset == iter.mNowOffset)
    {
        return false;
    }
    int32_t deltaSize = iter.mLastOffset - iter.mNowOffset;
    memmove(mRawBuffer + iter.mLastOffset, mRawBuffer + iter.mNowOffset, mRawLen - iter.mNowOffset);
    iter.mNowOffset = iter.mLastOffset;
    adjustLogSize(deltaSize);
    mRawLen += deltaSize;
    return true;
}

bool RawLog::UpdateKeyValue(RawLog::iterator &iter, const char *key, size_t key_len,
                       const char *value, size_t value_len)
{
    if (iter.mLastOffset == 0)
    {
        return false;
    }
    // sum total size
    int32_t kv_size = sizeof(char) * (key_len + value_len) + uint32_size((uint32_t)key_len) + uint32_size((uint32_t)value_len) + 2;
    int32_t lastSize = iter.mNowOffset - iter.mLastOffset;
    kv_size += 1 + uint32_size(kv_size);
    // ensure buffer size
    int32_t deltaSize = kv_size - lastSize;
    if (deltaSize > 0 && mRawLen + kv_size > mMaxLen)
    {
        // reset log_now_buffer
        adjustBuffer(kv_size);
    }

    // move
    if (deltaSize != 0)
    {
        memmove(mRawBuffer + iter.mLastOffset + kv_size, mRawBuffer + iter.mNowOffset, mRawLen - iter.mNowOffset);
        adjustLogSize(deltaSize);
    }

    mRawLen += deltaSize;
    uint8_t * buf = mRawBuffer + iter.mLastOffset;
    // key_value header
    *buf++ = 0x12;
    buf += uint32_pack(uint32_size(key_len) + uint32_size(value_len) + 2 + key_len + value_len, buf);
    // key len
    *buf++ = 0x0A;
    buf += uint32_pack(key_len, buf);
    // key
    memcpy(buf, key, key_len);
    buf += key_len;
    // value len
    *buf++ = 0x12;
    buf += uint32_pack(value_len, buf);
    // value
    memcpy(buf, value, value_len);
    buf += value_len;
    mNowBuffer = (uint8_t *)buf;
    iter.mNowOffset = iter.mLastOffset + kv_size;
    return true;
}

bool RawLog::AppendKeyValue(const char *key, size_t kenLen, const char *value,
                            size_t valueLen)
{
    uint32_t deltaSize = AddKeyValue(key, kenLen, value, valueLen);
    adjustLogSize(deltaSize);
    return false;
}

RawLog::iterator RawLog::GetIterator() const
{
    auto iter = iterator{};
    // skip delta and log header
    uint32_t startLen = mMallocDelta + 1;
    // skip log header size
    startLen += scan_varint(5, mRawBuffer + startLen);
    // skip time
    startLen += 6;
    iter.mNowOffset = startLen;
    assert(mRawBuffer[startLen] == 0x12);
    return iter;
}

void RawLog::adjustLogSize(int32_t deltaLen)
{
    if (deltaLen == 0)
    {
        return;
    }

    uint32_t logLenSize = scan_varint(5, mRawBuffer + mMallocDelta + 1);
    int32_t logSize = parse_uint32(logLenSize, mRawBuffer + mMallocDelta + 1);
    int32_t newSize = logSize + deltaLen;
    uint32_t newHeaderSize = uint32_size(newSize) + 1;

    mMallocDelta = (int8_t)(INIT_LOG_SIZE_BYTES - newHeaderSize);

    // set log header
    uint8_t * buf = (mRawBuffer + mMallocDelta);
    *buf++ = 0x0A;
    uint32_pack(newSize, buf);
}


}
