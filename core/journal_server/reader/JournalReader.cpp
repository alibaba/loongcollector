/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * ...
 */

#include "JournalReader.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <system_error>

#ifdef __linux__
#include <systemd/sd-journal.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#endif

namespace logtail {

// ============================================================================
// SystemdJournalReader 实现
// ============================================================================

/*========================================================
 *  Impl：Linux 下真正干活；非 Linux 下只留空壳
 *========================================================*/
class SystemdJournalReader::Impl {
public:
    Impl() = default;

    ~Impl() { 
        Close(); 
    }
    
    // Delete copy and move operations
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    /*---------------  打开 / 关闭  ----------------*/
    bool Open() {
#ifdef __linux__
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
#else
        mIsOpen = true;
        return true;
#endif
    }

    void Close() {
#ifdef __linux__
        if (mJournal != nullptr) {
            sd_journal_close(mJournal);
            mJournal = nullptr;
        }
#endif
        mIsOpen = false;
    }

    [[nodiscard]] bool IsOpen() const {
#ifdef __linux__
        return mIsOpen && mJournal != nullptr;
#else
        return mIsOpen;
#endif
    }

    /*---------------  遍历  ----------------*/
    bool SeekHead()  { return call([](auto journal){ return sd_journal_seek_head(journal); }); }
    bool SeekTail()  { return call([](auto journal){ return sd_journal_seek_tail(journal); }); }
    bool SeekCursor(const std::string& cursor) {
        // 添加参数验证
        if (cursor.empty()) {
            return false;
        }
        return call([&](auto journal){ return sd_journal_seek_cursor(journal, cursor.c_str()); });
    }
    
    bool Next() {  
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        int ret = sd_journal_next(mJournal);
        
        if (ret < 0) {
            // 区分错误和正常的结束情况
            return false;
        }
        return ret > 0;
#else
        return mIsOpen;
#endif
    }
    
    JournalReadStatus NextWithStatus() {
#ifdef __linux__
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
#else
        return mIsOpen ? JournalReadStatus::kOk : JournalReadStatus::kError;
#endif
    }
    
    bool Previous() { 
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        int ret = sd_journal_previous(mJournal);
        
        if (ret < 0) {
            // 区分错误和正常的结束情况
            return false;
        }
        return ret > 0;
#else
        return mIsOpen;
#endif
    }

    /*---------------  读取单条  ----------------*/
    bool GetEntry(JournalEntry& entry) {
#ifdef __linux__
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
        // 注意：根据 fluentbit 经验，如果这里返回错误，通常意味着 journal 文件已被删除（轮转）
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
                entry.fields.emplace(std::string(dataPtr, keyLen),
                                   std::string(equalSign + 1, valueLen));
            } catch (const std::bad_alloc&) {
                break; // 内存不足时停止添加字段
            }
            fieldCount++;
        }
        return true;
#else
        entry = { "simulated_cursor", 0, 0,
                  {{"MESSAGE","Simulated entry"},
                   {"_SYSTEMD_UNIT","simulated.service"}}};
        return mIsOpen;
#endif
    }

    std::string GetCursor() {
#ifdef __linux__
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
#else
        return mIsOpen ? "simulated_cursor" : "";
#endif
    }

    /*---------------  过滤 / 等待  ----------------*/
    bool AddMatch(const std::string& field, const std::string& value) {
#ifdef __linux__
        if (!IsOpen() || field.empty() || value.empty()) {
           return false;
        }
        
        // 限制匹配字符串长度
        if (field.length() > 1024 || value.length() > 1024) {
            return false;
        }
        
        std::string keyValue = field + "=" + value;
        return sd_journal_add_match(mJournal, keyValue.c_str(), keyValue.size()) == 0;
#else
        return mIsOpen;
#endif
    }

    bool AddDisjunction() {
#ifdef __linux__
        return IsOpen() && sd_journal_add_disjunction(mJournal) == 0;
#else
        return mIsOpen;
#endif
   }
   
   std::vector<std::string> GetUniqueValues(const std::string& field) {
       std::vector<std::string> values;
#ifdef __linux__
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
#endif
       return values;
   }


    /*---------------  配置接口  ----------------*/
    bool SetJournalPaths(const std::vector<std::string>& p) { mJournalPaths = p; return true; }
    
