#pragma once

#include <cstddef>
#include <models/StringView.h>

#include <array>

#include "common/memory/SourceBuffer.h"
#include "ebpf/type/table/AppTable.h"
#include "ebpf/type/table/DataTable.h"
#include "ebpf/type/table/NetTable.h"
#include "ebpf/type/table/ProcessTable.h"
#include "logger/Logger.h"

namespace logtail {
namespace ebpf {

constexpr StringView kEmptyStrRet = "INDEX_OUT_OF_RANGE";
constexpr size_t kMaxInt32Width = 11;
constexpr size_t kMaxInt64Width = 20;

// static data row
template <const DataTableSchema* schema>
class StaticDataRow {
public:
    StaticDataRow() : mSourceBuffer(std::make_shared<SourceBuffer>()) {}
    StaticDataRow(const std::shared_ptr<SourceBuffer>& sourceBuffer) : mSourceBuffer(sourceBuffer) {}

    constexpr size_t Size() const { return schema->Size(); }

    template <const DataElement& TElement>
    inline void SetNoCopy(const StringView& val) {
        constexpr uint32_t idx = schema->ColIndex(TElement.Name());
        static_assert(idx < schema->Size());
        mRow[idx] = val;
    }

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

    template <const ebpf::DataElement& key>
    void Set(const char* data, size_t len) {
        Set<key>(StringView(data, len));
    }

    template <const ebpf::DataElement& key>
    void Set(int32_t val) {
        auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt32Width);
        auto end = fmt::format_to_n(buf.data, buf.capacity, "{}", val);
        *end.out = '\0';
        buf.size = end.size;
        Set<key>(StringView(buf.data, buf.size));
    }

    template <const ebpf::DataElement& key>
    void Set(uint32_t val) {
        auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt32Width);
        auto end = fmt::format_to_n(buf.data, buf.capacity, "{}", val);
        *end.out = '\0';
        buf.size = end.size;
        Set<key>(StringView(buf.data, buf.size));
    }

    template <const ebpf::DataElement& key>
    void Set(int64_t val) {
        auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt64Width);
        auto end = fmt::format_to_n(buf.data, buf.capacity, "{}", val);
        *end.out = '\0';
        buf.size = end.size;
        Set<key>(StringView(buf.data, buf.size));
    }

    template <const ebpf::DataElement& key>
    void Set(uint64_t val) {
        auto buf = mSourceBuffer->AllocateStringBuffer(kMaxInt64Width);
        auto end = fmt::format_to_n(buf.data, buf.capacity, "{}", val);
        *end.out = '\0';
        buf.size = end.size;
        Set<key>(StringView(buf.data, buf.size));
    }

    template <const ebpf::DataElement& key>
    void Set(long long val) {
        Set<key>(int64_t(val));
    }

    template <const ebpf::DataElement& key>
    void Set(unsigned long long val) {
        Set<key>(uint64_t(val));
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

    StringView& operator[](size_t idx) { return mRow[idx]; }

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

    std::shared_ptr<SourceBuffer> GetSourceBuffer() { return mSourceBuffer; }

private:
    std::shared_ptr<SourceBuffer> mSourceBuffer;
    std::array<StringView, schema->Size()> mRow;
};

extern template class StaticDataRow<&kConnTrackerTable>;
extern template class StaticDataRow<&kAppMetricsTable>;
extern template class StaticDataRow<&kNetMetricsTable>;

extern template class StaticDataRow<&kProcessCacheTable>;

} // namespace ebpf
} // namespace logtail
