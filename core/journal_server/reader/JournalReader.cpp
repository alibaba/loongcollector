/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * ...
 */

#include "JournalReader.h"

#include <cstring>

#include <chrono>
#include <filesystem>
#include <system_error>

#ifdef __linux__
#include <errno.h>
#include <sys/epoll.h>
#include <systemd/sd-journal.h>
#include <unistd.h>
#endif

#include "logger/Logger.h"

namespace logtail {

/*========================================================
 *  Impl: systemd-journal implementation for Linux
 *  Note: This plugin is only available on Linux platform
 *========================================================*/
class JournalReader::Impl {
public:
    Impl() = default;

    ~Impl() { Close(); }

    // Delete copy and move operations
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    /*---------------  Open / Close  ----------------*/
    bool Open() {
        if (mIsOpen) {
            return true;
        }

        int ret = 0;

        if (mJournalPaths.empty()) {
            ret = sd_journal_open(&mJournal, SD_JOURNAL_LOCAL_ONLY);
        } else {
            // Add boundary check
            if (mJournalPaths[0].empty()) {
                return false;
            }
            const std::string& path = mJournalPaths[0];

            // Verify path existence
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) {
                return false;
            }

            ret = std::filesystem::is_directory(path) ? openDir(path) : openFile(path);
        }

        if (ret < 0) {
            // Clean up resources on failure
            if (mJournal != nullptr) {
                sd_journal_close(mJournal);
                mJournal = nullptr;
            }
            return false;
        }

        // Set data threshold
        sd_journal_set_data_threshold(mJournal, mDataThreshold);

        mIsOpen = true;
        return true;
    }

    void Close() {
        if (mJournal != nullptr) {
            sd_journal_close(mJournal);
            mJournal = nullptr;
        }
        mIsOpen = false;
    }

    [[nodiscard]] bool IsOpen() const { return mIsOpen && mJournal != nullptr; }

    /*---------------  Traversal  ----------------*/
    bool SeekHead() {
        return call([](auto journal) { return sd_journal_seek_head(journal); });
    }
    bool SeekTail() {
        return call([](auto journal) { return sd_journal_seek_tail(journal); });
    }
    bool SeekCursor(const std::string& cursor) {
        // Add parameter validation
        if (cursor.empty()) {
            return false;
        }
        return call([&](auto journal) { return sd_journal_seek_cursor(journal, cursor.c_str()); });
    }

    bool Next() {
        if (!IsOpen()) {
            return false;
        }

        int ret = sd_journal_next(mJournal);

        if (ret < 0) {
            // Distinguish between errors and normal end conditions
            return false;
        }
        return ret > 0;
    }

    JournalReadStatus NextWithStatus() {
        if (!IsOpen()) {
            return JournalReadStatus::kError;
        }

        int ret = sd_journal_next(mJournal);

        if (ret > 0) {
            // Successfully moved to next entry, data available
            return JournalReadStatus::kOk;
        }
        if (ret == 0) {
            // Reached end, no more data
            return JournalReadStatus::kEndOfJournal;
        }
        // Error condition (ret < 0)
        // Common error codes:
        // -ESTALE (116): Journal file has been deleted/rotated, cursor invalid
        // -EINVAL: Invalid parameter
        // -EBADMSG: Log file corrupted
        return JournalReadStatus::kError;
    }

    bool Previous() {
        if (!IsOpen()) {
            return false;
        }

        int ret = sd_journal_previous(mJournal);

        if (ret < 0) {
            // Distinguish between errors and normal end conditions
            return false;
        }
        return ret > 0;
    }

    /*---------------  Read Single Entry  ----------------*/
    bool GetEntry(JournalEntry& entry) {
        if (!IsOpen()) {
            return false;
        }
        entry = {};

        char* cursorPtr = nullptr;
        int cursorRet = sd_journal_get_cursor(mJournal, &cursorPtr);
        if (cursorRet < 0 || !cursorPtr) {
            return false;
        }
        std::unique_ptr<char, decltype(&free)> cursor(cursorPtr, &free);
        entry.cursor = cursor.get();

        // Read realtime timestamp
        // Note: If this returns an error, it usually means the journal file has been deleted (rotated)
        // Need to trigger error recovery flow
        uint64_t timestamp = 0;
        int timeRet = sd_journal_get_realtime_usec(mJournal, &timestamp);
        if (timeRet < 0) {
            // Timestamp read failed, this usually indicates the journal file has been rotated/deleted
            // Return false to trigger upper-level error recovery logic
            return false;
        }
        entry.realtimeTimestamp = timestamp;

        // Read monotonic timestamp (this can fail, not fatal)
        uint64_t monotonicTimestamp = 0;
        sd_id128_t bootId;
        int monoRet = sd_journal_get_monotonic_usec(mJournal, &monotonicTimestamp, &bootId);
        if (monoRet >= 0) {
            entry.monotonicTimestamp = monotonicTimestamp;
        }

        const void* data = nullptr;
        size_t len = 0;
        sd_journal_restart_data(mJournal);

        int fieldCount = 0;
        constexpr int kMaxFieldsPerEntry = 1000; // Prevent memory explosion

        while (sd_journal_enumerate_data(mJournal, &data, &len) > 0) {
            if (fieldCount >= kMaxFieldsPerEntry) {
                break;
            }

            if (!data || len == 0) {
                continue;
            }

            const char* dataPtr = static_cast<const char*>(data);
            const char* equalSign = static_cast<const char*>(memchr(dataPtr, '=', len));
            if (equalSign == nullptr) {
                continue;
            }

            // Boundary check
            size_t keyLen = equalSign - dataPtr;
            size_t valueLen = len - keyLen - 1;

            if (keyLen == 0 || valueLen == 0) {
                continue;
            }

            // Limit field length
            constexpr size_t kMaxFieldLength = 65536;
            if (keyLen > kMaxFieldLength || valueLen > kMaxFieldLength) {
                continue;
            }

            try {
                entry.fields.emplace(std::string(dataPtr, keyLen), std::string(equalSign + 1, valueLen));
            } catch (const std::bad_alloc&) {
                break; // Stop adding fields when out of memory
            }
            fieldCount++;
        }
        return true;
    }

