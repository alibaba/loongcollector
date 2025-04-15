// Copyright 2025 iLogtail Authors
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

#pragma once

#include "common/Span.h"
#include "common/StringView.h"

namespace logtail::ebpf {

class DataElement {
public:
    constexpr DataElement() = delete;
    constexpr DataElement(StringView name, StringView metricKey, StringView spanKey, StringView logKey, StringView desc)
        : mName(name), mMetricKey(metricKey), mSpanKey(spanKey), mLogKey(logKey), mDesc(desc) {}

    constexpr bool operator==(const DataElement& rhs) const { return mName == rhs.mName; }

    constexpr bool operator!=(const DataElement& rhs) const { return !(*this == rhs); }

    [[nodiscard]] constexpr StringView Name() const { return mName; }
    [[nodiscard]] constexpr StringView MetricKey() const { return mMetricKey; }
    [[nodiscard]] constexpr StringView SpanKey() const { return mSpanKey; }
    [[nodiscard]] constexpr StringView LogKey() const { return mLogKey; }
    [[nodiscard]] constexpr StringView Desc() const { return mDesc; }

protected:
    const StringView mName;
    const StringView mMetricKey;
    const StringView mSpanKey;
    const StringView mLogKey;
    const StringView mDesc;
};

class DataTableSchema {
public:
    template <std::size_t N>
    constexpr DataTableSchema(StringView name, StringView desc, const DataElement (&elements)[N])
        : mName(name), mDesc(desc), mElements(elements) {}

    [[nodiscard]] constexpr StringView Name() const { return mName; }
    [[nodiscard]] constexpr StringView Desc() const { return mDesc; }
    [[nodiscard]] constexpr Span<DataElement> Elements() const { return mElements; }
    [[nodiscard]] constexpr size_t Size() const { return mElements.size(); }

    [[nodiscard]] constexpr uint32_t ColIndex(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < mElements.size(); i++) {
            if (mElements[i].Name() == key) {
                break;
            }
        }
        return i;
    }

    [[nodiscard]] constexpr bool HasCol(StringView key) const {
        uint32_t i = 0;
        for (i = 0; i < mElements.size(); i++) {
            if (mElements[i].Name() == key) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] constexpr StringView ColName(size_t i) const { return mElements[i].Name(); }
    [[nodiscard]] constexpr StringView ColMetricKey(size_t i) const { return mElements[i].MetricKey(); }
    [[nodiscard]] constexpr StringView ColSpanKey(size_t i) const { return mElements[i].SpanKey(); }
    [[nodiscard]] constexpr StringView ColLogKey(size_t i) const { return mElements[i].LogKey(); }

private:
    const StringView mName;
    const StringView mDesc;
    const Span<DataElement> mElements;
};

} // namespace logtail::ebpf
