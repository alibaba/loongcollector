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
#include <atomic>

namespace logtail {

class LogFileReader;

class GloablFileDescriptorManager {
public:
    static GloablFileDescriptorManager* GetInstance() {
        static GloablFileDescriptorManager singleton;
        return &singleton;
    }

    void OnFileOpen(LogFileReader*) { ++mOpenFileSize; }

    void OnFileClose(LogFileReader*) { --mOpenFileSize; }

    int32_t GetOpenedFilePtrSize() { return mOpenFileSize; }

private:
    std::atomic_int mOpenFileSize{0};
};

} // namespace logtail