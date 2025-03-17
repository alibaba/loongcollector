#pragma once

#include <models/StringView.h>

#include <array>

#include "common/memory/SourceBuffer.h"
#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/NetTable.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

constexpr StringView kEmptyStrRet = "INDEX_OUT_OF_RANGE";

// static data row
template <const DataTableSchema* schema>
class StaticDataRow {
public:
    StaticDataRow() : mSourceBuffer(std::make_shared<SourceBuffer>()) {}

    StaticDataRow(const StaticDataRow&) = delete;
    StaticDataRow& operator=(const StaticDataRow&) = delete;

    template <const DataElement& TElement>
    inline void Set(const std::string& val) {
        constexpr uint32_t idx = schema->ColIndex(TElement.Name());
        static_assert(idx < schema->Size());
        auto v = mSourceBuffer->CopyString(val);
        mRow[idx] = StringView(v.data, v.size);
    }

    template <const DataElement& TElement>
    inline void Set(const StringView& val) {
        constexpr uint32_t idx = schema->ColIndex(TElement.Name());
        static_assert(idx < schema->Size());
        auto v = mSourceBuffer->CopyString(val);
        mRow[idx] = StringView(v.data, v.size);
    }

    template <const DataElement& TElement>
    inline void SetNoCopy(const StringView& val) {
        constexpr uint32_t idx = schema->ColIndex(TElement.Name());
        static_assert(idx < schema->Size());
        mRow[idx] = val;
    }

    template <const size_t TIndex>
    inline const StringView& Get() const {
        static_assert(TIndex < schema->Size());
        return mRow[TIndex];
    }

    template <const DataElement& TElement>
    inline const StringView& Get() const {
        constexpr uint32_t idx = schema->ColIndex(TElement.Name());
        static_assert(idx < schema->Size());
        return mRow[idx];
    }

    const StringView& operator[](size_t idx) const {
        if (idx >= schema->Size()) {
            LOG_WARNING(sLogger, ("invalid idx", idx)("schema", schema->Name()));
            return kEmptyStrRet;
        }
        return mRow[idx];
    }

    template <const size_t TIndex>
    constexpr const StringView& GetColName() const {
        static_assert(TIndex < schema->Size());
        return schema->ColName(TIndex);
    }

    template <const size_t TIndex>
    constexpr const StringView& GetLogKey() const {
        static_assert(TIndex < schema->Size());
        return schema->ColLogKey(TIndex);
    }

    template <const size_t TIndex>
    constexpr const StringView& GetMetricKey() const {
        static_assert(TIndex < schema->Size());
        return schema->ColMetricKey(TIndex);
    }

    template <const size_t TIndex>
    constexpr const StringView& GetSpanKey() const {
        static_assert(TIndex < schema->Size());
        return schema->ColSpanKey(TIndex);
    }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::array<StringView, schema->Size()> mRow;
};

extern template class StaticDataRow<&kConnTrackerTable>;
extern template class StaticDataRow<&kAppMetricsTable>;
extern template class StaticDataRow<&kNetMetricsTable>;

} // namespace ebpf
} // namespace logtail
