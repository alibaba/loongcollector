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
 
 #ifdef __linux__
 #  include <systemd/sd-journal.h>
 #endif
 
 namespace logtail {
 
 /*========================================================
  *  Impl：Linux 下真正干活；非 Linux 下只留空壳
  *========================================================*/
 class SystemdJournalReader::Impl {
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
 #ifdef __linux__
         if (mIsOpen) {
            return true;
        }
 
         int ret = 0;
 
                 if (mJournalPaths.empty()) {
            ret = sd_journal_open(&mJournal, SD_JOURNAL_LOCAL_ONLY);
        } else {
            const std::string& path = mJournalPaths[0];
            ret = std::filesystem::is_directory(path) ? openDir(path) : openFile(path);
        }
        if (ret < 0) {
            return false;
        }
        (void)sd_journal_set_data_threshold(mJournal, mDataThreshold);
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
        return call([&](auto journal){ return sd_journal_seek_cursor(journal, cursor.c_str()); });
    }
    
    bool Next() {  
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        int ret = sd_journal_next(mJournal);
        
        if (ret <= 0) {
            return false;
        }
        return true;
#else
        return mIsOpen;
#endif
    }
    
    bool Previous() { 
#ifdef __linux__
        if (!IsOpen()) {
            return false;
        }
        
        int ret = sd_journal_previous(mJournal);
        
        if (ret <= 0) {
            return false;
        }
        return true;
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
        if (sd_journal_get_cursor(mJournal, &cursorPtr) < 0) {
            return false;
        }
        std::unique_ptr<char, decltype(&free)> cursor(cursorPtr, &free);
        entry.cursor = cursor.get();

        uint64_t timestamp = 0;
        sd_journal_get_realtime_usec(mJournal, &timestamp);
        entry.realtimeTimestamp = timestamp;
        
        uint64_t monotonicTimestamp = 0;
        sd_id128_t bootId;
        sd_journal_get_monotonic_usec(mJournal, &monotonicTimestamp, &bootId);
        entry.monotonicTimestamp = monotonicTimestamp;
 
         const void* data = nullptr;
         size_t len = 0;
         sd_journal_restart_data(mJournal);
                 while (sd_journal_enumerate_data(mJournal, &data, &len) > 0) {
            const char* dataPtr = static_cast<const char*>(data);
                        const char* equalSign = static_cast<const char*>(memchr(dataPtr, '=', len));
            if (equalSign == nullptr) {
                continue;
            }
            entry.fields.emplace(std::string(dataPtr, equalSign),
                                 std::string(equalSign + 1, dataPtr + len));
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
        if (sd_journal_get_cursor(mJournal, &cursorPtr) < 0) {
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
         if (!IsOpen()) {
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
 
     int Wait(std::chrono::milliseconds timeout) {
 #ifdef __linux__
         if (!IsOpen()) { return -1;
}
                 return sd_journal_wait(mJournal,
               std::chrono::duration_cast<std::chrono::microseconds>(timeout).count());
#else
        std::this_thread::sleep_for(timeout);
         return 1; // 模拟有数据
 #endif
     }
 
     /*---------------  配置接口  ----------------*/
     bool SetDataThreshold(size_t t) {
         mDataThreshold = t;
 #ifdef __linux__
         if (IsOpen()) {
            sd_journal_set_data_threshold(mJournal, t);
        }
 #endif
         return true;
     }
     bool SetTimeout(std::chrono::milliseconds timeout)  { mTimeoutMs = (int)timeout.count(); return true; }
     bool SetJournalPaths(const std::vector<std::string>& p) { mJournalPaths = p; return true; }
 
 private:
 #ifdef __linux__
     sd_journal* mJournal{nullptr};
 
     template<typename F>
     bool call(F&& f) {
         return IsOpen() && f(mJournal) == 0;
     }
 
     int openDir(const std::string& dir) {
         std::vector<std::string> fs;
         for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
             if (e.is_regular_file() && e.path().extension() == ".journal") {
                 fs.emplace_back(e.path().string());
}
}
         if (fs.empty()) { return -ENOENT;
}
 
        std::vector<const char*> ptrs;
        ptrs.reserve(fs.size() + 1);
        for (auto& s : fs) {
            ptrs.push_back(s.c_str());
        }
         ptrs.push_back(nullptr);
         return sd_journal_open_files(&mJournal, ptrs.data(), 0);
     }
 
     int openFile(const std::string& file) {
                 const char* arr[] = { file.c_str(), nullptr };
        return sd_journal_open_files(&mJournal, static_cast<const char**>(arr), 0);
     }
 #endif
 
         static constexpr size_t kDefaultDataThreshold = 64 * 1024;
    static constexpr int kDefaultTimeoutMs = 1000;
    
    bool mIsOpen{false};
    size_t mDataThreshold{kDefaultDataThreshold};
    int mTimeoutMs{kDefaultTimeoutMs};
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
bool  SystemdJournalReader::GetEntry(JournalEntry& e)       { FWD(GetEntry(e)); }
std::string SystemdJournalReader::GetCursor()               { FWD(GetCursor()); }
bool  SystemdJournalReader::AddMatch(const std::string& f,
                                     const std::string& v)  { FWD(AddMatch(f,v)); }
bool  SystemdJournalReader::AddDisjunction()                { FWD(AddDisjunction()); }
int   SystemdJournalReader::Wait(std::chrono::milliseconds timeout){ FWD(Wait(timeout)); }
bool  SystemdJournalReader::SetDataThreshold(size_t t)      { FWD(SetDataThreshold(t)); }
bool  SystemdJournalReader::SetTimeout(std::chrono::milliseconds timeout){ FWD(SetTimeout(timeout)); }
bool  SystemdJournalReader::SetJournalPaths(const std::vector<std::string>& p){ FWD(SetJournalPaths(p)); }
#undef FWD
 
 } // namespace logtail
 