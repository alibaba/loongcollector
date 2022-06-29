/*
 * Copyright 2022 iLogtail Authors
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
#include <gflags/gflags.h>

/** Macro to define INT32 flag. Must be used in global scope.                */
#define DEFINE_FLAG_INT32(name, desc, value) DEFINE_int32(name, value, desc)

/** Macro to define INT64 flag. Must be used in global scope */
#define DEFINE_FLAG_INT64(name, desc, value) DEFINE_int64(name, value, desc)

/** Macro to define BOOL flag. Must be used in global scope.               */
#define DEFINE_FLAG_BOOL(name, desc, value) DEFINE_bool(name, value, desc)

/** Macro to define DOUBLE flag. Must be used in global scope.            */
#define DEFINE_FLAG_DOUBLE(name, desc, value) DEFINE_double(name, value, desc)

/** Macro to define STRING flag. Must be used in global scope.            */
#define DEFINE_FLAG_STRING(name, desc, value) DEFINE_string(name, value, desc)

/** Macro to declare INT32 flag                                           */
#define DECLARE_FLAG_INT32(name) DECLARE_int32(name)

/** Macro to declare INT64 flag                                           */
#define DECLARE_FLAG_INT64(name) DECLARE_int64(name)

/** Macro to declare BOOL flag                                            */
#define DECLARE_FLAG_BOOL(name) DECLARE_bool(name)

/** Macro to decclare STRING flag                                         */
#define DECLARE_FLAG_STRING(name) DECLARE_string(name)

/** Macro to declare DOUBLE flag                                          */
#define DECLARE_FLAG_DOUBLE(name) DECLARE_double(name)

/** Macro to access flags */
#define INT32_FLAG(name) (FLAGS_##name)
#define INT64_FLAG(name) (FLAGS_##name)
#define BOOL_FLAG(name) (FLAGS_##name)
#define DOUBLE_FLAG(name) (FLAGS_##name)
#define STRING_FLAG(name) (FLAGS_##name)
