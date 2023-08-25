/*
 * Copyright 2023 iLogtail Authors
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

#include <memory>
#include <list>
#include "models/StringView.h"

namespace logtail {

class SourceBuffer;

struct StringBuffer {
    bool IsValid() { return data != nullptr; }
    char* const data;
    size_t size;
    const size_t capacity; // max bytes of data can be stored, data[capacity] is always '\0'.
private:
    StringBuffer(char* data, size_t capacity) : data(data), size(0), capacity(capacity) { data[0] = '\0'; }
    friend class SourceBuffer;
};

class BufferAllocator {
public:
    BufferAllocator() : mCapacity(0), mUsed(0), mOtherUsed(0) {}

    bool Init(size_t capacity) {
        if (capacity == 0 || mCapacity > 0) {
            return false;
        }
        mData.reset(new char[capacity]);
        mCapacity = capacity;
        return true;
    }

    bool IsInited() { return mCapacity > 0; }

    void* Allocate(size_t size) {
        if (mCapacity == 0) {
            return nullptr;
        }
        if (mUsed + size > mCapacity) {
            mOtherData.emplace_back(new char[size]);
            mOtherUsed += size;
            return mOtherData.back().get();
        }
        void* result = mData.get() + mUsed;
        mUsed += size;
        return result;
    }

    size_t TotalAllocated() { return mUsed + mOtherUsed; }

    size_t TotalMemory() { return mCapacity + mOtherUsed; }

private:
    std::unique_ptr<char[]> mData;
    size_t mCapacity;
    size_t mUsed;
    // Stores other memory allocation than buffer
    std::list<std::unique_ptr<char[]>> mOtherData;
    size_t mOtherUsed;
};

class SourceBuffer {
public:
    SourceBuffer() {}
    virtual ~SourceBuffer() {}
    StringBuffer AllocateStringBuffer(size_t size) {
        if (!mAllocator.IsInited()) {
            if (!mAllocator.Init(size * 1.2)) {
                return StringBuffer(nullptr, 0);
            }; // TODO: better allocate strategy
        }
        char* data = static_cast<char*>(mAllocator.Allocate(size + 1));
        data[size] = '\0';
        return StringBuffer(data, size);
    }

    StringBuffer CopyString(const char* data, size_t len) {
        StringBuffer sb = AllocateStringBuffer(len);
        memcpy(sb.data, data, len);
        sb.size = len;
        return sb;
    }
    StringBuffer CopyString(const std::string& s) { return CopyString(s.data(), s.length()); }
    StringBuffer CopyString(const StringView& s) { return CopyString(s.data(), s.length()); }

    // StringView GetProperty(const char* key){};

private:
    BufferAllocator mAllocator;
#ifdef APSARA_UNIT_TEST_MAIN
    friend class LogEventUnittest;
    friend class PipelineEventGroupUnittest;
#endif
};

} // namespace logtail