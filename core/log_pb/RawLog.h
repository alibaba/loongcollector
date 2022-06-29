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

#ifndef LOGTAIL_RAWLOG_H
#define LOGTAIL_RAWLOG_H

#include <string>
#include <vector>
#include <boost/regex.hpp>

namespace logtail
{

class RawLog
{
public:

    struct iterator
    {
        int32_t mLastOffset = 0;
        int32_t mNowOffset = 0;
    };

    RawLog() = default;
    ~RawLog()
    {
        if (mRawBuffer != NULL)
        {
            free(mRawBuffer);
        }
    }

    static RawLog * AddLogFull(uint32_t logTime, std::vector<std::string> keys, boost::match_results<const char*> subMathValues);

    void AddLogStart(uint32_t logTime);


    void AddKeyValue(const std::string & key, const std::string & value)
    {
        AddKeyValue(key.c_str(), key.size(), value.c_str(), value.size());
    }

    void AddKeyValue(const std::string & key, const char * value, size_t valueLen)
    {
        AddKeyValue(key.c_str(), key.size(), value, valueLen);
    }

    uint32_t AddKeyValue(const char * key, size_t kenLen, const char * value, size_t valueLen);

    void AddLogDone();

    uint8_t * GetBuffer()
    {
        return mRawBuffer + mMallocDelta;
    }

    size_t GetBufferLength()
    {
        return mRawLen - mMallocDelta;
    }

    void AppendToString(std::string* output) const;

    /**
     * init iterator
     * @return
     */
    iterator GetIterator() const;

    /**
     * get next key value pair, return false if end of log
     * @param iter
     * @param key
     * @param keyLen
     * @param value
     * @param valueLen
     * @return
     */
    bool NextKeyValue(iterator & iter, const char * & key, uint32_t & keyLen, const char * & value, uint32_t & valueLen);

    /**
     * delete this key value pair, return false if iter is invalid
     * @param iter
     * @return
     */
    bool DeleteKeyValue(iterator & iter);

    /**
     * update this key value pair
     * @param iter
     * @param key
     * @param kenLen
     * @param value
     * @param valueLen
     * @return
     */
    bool UpdateKeyValue(iterator & iter, const char * key, size_t kenLen, const char * value, size_t valueLen);

    /**
     * append key value pair after end of log
     * @param key
     * @param kenLen
     * @param value
     * @param valueLen
     * @return
     */
    bool AppendKeyValue(const char * key, size_t kenLen, const char * value, size_t valueLen);


protected:

    void adjustBuffer(uint32_t newLen);
    void AddTime(uint32_t logTime);
    void adjustLogSize(int32_t deltaLen);

    uint32_t mRawLen = 0;
    uint32_t mMaxLen = 0;
    uint8_t * mRawBuffer = NULL;
    uint8_t * mNowBuffer = NULL;
    int8_t mMallocDelta = 0;

};


}



#endif //LOGTAIL_RAWLOG_H
