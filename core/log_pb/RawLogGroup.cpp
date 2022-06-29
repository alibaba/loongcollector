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

#include "RawLogGroup.h"

namespace logtail
{

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
static inline size_t uint32_pack(uint32_t value, std::string *output)
{
    unsigned rv = 1;

    if (value >= 0x80) {
        output->push_back(value | 0x80);
        ++rv;
        value >>= 7;
        if (value >= 0x80) {
            output->push_back(value | 0x80);
            ++rv;
            value >>= 7;
            if (value >= 0x80) {
                output->push_back(value | 0x80);
                ++rv;
                value >>= 7;
                if (value >= 0x80) {
                    output->push_back(value | 0x80);
                    ++rv;
                    value >>= 7;
                }
            }
        }
    }
    /* assert: value<128 */
    output->push_back(value);
    return rv;
}



int RawLogGroup::logtags_size() const
{
    return (int)logtags_.size();
}

void RawLogGroup::clear_logtags()
{
    logtags_.clear();
}

const LogTag &RawLogGroup::logtags(int index) const
{
    return logtags_[index];
}

LogTag *RawLogGroup::mutable_logtags(int index)
{
    return &logtags_[index];
}

void RawLogGroup::add_logtags(const std::string &key, const std::string &value)
{
    logtags_.push_back(::std::move(LogTag(key, value)));
}

void RawLogGroup::add_logtags(const std::string &key, std::string &&value)
{
    logtags_.push_back(LogTag(key, ::std::move(value)));
}

std::vector<LogTag> *RawLogGroup::mutable_logtags()
{
    return &logtags_;
}

const std::vector<LogTag> &RawLogGroup::logtags() const
{
    return logtags_;
}

RawLogGroup::~RawLogGroup()
{
    for (size_t size = 0; size < rawlogs_.size(); ++size)
    {
        delete rawlogs_[size];
    }
}

int RawLogGroup::logs_size() const
{
    return (int)rawlogs_.size();
}

void RawLogGroup::clear_logs()
{
    for (size_t size = 0; size < rawlogs_.size(); ++size)
    {
        delete rawlogs_[size];
    }
    rawlogs_.clear();
}

const RawLog &RawLogGroup::logs(int index) const
{
    return *rawlogs_[index];
}

RawLog *RawLogGroup::mutable_logs(int index)
{
    return rawlogs_[index];
}

void RawLogGroup::add_logs(RawLog *log)
{
    rawlogs_.push_back(log);
}

std::vector<RawLog *> *RawLogGroup::mutable_logs()
{
    return &rawlogs_;
}

const std::vector<RawLog *> & RawLogGroup::logs()
{
    return rawlogs_;
}

bool RawLogGroup::SerializeToString(std::string *output) const
{
    output->clear();
    return AppendToString(output);
}

bool RawLogGroup::AppendToString(std::string *output) const
{
    if (rawlogs_.empty())
    {
        return false;
    }
    pack_logs(output);
    pack_others(output);
    pack_logtags(output);
    return true;
}

void RawLogGroup::pack_logs(std::string *output) const
{
    for (size_t size = 0; size < rawlogs_.size(); ++size)
    {
        RawLog * log = rawlogs_[size];
        log->AppendToString(output);
    }
}

void RawLogGroup::pack_logtags(std::string *output) const
{
    for (size_t size = 0; size < logtags_.size(); ++size)
    {
        const LogTag & logTag = logtags_[size];

        // use only 1 malloc
        size_t k_len = logTag.first.size();
        size_t v_len = logTag.second.size();
        const char * k = logTag.first.c_str();
        const char * v = logTag.second.c_str();
        uint32_t tag_size = sizeof(char) * (k_len + v_len) + uint32_size((uint32_t)k_len) + uint32_size((uint32_t)v_len) + 2;
        output->push_back(0x32);
        uint32_pack(tag_size, output);
        output->push_back(0x0A);
        uint32_pack((uint32_t)k_len, output);
        output->append(k, k_len);
        output->push_back(0x12);
        uint32_pack((uint32_t)v_len, output);
        output->append(v, v_len);
    }
}

void RawLogGroup::pack_others(std::string *output) const
{
    if (has_category())
    {
        output->push_back(0x12);
        uint32_pack((uint32_t)category_.size(), output);
        output->append(category_);
    }

    if (has_topic())
    {
        output->push_back(0x1A);
        uint32_pack((uint32_t)topic_.size(), output);
        output->append(topic_);
    }

    if (has_source())
    {
        output->push_back(0x22);
        uint32_pack((uint32_t)source_.size(), output);
        output->append(source_);
    }

    if (has_machineuuid())
    {
        output->push_back(0x2A);
        uint32_pack((uint32_t)machineuuid_.size(), output);
        output->append(machineuuid_);
    }
}


}