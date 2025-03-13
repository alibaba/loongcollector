// Copyright 2025 LoongCollector Authors
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

#include "ebpf/plugin/ProcessCache.h"

#include "ebpf/type/table/DataTable.h"
#include "logger/Logger.h"
#include "type/table/ProcessTable.h"

namespace logtail {

ProcessCacheValue::ProcessCacheValue() : mSourceBuffer(std::make_shared<SourceBuffer>()) {
}

ProcessCacheValue* ProcessCacheValue::CloneContents() {
    auto* newValue = new ProcessCacheValue();
    newValue->mContents = mContents;
    newValue->mSourceBuffer = mSourceBuffer;
    return newValue;
}

const StringView& ProcessCacheValue::operator[](const ebpf::DataElement& key) const {
    return mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)];
}

void ProcessCacheValue::SetContentNoCopy(const ebpf::DataElement& key, const StringView& val) {
    auto v = mSourceBuffer->CopyString(val);
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(v.data, v.size);
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, const StringView& val) {
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = val;
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, const std::string& val) {
    auto v = mSourceBuffer->CopyString(val);
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(v.data, v.size);
}

void ProcessCacheValue::SetContent(const ebpf::DataElement& key, const char* data, size_t len) {
    auto v = mSourceBuffer->CopyString(data, len);
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(v.data, v.size);
}

void ProcessCacheValue::SetContent(const ebpf::DataElement& key, int32_t val) {
    auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt32Width);
    char* out = buf.data;
    fmt::format_to_n(out, buf.capacity, "{}", val);
    *out = '\0';
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(buf.data, buf.size);
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, uint32_t val) {
    auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt32Width);
    char* out = buf.data;
    fmt::format_to_n(out, buf.capacity, "{}", val);
    *out = '\0';
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(buf.data, buf.size);
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, int64_t val) {
    auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt64Width);
    char* out = buf.data;
    fmt::format_to_n(out, buf.capacity, "{}", val);
    *out = '\0';
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(buf.data, buf.size);
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, uint64_t val) {
    auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt64Width);
    char* out = buf.data;
    fmt::format_to_n(out, buf.capacity, "{}", val);
    *out = '\0';
    mContents[ebpf::FindIndex(ebpf::kProcessCacheElements, key)] = StringView(buf.data, buf.size);
}

void ProcessCacheValue::SetContent(const ebpf::DataElement& key, long long val) {
    SetContent(key, int64_t(val));
}
void ProcessCacheValue::SetContent(const ebpf::DataElement& key, unsigned long long val) {
    SetContent(key, uint64_t(val));
}

ProcessCache::ProcessCache(size_t initCacheSize) {
    mCache.reserve(initCacheSize);
}

bool ProcessCache::Contains(const data_event_id& key) const {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    return mCache.find(key) != mCache.end();
}

std::shared_ptr<ProcessCacheValue> ProcessCache::Lookup(const data_event_id& key) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    auto it = mCache.find(key);
    if (it != mCache.end()) {
        return it->second;
    }
    return nullptr;
}

size_t ProcessCache::Size() const {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    return mCache.size();
}

void ProcessCache::removeCache(const data_event_id& key) {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.erase(key);
}

void ProcessCache::AddCache(const data_event_id& key, std::shared_ptr<ProcessCacheValue>&& value) {
    ++value->mRefCount;
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.emplace(key, std::move(value));
}

void ProcessCache::IncRef(const data_event_id& key) {
    auto v = Lookup(key);
    if (v) {
        ++(v->mRefCount);
    }
}

void ProcessCache::DecRef(const data_event_id& key, time_t curktime) {
    auto v = Lookup(key);
    if (v) {
        --(v->mRefCount);
    }
    if (v && v->mRefCount == 0) {
        enqueueExpiredEntry(key, curktime);
    }
}

void ProcessCache::enqueueExpiredEntry(const data_event_id& key, time_t curktime) {
    mCacheExpireQueue.emplace_back(ExitedEntry{curktime, key});
}

void ProcessCache::ClearCache() {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    mCache.clear();
}


void ProcessCache::ClearExpiredCache(time_t ktime) {
    ktime -= kMaxCacheExpiredTimeout;
    if (mCacheExpireQueue.empty() || mCacheExpireQueue.front().time > ktime) {
        return;
    }
    while (!mCacheExpireQueue.empty() && mCacheExpireQueue.front().time <= ktime) {
        auto& key = mCacheExpireQueue.front().key;
        LOG_DEBUG(sLogger, ("[RecordExecveEvent][DUMP] clear expired cache pid", key.pid)("ktime", key.time));
        removeCache(key);
        mCacheExpireQueue.pop_front();
    }
}

} // namespace logtail
