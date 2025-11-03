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
 *  Impl：Linux 下的 systemd-journal 实现
 *  注意：此插件仅在 Linux 平台可用
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

    /*---------------  打开 / 关闭  ----------------*/
    bool Open() {
        if (mIsOpen) {
            return true;
        }

        int ret = 0;

        if (mJournalPaths.empty()) {
            ret = sd_journal_open(&mJournal, SD_JOURNAL_LOCAL_ONLY);
        } else {
            // 添加边界检查
            if (mJournalPaths[0].empty()) {
                return false;
            }
            const std::string& path = mJournalPaths[0];

            // 验证路径存在性
            std::error_code ec;
            if (!std::filesystem::exists(path, ec)) {
                return false;
            }

            ret = std::filesystem::is_directory(path) ? openDir(path) : openFile(path);
        }

        if (ret < 0) {
            // 失败时清理资源
            if (mJournal != nullptr) {
                sd_journal_close(mJournal);
                mJournal = nullptr;
            }
            return false;
        }

        // 设置数据阈值
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

    /*---------------  遍历  ----------------*/
    bool SeekHead() {
        return call([](auto journal) { return sd_journal_seek_head(journal); });
    }
    bool SeekTail() {
        return call([](auto journal) { return sd_journal_seek_tail(journal); });
    }
    bool SeekCursor(const std::string& cursor) {
        // 添加参数验证
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
            // 区分错误和正常的结束情况
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
            // 成功移动到下一条，有数据
            return JournalReadStatus::kOk;
        }
        if (ret == 0) {
            // 到达末尾，没有更多数据
            return JournalReadStatus::kEndOfJournal;
        }
        // 错误情况 (ret < 0)
        // 常见错误码：
        // -ESTALE (116): Journal文件已被删除/轮转，cursor失效
        // -EINVAL: 参数无效
        // -EBADMSG: 日志文件损坏
        return JournalReadStatus::kError;
    }

    bool Previous() {
        if (!IsOpen()) {
            return false;
        }

        int ret = sd_journal_previous(mJournal);

        if (ret < 0) {
            // 区分错误和正常的结束情况
            return false;
        }
        return ret > 0;
    }

    /*---------------  读取单条  ----------------*/
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

        // 读取 realtime 时间戳
        // 注意：如果这里返回错误，通常意味着 journal 文件已被删除（轮转）
        // 需要触发错误恢复流程
        uint64_t timestamp = 0;
        int timeRet = sd_journal_get_realtime_usec(mJournal, &timestamp);
        if (timeRet < 0) {
            // 时间戳读取失败，这通常表示 journal 文件已被轮转删除
            // 返回 false 触发上层的错误恢复逻辑
            return false;
        }
        entry.realtimeTimestamp = timestamp;

        // 读取 monotonic 时间戳（这个可以失败，不是致命的）
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
        constexpr int kMaxFieldsPerEntry = 1000; // 防止内存爆炸

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

            // 边界检查
            size_t keyLen = equalSign - dataPtr;
            size_t valueLen = len - keyLen - 1;

            if (keyLen == 0 || valueLen == 0) {
                continue;
            }

            // 限制字段长度
            constexpr size_t kMaxFieldLength = 65536;
            if (keyLen > kMaxFieldLength || valueLen > kMaxFieldLength) {
                continue;
            }

            try {
                entry.fields.emplace(std::string(dataPtr, keyLen), std::string(equalSign + 1, valueLen));
            } catch (const std::bad_alloc&) {
                break; // 内存不足时停止添加字段
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

    /*---------------  过滤 / 等待  ----------------*/
    bool AddMatch(const std::string& field, const std::string& value) {
        if (!IsOpen() || field.empty() || value.empty()) {
            return false;
        }

        // 限制匹配字符串长度
        if (field.length() > 1024 || value.length() > 1024) {
            return false;
        }

        std::string keyValue = field + "=" + value;
        return sd_journal_add_match(mJournal, keyValue.c_str(), keyValue.size()) == 0;
    }

    bool AddDisjunction() { return IsOpen() && sd_journal_add_disjunction(mJournal) == 0; }

    std::vector<std::string> GetUniqueValues(const std::string& field) {
        std::vector<std::string> values;
        if (!IsOpen() || field.empty()) {
            return values;
        }

        const void* data = nullptr;
        size_t length = 0;

        // 使用sd_journal_query_unique获取字段的唯一值
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

            // 分割获取值部分
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


    /*---------------  配置接口  ----------------*/
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

        // 先尝试ADD，如果fd已存在则使用MOD
        int result = epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event);
        if (result != 0) {
            if (errno == EEXIST) {
                // fd已存在，使用MOD来修改事件设置
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

        // 先尝试ADD，如果fd已存在则使用MOD
        int result = epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event);
        if (result != 0) {
            if (errno == EEXIST) {
                // fd已存在，使用MOD来修改事件设置
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

        // 调用 sd_journal_process() 来检查 journal 状态变化
        int ret = sd_journal_process(mJournal);

        // 转换为封装的枚举类型
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

            // 🔥 关键修改：使用 sd_journal_open_directory 而不是打开文件列表
            // 这样可以获取有效的 fd 用于 epoll 监听
            // sd_journal_open_directory 返回值：0 表示成功，负数表示错误
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
 *  公共接口转发 - Pimpl 模式实现
 *
 *  设计意图：
 *  1. 编译隔离：避免头文件暴露 systemd 依赖，减小头文件依赖
 *  2. 接口稳定性：Impl 类实现可修改，不影响公共 API
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
