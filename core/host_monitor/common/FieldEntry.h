/*
 * Copyright 2025 iLogtail Authors
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