    std::string GetCursor() {
        if (!IsOpen()) {
            return "";
        }
        char* cursorPtr = nullptr;
        if (sd_journal_get_cursor(mJournal, &cursorPtr) < 0 || !cursorPtr) {
            return "";
        }
        std::unique_ptr<char, decltype(&free)> cursor(cursorPtr, &free);
        std::string res(cursor.get());
        return res;
    }

    /*---------------  Filter / Wait  ----------------*/
    bool AddMatch(const std::string& field, const std::string& value) {
        if (!IsOpen() || field.empty() || value.empty()) {
            return false;
        }

        // Limit match string length
        if (field.length() > 1024 || value.length() > 1024) {
            return false;
        }

        std::string keyValue = field + "=" + value;
        return sd_journal_add_match(mJournal, keyValue.c_str(), 0) == 0;
    }

    bool AddDisjunction() { return IsOpen() && sd_journal_add_disjunction(mJournal) == 0; }

    std::vector<std::string> GetUniqueValues(const std::string& field) {
        std::vector<std::string> values;
        if (!IsOpen() || field.empty()) {
            return values;
        }

        const void* data = nullptr;
        size_t length = 0;

        // Use sd_journal_query_unique to get unique values for the field
        int r = sd_journal_query_unique(mJournal, field.c_str());
        if (r < 0) {
            return values;
        }

        size_t count = 0;
        constexpr size_t kMaxUniqueValues = 10000;

        SD_JOURNAL_FOREACH_UNIQUE(mJournal, data, length) {
            if (count >= kMaxUniqueValues) {
                break;
            }

            if (!data || length == 0) {
                continue;
            }

            std::string entry(static_cast<const char*>(data), length);

            // Split to get value part
            size_t equalPos = entry.find('=');
            if (equalPos != std::string::npos && equalPos + 1 < entry.length()) {
                std::string value = entry.substr(equalPos + 1);
                if (!value.empty()) {
                    values.push_back(std::move(value));
                }
            }
            count++;
        }
        return values;
    }


    /*---------------  Configuration Interface  ----------------*/
    bool SetJournalPaths(const std::vector<std::string>& p) {
        mJournalPaths = p;
        return true;
    }

    bool AddToEpoll(int epollFD) {
        if (!IsOpen() || epollFD < 0) {
            return false;
        }

        int fd = sd_journal_get_fd(mJournal);
        if (fd < 0) {
            return false;
        }

        struct epoll_event event = {};
        event.events = EPOLLIN;
        event.data.fd = fd;

        // Try ADD first, use MOD if fd already exists
        int result = epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event);
        if (result != 0) {
            if (errno == EEXIST) {
                // fd already exists, use MOD to modify event settings
                result = epoll_ctl(epollFD, EPOLL_CTL_MOD, fd, &event);
            }
            if (result != 0) {
                return false;
            }
        }

