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

#include "models/PipelineEvent.h"

namespace logtail {

using LogContent = std::pair<StringView, StringView>;
using ContentsContainer = std::vector<std::pair<LogContent, bool>>;

template <class T, class F>
class BaseContentIterator {
    friend class LogEvent;

public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = F;
    using reference = F&;
    using pointer = F*;

    BaseContentIterator(const ContentsContainer& c) : container(c) {}

    reference operator*() const { return ptr->first; }
    pointer operator->() const { return &ptr->first; }
    BaseContentIterator& operator++() {
        Advance();
        return *this;
    }
    BaseContentIterator operator++(int) {
        BaseContentIterator it(ptr, container);
        Advance();
        return it;
    }
    bool operator==(const BaseContentIterator& rhs) const { return ptr == rhs.ptr; }
    bool operator!=(const BaseContentIterator& rhs) const { return ptr != rhs.ptr; }

private:
    explicit BaseContentIterator(const T& p, const ContentsContainer& c) : ptr(p), container(c) {}
    void Advance() {
        while ((++ptr != container.end()) && !(ptr->second))
            ;
    }

    T ptr;
    const ContentsContainer& container;
};

class LogEvent : public PipelineEvent {
    friend class PipelineEventGroup;

public:
    using ConstContentIterator = BaseContentIterator<ContentsContainer::const_iterator, const LogContent>;
    using ContentIterator = BaseContentIterator<ContentsContainer::iterator, LogContent>;

    StringView GetContent(StringView key) const;
    bool HasContent(StringView key) const;
    ContentIterator FindContent(StringView key);
    ConstContentIterator FindContent(StringView key) const;
    void SetContent(StringView key, StringView val);
    void SetContent(const std::string& key, const std::string& val);
    void SetContent(const StringBuffer& key, StringView val);
    void SetContentNoCopy(const StringBuffer& key, const StringBuffer& val);
    void SetContentNoCopy(StringView key, StringView val);
    void DelContent(StringView key);

    bool Empty() const { return mIndex.empty(); }
    size_t Size() const { return mIndex.size(); }

    ContentIterator begin();
    ContentIterator end();
    ConstContentIterator begin() const;
    ConstContentIterator end() const;
    ConstContentIterator cbegin() const;
    ConstContentIterator cend() const;

    uint64_t EventsSizeBytes() override;

#ifdef APSARA_UNIT_TEST_MAIN
    Json::Value ToJson() const override;
    bool FromJson(const Json::Value&) override;
#endif

private:
    LogEvent(PipelineEventGroup* ptr);

    // this is only used for ProcessorParseApsaraNative for backward compatability, since multiple keys are allowed.
    // We do not invalidate existing LogContent when the same key has arrived.
    friend class ProcessorParseApsaraNative;
    void AppendContentNoCopy(StringView key, StringView val);

    // since log reduce in SLS server requires the original order of log contents, we have to maintain this sequential
    // information for backward compatability.
    ContentsContainer mContents;
    std::map<StringView, size_t> mIndex;
};

} // namespace logtail
