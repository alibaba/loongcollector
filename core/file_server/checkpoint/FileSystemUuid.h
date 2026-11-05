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

#include <cstdint>
#include <functional>
#include <string>

namespace logtail {

// Filesystem UUID for a block device (ext4 / xfs). Empty when the filesystem type is
// unsupported or sysfs cannot be read. Result is cached per dev.
std::string GetFileSystemUuid(uint64_t dev);

// Test hook. An empty function clears the override. Also drops the dev cache.
void SetFileSystemUuidResolverForTest(const std::function<std::string(uint64_t)>& resolver);
void ClearFileSystemUuidCacheForTest();

} // namespace logtail