        return true;
    }

    int AddToEpollAndGetFD(int epollFD) {
        if (!IsOpen() || epollFD < 0) {
            return -1;
        }

        int fd = sd_journal_get_fd(mJournal);
        if (fd < 0) {
            return -1;
        }

        struct epoll_event event = {};
        event.events = EPOLLIN;
        event.data.fd = fd;

        // Try ADD first, use MOD if fd already exists
        int result = epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event);
        if (result != 0) {
            if (errno == EEXIST) {
                // fd already exists, use MOD to modify event settings
                result = epoll_ctl(epollFD, EPOLL_CTL_MOD, fd, &event);
            }
            if (result != 0) {
                return -1;
            }
        }

        return fd;
    }

    void RemoveFromEpoll(int epollFD) {
        if (!IsOpen() || epollFD < 0) {
            return;
        }

        int fd = sd_journal_get_fd(mJournal);
        if (fd >= 0) {
            epoll_ctl(epollFD, EPOLL_CTL_DEL, fd, nullptr);
        }
    }

    JournalStatusType CheckJournalStatus() {
        if (!IsOpen()) {
            return JournalStatusType::kError;
        }

        // Call sd_journal_process() to check journal status changes
        int ret = sd_journal_process(mJournal);

        // Convert to encapsulated enum type
        if (ret == SD_JOURNAL_NOP) {
            return JournalStatusType::kNop;
        }
        if (ret == SD_JOURNAL_APPEND) {
            return JournalStatusType::kAppend;
        }
        if (ret == SD_JOURNAL_INVALIDATE) {
            return JournalStatusType::kInvalidate;
        }
        return JournalStatusType::kError;
    }

    int GetJournalFD() const {
        if (!IsOpen()) {
            return -1;
        }

        int fd = sd_journal_get_fd(mJournal);
        return fd;
    }

private:
    sd_journal* mJournal{nullptr};

    template <typename F>
    bool call(F&& f) {
        return IsOpen() && f(mJournal) == 0;
    }

    int openDir(const std::string& dir) {
        try {
            std::error_code ec;
            if (!std::filesystem::exists(dir, ec) || ec) {
                return -ENOENT;
            }

            // 🔥 Key modification: Use sd_journal_open_directory instead of opening file list
            // This allows getting a valid fd for epoll monitoring
            // sd_journal_open_directory return value: 0 for success, negative for error
            int ret = sd_journal_open_directory(&mJournal, dir.c_str(), 0);
            if (ret < 0) {
                return ret;
            }

            return 0;

        } catch (...) {
            return -EIO;
        }
    }

    int openFile(const std::string& file) {
        try {
            std::error_code ec;
            if (!std::filesystem::exists(file, ec) || ec) {
                return -ENOENT;
            }

            const char* arr[] = {file.c_str(), nullptr};
            return sd_journal_open_files(&mJournal, arr, 0);

        } catch (...) {
            return -EIO;
        }
    }

    static constexpr size_t kDefaultDataThreshold = 64 * 1024;

    bool mIsOpen{false};
    size_t mDataThreshold{kDefaultDataThreshold};
    std::vector<std::string> mJournalPaths;
};

/*========================================================
 *  Public Interface Forwarding - Pimpl Pattern Implementation
 *
 *  Design Intent:
 *  1. Compilation Isolation: Avoid exposing systemd dependencies in headers, reduce header dependencies
 *  2. Interface Stability: Impl class implementation can be modified without affecting public API
 *========================================================*/
JournalReader::JournalReader() : mImpl(std::make_unique<Impl>()) {
}
JournalReader::~JournalReader() = default;

bool JournalReader::Open() {
    return mImpl->Open();
}

void JournalReader::Close() {
    mImpl->Close();
}

bool JournalReader::IsOpen() const {
    return mImpl->IsOpen();
}

bool JournalReader::SeekHead() {
    return mImpl->SeekHead();
}

bool JournalReader::SeekTail() {
    return mImpl->SeekTail();
}

bool JournalReader::SeekCursor(const std::string& cursor) {
    return mImpl->SeekCursor(cursor);
}

bool JournalReader::Next() {
    return mImpl->Next();
}

bool JournalReader::Previous() {
    return mImpl->Previous();
}

JournalReadStatus JournalReader::NextWithStatus() {
    return mImpl->NextWithStatus();
}

bool JournalReader::GetEntry(JournalEntry& entry) {
    return mImpl->GetEntry(entry);
}

std::string JournalReader::GetCursor() {
    return mImpl->GetCursor();
}

bool JournalReader::AddMatch(const std::string& field, const std::string& value) {
    return mImpl->AddMatch(field, value);
}

bool JournalReader::AddDisjunction() {
    return mImpl->AddDisjunction();
}

std::vector<std::string> JournalReader::GetUniqueValues(const std::string& field) {
    return mImpl->GetUniqueValues(field);
}

bool JournalReader::SetJournalPaths(const std::vector<std::string>& paths) {
    return mImpl->SetJournalPaths(paths);
}

bool JournalReader::AddToEpoll(int epollFD) {
    return mImpl->AddToEpoll(epollFD);
}

int JournalReader::AddToEpollAndGetFD(int epollFD) {
    return mImpl->AddToEpollAndGetFD(epollFD);
}

void JournalReader::RemoveFromEpoll(int epollFD) {
    mImpl->RemoveFromEpoll(epollFD);
}

JournalStatusType JournalReader::CheckJournalStatus() {
    return mImpl->CheckJournalStatus();
}

int JournalReader::GetJournalFD() const {
    return mImpl->GetJournalFD();
}

} // namespace logtail
