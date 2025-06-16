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


template <typename TClass, typename TField = double>
class FieldName {
public:
    std::string_view name;
    TField TClass::*ptr; // 成员指针，直接访问字段

    constexpr FieldName(std::string_view n, TField TClass::*p) : name(n), ptr(p) {}

    // 访问对象中的字段值
    TField& value(TClass& obj) const { return obj.*ptr; }
    const TField& value(const TClass& obj) const { return obj.*ptr; }
};

// 字段定义宏
#define FIELD_ENTRY(CLASS, FIELD) FieldName<CLASS>(#FIELD, &CLASS::FIELD)
