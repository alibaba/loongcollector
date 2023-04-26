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

#include <stdlib.h>

namespace logtail {

void SetEnv(const char* key, const char* value) {
#if defined(__linux__)
    setenv(key, value, 1);
#elif defined(_MSC_VER)
    _putenv_s(key, value);
#endif
}

void UnsetEnv(const char* key) {
#if defined(__linux__)
    unsetenv(key);
#elif defined(_MSC_VER)
    _putenv_s(key, "");
#endif
}

} // namespace logtail