#ifdef __linux__
    
    bool AddToEpoll(int epollFD) {
        if (!IsOpen() || epollFD < 0) {
            printf("[JournalReader] AddToEpoll failed: reader not open or invalid epollFD (is_open=%d, epoll_fd=%d)\n", 
                   IsOpen() ? 1 : 0, epollFD);
            return false;
        }
        
        int fd = sd_journal_get_fd(mJournal);
        if (fd < 0) {
            printf("[JournalReader] AddToEpoll failed: sd_journal_get_fd returned negative (fd=%d, errno=%d)\n", 
                   fd, errno);
            return false;
        }
        
        struct epoll_event event = {};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = fd;
        
        int result = epoll_ctl(epollFD, EPOLL_CTL_ADD, fd, &event);
        if (result != 0) {
            printf("[JournalReader] AddToEpoll failed: epoll_ctl failed (epoll_fd=%d, journal_fd=%d, errno=%d, error=%s)\n", 
                   epollFD, fd, errno, strerror(errno));
            return false;
        }
        
        printf("[JournalReader] AddToEpoll succeeded (epoll_fd=%d, journal_fd=%d)\n", epollFD, fd);
        return true;
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
    
    bool ProcessJournalEvent() {
        if (!IsOpen()) {
            return false;
        }
        
        // 调用 sd_journal_process() 来处理 journal 事件
        // 返回值含义：
        // - SD_JOURNAL_NOP (0): 没有变化
        // - SD_JOURNAL_APPEND (1): 新条目被添加
        // - SD_JOURNAL_INVALIDATE (2): 条目被移除/失效（日志轮转）
        // - 负值: 错误
        int ret = sd_journal_process(mJournal);
        
        if (ret == SD_JOURNAL_NOP) {
            // 没有变化，可以继续处理
            return true;
        }
        
        if (ret == SD_JOURNAL_APPEND) {
            // 新条目被添加，可以继续处理
            return true;
        }
        
        if (ret == SD_JOURNAL_INVALIDATE) {
            // Journal 文件被添加、删除或轮转
            // 根据 fluentbit 经验：打印日志，但继续处理
            // 后续的 Next() 调用会处理这种情况（通过错误恢复机制）
            printf("[JournalReader] ProcessJournalEvent: SD_JOURNAL_INVALIDATE received (ret=%d), "
                   "journal files changed (rotation/deletion), continuing\n", ret);
            return true;
        }
        
        // 错误情况（ret < 0）
        printf("[JournalReader] ProcessJournalEvent failed: sd_journal_process returned %d, "
               "errno=%d (%s)\n", ret, errno, strerror(errno));
        return false;
    }
    
    int GetJournalFD() const {
        if (!IsOpen()) {
            printf("[JournalReader] GetJournalFD failed: reader not open\n");
            return -1;
        }
        
        int fd = sd_journal_get_fd(mJournal);
        printf("[JournalReader] GetJournalFD: journal_fd=%d, errno=%d\n", fd, errno);
        
        if (fd < 0) {
            printf("[JournalReader] GetJournalFD failed: sd_journal_get_fd returned %d, errno=%d (%s)\n", 
                   fd, errno, strerror(errno));
        }
        
        return fd;
    }
#endif

private:
#ifdef __linux__
    sd_journal* mJournal{nullptr};

    template<typename F>
    bool call(F&& f) {
        return IsOpen() && f(mJournal) == 0;
    }

    int openDir(const std::string& dir) {
        try {
            std::vector<std::string> fs;
            
            std::error_code ec;
            if (!std::filesystem::exists(dir, ec) || ec) {
                return -ENOENT;
            }
            
            for (const auto& e : std::filesystem::recursive_directory_iterator(dir, ec)) {
                if (ec) {
                    continue;
                }
                
                if (e.is_regular_file() && e.path().extension() == ".journal") {
                    fs.emplace_back(e.path().string());
                }
            }
            
            if (fs.empty()) { 
                return -ENOENT;
            }

           std::vector<const char*> ptrs;
           ptrs.reserve(fs.size() + 1);
           for (auto& s : fs) {
               ptrs.push_back(s.c_str());
           }
           ptrs.push_back(nullptr);
           return sd_journal_open_files(&mJournal, ptrs.data(), 0);
           
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
            
            const char* arr[] = { file.c_str(), nullptr };
            return sd_journal_open_files(&mJournal, arr, 0);
            
        } catch (...) {
            return -EIO;
        }
    }
#endif

    static constexpr size_t kDefaultDataThreshold = 64 * 1024;
   
   bool mIsOpen{false};
   size_t mDataThreshold{kDefaultDataThreshold};
   std::vector<std::string> mJournalPaths;
};

/*========================================================
*  公共接口转发 - Pimpl 模式实现
*  
*  设计意图：
*  1. 编译隔离：避免头文件暴露 systemd 依赖
*  2. 跨平台兼容：Linux/非Linux 统一接口
*  3. 扩展性：支持抽象接口和多实现
*========================================================*/
SystemdJournalReader::SystemdJournalReader()
   : mImpl(std::make_unique<Impl>()) {}
SystemdJournalReader::~SystemdJournalReader() = default;

#define FWD(Method) return mImpl->Method
bool  SystemdJournalReader::Open()                          { FWD(Open()); }
void  SystemdJournalReader::Close()                         { FWD(Close()); }
bool  SystemdJournalReader::IsOpen() const                  { FWD(IsOpen()); }
bool  SystemdJournalReader::SeekHead()                      { FWD(SeekHead()); }
bool  SystemdJournalReader::SeekTail()                      { FWD(SeekTail()); }
bool  SystemdJournalReader::SeekCursor(const std::string& c){ FWD(SeekCursor(c)); }
bool  SystemdJournalReader::Next()                          { FWD(Next()); }
bool  SystemdJournalReader::Previous()                      { FWD(Previous()); }
JournalReadStatus SystemdJournalReader::NextWithStatus()    { FWD(NextWithStatus()); }
bool  SystemdJournalReader::GetEntry(JournalEntry& e)       { FWD(GetEntry(e)); }
std::string SystemdJournalReader::GetCursor()               { FWD(GetCursor()); }
bool  SystemdJournalReader::AddMatch(const std::string& f,
                                    const std::string& v)  { FWD(AddMatch(f,v)); }
bool  SystemdJournalReader::AddDisjunction()                { FWD(AddDisjunction()); }
std::vector<std::string> SystemdJournalReader::GetUniqueValues(const std::string& field) { FWD(GetUniqueValues(field)); }
bool  SystemdJournalReader::SetJournalPaths(const std::vector<std::string>& p){ FWD(SetJournalPaths(p)); }

#ifdef __linux__
bool SystemdJournalReader::AddToEpoll(int epollFD) {
    return mImpl->AddToEpoll(epollFD);
}

void SystemdJournalReader::RemoveFromEpoll(int epollFD) {
    mImpl->RemoveFromEpoll(epollFD);
}

bool SystemdJournalReader::ProcessJournalEvent() {
    return mImpl->ProcessJournalEvent();
}

int SystemdJournalReader::GetJournalFD() const {
    return mImpl->GetJournalFD();
}
#endif
#undef FWD

} // namespace logtail
 