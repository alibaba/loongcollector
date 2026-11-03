// Copyright 2026 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "common/FileSystemUtil.h"

namespace logtail {

// Uses the real directory backend except for a chosen operation and path.
// State is shared by value so resetting the hook cannot leave dangling captures.
class ScopedDirErrorInjector {
public:
    ScopedDirErrorInjector(const std::string& path,
                           const std::string& operation,
                           int error,
                           size_t successfulReads = 0) {
        auto reads = std::make_shared<size_t>(0);
        mPrevious = fsutil::Dir::SetErrorInjectorForTesting(
            [path = std::filesystem::path(path).lexically_normal(), operation, error, successfulReads, reads](
                const std::string& dir, const char* op) {
                if (std::filesystem::path(dir).lexically_normal() != path || operation != op) {
                    return 0;
                }
                if (operation == "read" && (*reads)++ < successfulReads) {
                    return 0;
                }
                return error;
            });
    }

    ~ScopedDirErrorInjector() { fsutil::Dir::SetErrorInjectorForTesting(std::move(mPrevious)); }
    ScopedDirErrorInjector(const ScopedDirErrorInjector&) = delete;
    ScopedDirErrorInjector& operator=(const ScopedDirErrorInjector&) = delete;

private:
    fsutil::Dir::ErrorInjector mPrevious;
};

} // namespace logtail
