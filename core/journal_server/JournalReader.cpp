/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * ...
 */

 #include "journal_server/JournalReader.h"
 #include "logger/Logger.h"
 
 #include <chrono>
 #include <thread>
 #include <cstring>
 #include <filesystem>
 #include <iostream>
 
 #ifdef __linux__
 #  include <systemd/sd-journal.h>
 #endif
 
 namespace logtail {
 
 /*========================================================
  *  Impl：Linux 下真正干活；非 Linux 下只留空壳
  *========================================================*/
 class SystemdJournalReader::Impl {
 public:
     Impl()
 #ifdef __linux__
         : mJournal(nullptr)
 #endif
         , mIsOpen(false), mDataThreshold(64 * 1024), mTimeoutMs(1000) {}
 
     ~Impl() { Close(); }
 
     /*---------------  打开 / 关闭  ----------------*/
     bool Open() {
 #ifdef __linux__
         if (mIsOpen) return true;
 
         int ret = 0;
 
         if (mJournalPaths.empty()) {
             ret = sd_journal_open(&mJournal, SD_JOURNAL_LOCAL_ONLY);
         } else {
             const std::string& p = mJournalPaths[0];
             ret = std::filesystem::is_directory(p) ? openDir(p) : openFile(p);
         }
         if (ret < 0) {
             std::cerr << "journal open failed: " << strerror(-ret) << '\n';
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
         if (mJournal) {
             sd_journal_close(mJournal);
             mJournal = nullptr;
         }
 #endif
         mIsOpen = false;
     }
 
     bool IsOpen() const {
 #ifdef __linux__
         return mIsOpen && mJournal != nullptr;
 #else
         return mIsOpen;
 #endif
     }
 
     /*---------------  遍历  ----------------*/
     bool SeekHead()  { return call([](auto j){ return sd_journal_seek_head(j); }); }
     bool SeekTail()  { return call([](auto j){ return sd_journal_seek_tail(j); }); }
     bool SeekCursor(const std::string& c) {
         return call([&](auto j){ return sd_journal_seek_cursor(j, c.c_str()); });
     }
     bool Next()      { return call([](auto j){ return sd_journal_next(j) > 0; }); }
     bool Previous()  { return call([](auto j){ return sd_journal_previous(j) > 0; }); }
 
     /*---------------  读取单条  ----------------*/
     bool GetEntry(JournalEntry& entry) {
 #ifdef __linux__
         if (!IsOpen()) return false;
         entry = {};
 
         char* cursor = nullptr;
         if (sd_journal_get_cursor(mJournal, &cursor) < 0) return false;
         entry.cursor = cursor;
         free(cursor);
 
         uint64_t ts = 0;
         sd_journal_get_realtime_usec(mJournal, &ts);
         entry.realtimeTimestamp = ts;
 
         const void* data = nullptr;
         size_t len = 0;
         sd_journal_restart_data(mJournal);
         while (sd_journal_enumerate_data(mJournal, &data, &len) > 0) {
             const char* p = static_cast<const char*>(data);
             const char* eq = static_cast<const char*>(memchr(p, '=', len));
             if (!eq) continue;
             entry.fields.emplace(std::string(p, eq),
                                  std::string(eq + 1, p + len));
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
         if (!IsOpen()) return "";
         char* c = nullptr;
         if (sd_journal_get_cursor(mJournal, &c) < 0) return "";
         std::string res(c);
         free(c);
         return res;
 #else
         return mIsOpen ? "simulated_cursor" : "";
 #endif
     }
 
     /*---------------  过滤 / 等待  ----------------*/
     bool AddMatch(const std::string& f, const std::string& v) {
 #ifdef __linux__
         if (!IsOpen()) return false;
         std::string kv = f + "=" + v;
         return sd_journal_add_match(mJournal, kv.c_str(), kv.size()) == 0;
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
 
     int Wait(std::chrono::milliseconds ms) {
 #ifdef __linux__
         if (!IsOpen()) return -1;
         return sd_journal_wait(mJournal,
                std::chrono::duration_cast<std::chrono::microseconds>(ms).count());
 #else
         std::this_thread::sleep_for(ms);
         return 1; // 模拟有数据
 #endif
     }
 
     /*---------------  配置接口  ----------------*/
     bool SetDataThreshold(size_t t) {
         mDataThreshold = t;
 #ifdef __linux__
         if (IsOpen()) sd_journal_set_data_threshold(mJournal, t);
 #endif
         return true;
     }
     bool SetTimeout(std::chrono::milliseconds ms)  { mTimeoutMs = (int)ms.count(); return true; }
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
         for (auto& e : std::filesystem::recursive_directory_iterator(dir))
             if (e.is_regular_file() && e.path().extension() == ".journal")
                 fs.emplace_back(e.path().string());
         if (fs.empty()) return -ENOENT;
 
         std::vector<const char*> ptrs;
         for (auto& s : fs) ptrs.push_back(s.c_str());
         ptrs.push_back(nullptr);
         return sd_journal_open_files(&mJournal, ptrs.data(), 0);
     }
 
     int openFile(const std::string& file) {
         const char* arr[] = { file.c_str(), nullptr };
         return sd_journal_open_files(&mJournal, arr, 0);
     }
 #endif
 
     bool mIsOpen{false};
     size_t mDataThreshold{64 * 1024};
     int mTimeoutMs{1000};
     std::vector<std::string> mJournalPaths;
 };
 
 /*========================================================
  *  公共接口空壳转发
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
 int   SystemdJournalReader::Wait(std::chrono::milliseconds ms){ FWD(Wait(ms)); }
 bool  SystemdJournalReader::SetDataThreshold(size_t t)      { FWD(SetDataThreshold(t)); }
 bool  SystemdJournalReader::SetTimeout(std::chrono::milliseconds ms){ FWD(SetTimeout(ms)); }
 bool  SystemdJournalReader::SetJournalPaths(const std::vector<std::string>& p){ FWD(SetJournalPaths(p)); }
 #undef FWD
 
 } // namespace logtail
 