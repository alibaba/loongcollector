// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "common/ArchiveHelper.h"

#include <archive.h>
#include <archive_entry.h>

#include <filesystem>

#include "logger/Logger.h"

namespace logtail {

namespace fs = std::filesystem;

void ArchiveHelper::ArchiveDeleter::operator()(archive* a) const {
    if (a) {
        archive_read_free(a);
    }
}

ArchiveHelper::ArchiveHelper(const std::string& archivePath, const std::string& outputDir)
    : mArchivePath(archivePath), mOutputDir(outputDir) {
}

bool ArchiveHelper::Extract() {
    if (!fs::exists(mArchivePath)) {
        LOG_ERROR(sLogger, ("archive file does not exists", mArchivePath));
        return false;
    }

    if (!fs::exists(mOutputDir)) {
        try {
            fs::create_directories(mOutputDir);
        } catch (const fs::filesystem_error& e) {
            LOG_ERROR(sLogger, ("Failed to create output directory, e", std::string(e.what())));
            return false;
        }
    }

    auto archive = createArchiveReader();
    if (archive == nullptr) {
        LOG_ERROR(sLogger, ("failed to create archive reader", ""));
        return false;
    }

    struct archive_entry* entry = nullptr;
    while (archive_read_next_header(archive.get(), &entry) == ARCHIVE_OK) {
        const char* name = archive_entry_pathname(entry);
        std::string fullPath = mOutputDir + "/" + name;

        if (archive_entry_filetype(entry) == AE_IFDIR) {
            try {
                fs::create_directories(fullPath);
            } catch (const fs::filesystem_error& e) {
                LOG_ERROR(sLogger, ("Failed to create directory, e", std::string(e.what())));
                return false;
            }
            continue;
        }

        FILE* out = fopen(fullPath.c_str(), "wb");
        if (!out) {
            LOG_ERROR(sLogger, ("Failed to create file, e", fullPath));
            return false;
        }

        const void* buffer = nullptr;
        size_t size = 0;
        int64_t offset = 0;
        while (archive_read_data_block(archive.get(), &buffer, &size, &offset) == ARCHIVE_OK) {
            fwrite(buffer, 1, size, out);
        }

        fclose(out);
    }
    return true;
}

ArchiveHelper::ArchivePtr ArchiveHelper::createArchiveReader() {
    struct archive* a = archive_read_new();
    if (!a) {
        LOG_ERROR(sLogger, ("failed to create archive reader", ""));
        return nullptr;
    }

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    int ret = archive_read_open_filename(a, mArchivePath.c_str(), 10240);
    if (ret != ARCHIVE_OK) {
        archive_read_free(a);
        LOG_ERROR(sLogger, ("failed to create archive reader", mArchivePath));
        return nullptr;
    }

    return ArchivePtr(a);
}

} // namespace logtail
