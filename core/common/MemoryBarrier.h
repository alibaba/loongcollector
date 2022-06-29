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
#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace logtail {

inline void ReadBarrier() {
#ifdef _MSC_VER
    _ReadBarrier();
#elif defined(__aarch64__)
    asm volatile("dsb ish" ::: "memory");
#elif defined(__sw_64__)
    asm volatile("memb" ::: "memory");
#else
    asm volatile("lfence" ::: "memory");
#endif
}

inline void WriteBarrier() {
#ifdef _MSC_VER
    _WriteBarrier();
#elif defined(__aarch64__)
    asm volatile("dsb ish" ::: "memory");
#elif defined(__sw_64__)
    asm volatile("memb" ::: "memory");
#else
    asm volatile("sfence" ::: "memory");
#endif
}

inline void ReadWriteBarrier() {
#ifdef _MSC_VER
    _ReadWriteBarrier();
#elif defined(__aarch64__)
    asm volatile("dsb ish" ::: "memory");
#elif defined(__sw_64__)
    asm volatile("memb" ::: "memory");
#else
    asm volatile("mfence" ::: "memory");
#endif
}

} // namespace logtail
