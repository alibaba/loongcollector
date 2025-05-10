#ifndef LOONGCOLLECTOR_FIELD_ENTRY_H
#define LOONGCOLLECTOR_FIELD_ENTRY_H
#include <functional>
#include <string>

#define FIELD_NAME_INITIALIZER(NAME, CLASS, F) NAME, [](CLASS& p) { return std::ref(p.F); }
#define FIELD_NAME_ENTRY(NAME, CLASS, F) {FIELD_NAME_INITIALIZER(NAME, CLASS, F)}
// 适用于字段与Name同名的情况
#define FIELD_ENTRY(CLASS, F) {#F, [](CLASS& p) { return std::ref(p.F); }}

template <typename TClass, typename TFieldType = double>
class FieldName {
    std::function<TFieldType&(TClass&)> _fnGet;

public:
    const std::string name; // 名称

    FieldName(const char* n, std::function<TFieldType&(TClass&)> fnGet) : _fnGet(fnGet), name(n == nullptr ? "" : n) {}

    const TFieldType& value(const TClass& p) const { return _fnGet(const_cast<TClass&>(p)); }

    TFieldType& value(TClass& p) const { return const_cast<TFieldType&>(_fnGet(p)); }
};

#define METRIC_FIELD_NAME_INITIALIZER(NAME, CLASS, F, BOOL) FIELD_NAME_INITIALIZER(NAME, CLASS, F), BOOL
#define METRIC_FIELD_NAME_ENTRY(NAME, CLASS, F, BOOL) {METRIC_FIELD_NAME_INITIALIZER(NAME, CLASS, F, BOOL)}
#define METRIC_FIELD_ENTRY(CLASS, F, BOOL) {METRIC_FIELD_NAME_INITIALIZER(#F, CLASS, F, BOOL)}

template <typename TClass, typename TFieldType>
struct MetricFieldName : FieldName<TClass, TFieldType> {
    const bool isMetric;

public:
    MetricFieldName(const char* name, std::function<TFieldType&(TClass&)> fnGet, bool isMetric)
        : FieldName<TClass, TFieldType>(name, fnGet), isMetric(isMetric) {}
};

#endif // LOONGCOLLECTOR_FIELD_ENTRY_H
