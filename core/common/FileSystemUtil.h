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
#include <string>
#include <memory>
#include <vector>
#include <sys/stat.h>
#include <boost/regex.hpp>
#if defined(__linux__)
#include <dirent.h>
#elif defined(_MSC_VER)
#include <Windows.h>
#endif
#include "ErrorUtil.h"
#include "DevInode.h"
#include "LogtailCommonFlags.h"

// Filesystem utility.
namespace logtail {

// Separator of path, use std::string to concat with const char *.
extern const std::string PATH_SEPARATOR;

// PathJoin concats base and sub (by adding necessary path separator).
// NOTE: the implementation is not elegant for better performance (backward).
inline std::string PathJoin(const std::string& base, const std::string& sub) {
    // Only Windows can collect root path, so linux do as old way.
#if defined(__linux__)
    return base + PATH_SEPARATOR + sub;
#elif defined(_MSC_VER)
    if (!BOOL_FLAG(enable_root_path_collection) || base.back() != PATH_SEPARATOR[0]) {
        return base + PATH_SEPARATOR + sub;
    }
    return base + sub;
#endif
}

std::string ParentPath(const std::string& path);

// CheckExistance checks whether the dir or file specified by @path is exist or not.
bool CheckExistance(const std::string& path);

bool Mkdirs(const std::string& dirPath);

// Mkdir @dirPath.
// NOTE: access errno directly for more information instead of GetErrno().
bool Mkdir(const std::string& dirPath);
// For Mkdir Only.
inline bool IsEEXIST(int e) {
#if defined(__linux__)
    return (EEXIST == e);
#elif defined(_MSC_VER)
    return (ERROR_ALREADY_EXISTS == e);
#endif
}

bool IsRelativePath(const std::string& path);
std::string AbsolutePath(const std::string& path, const std::string& basepath);

// FSeek, FTell that can handle file large than 2GB on 32bits OS.
int FSeek(FILE* stream, int64_t offset, int origin);
int64_t FTell(FILE* stream);

// TrimLastSeperator removes last path separator unless @path is equal to '/'.
void TrimLastSeperator(std::string& path);

// ReadFileContent reads all content of @fileName to @content.
bool ReadFileContent(const std::string& fileName, std::string& content);

// OverwriteFile overwrides @fileName with @content.
bool OverwriteFile(const std::string& fileName, const std::string& content);

// IsAccessibleDirectory checks if the @dirPath is a existent directory and accessible.
// Accessibility means that we can iterate the contents of it.
bool IsAccessibleDirectory(const std::string& dirPath);

// GetAllFiles gets all files that match @filePattern in @dirPath, save their names into @allFiles.
// @filePattern: supports wildcard, such as *.log. If no wildcard in it, just return one file
//   named @filePattern.
// @return true if succeeds.
bool GetAllFiles(const std::string& dirPath, const std::string& filePattern, std::vector<std::string>& allFiles);

// By default, fopen on Windows will make the opened file locked (even in read-only mode),
// which will affect users' actions such as deletion, renaming, rotating on that file.
// So we offer a series of functions to solve this problem.
// Functions will not check the @mode param, make sure what you pass is right.
FILE* FileReadOnlyOpen(const char* filePath, const char* mode = "r");
FILE* FileWriteOnlyOpen(const char* filePath, const char* mode = "w");
FILE* FileAppendOpen(const char* filePath, const char* mode = "a");

// Logtail will ignore files with special suffix.
bool IsValidSuffix(const std::string& filename);

// Return true if filePath is existing and empty, otherwise false.
bool IsEmptyConfigJSONFile(const std::string& filePath);

std::string GetFdPath(int fd);

#ifdef _MSC_VER
typedef int mode_t;
#endif
void Chmod(const char* filePath, mode_t mode);

namespace fsutil {

    class Entry {
    public:
        enum class Type { UNKNOWN, DIR, REG_FILE };

        Entry() {}
        Entry(const std::string& name, Type type, bool isSymbolic)
            : mName(name), mType(type), mIsSymbolic(isSymbolic) {}

        std::string Name() const { return mName; }
        bool IsDir() const { return Type::DIR == mType; }
        bool IsRegFile() const { return Type::REG_FILE == mType; }
        bool IsSymbolic() const { return mIsSymbolic; }

        // It's caller's job to deal unknown entry type.
        operator bool() const { return !mName.empty(); }

    private:
        std::string mName;
        Type mType = Type::UNKNOWN;
        bool mIsSymbolic = false;
    };

    class Dir {
        Dir(const Dir&) = delete;
        Dir& operator=(const Dir&) = delete;

    public:
        Dir(const std::string& dirPath);
        ~Dir();

        bool Open();

        // Utility function to decide why Open failed.
        inline static bool IsENOENT(int e) {
#if defined(__linux__)
            return (ENOENT == e);
#elif defined(_MSC_VER)
            return (ERROR_PATH_NOT_FOUND == e || ERROR_FILE_NOT_FOUND == e);
#endif
        }
        inline static bool IsENOTDIR(int e) {
#if defined(__linux__)
            return (ENOTDIR == e);
#elif defined(_MSC_VER)
            return (ERROR_DIRECTORY == e);
#endif
        }
        inline static bool IsEACCES(int e) {
#if defined(__linux__)
            return (EACCES == e);
#elif defined(_MSC_VER)
            return (ERROR_ACCESS_DENIED == e);
#endif
        }

        // ReadNext reads next entry (FILE or DIR).
        // @param resolveWithStat: if true, use stat to check entry type when the entry
        //   type from filesystem directory iteration API is not FILE or DIR,
        //   eg. LNK type under Linux.
        // @return: the entry of the file. If it is sure that we reached the end, an
        //   'false' entry will be returned. Otherwise, we will use UNKNOWN type entry
        //   to indicate some errors, it is caller's job to handle this (eg. skip it).
        // TODO: Most error happened when the entry is symbolic, maybe we can ignore
        //   this and let caller to call something else to deal it.
        Entry ReadNext(bool resolveWithStat = true);

        void Close();

    private:
        std::string mDirPath;
#if defined(__linux__)
        DIR* mDir;
#elif defined(_MSC_VER)
        HANDLE mFind;
        // Because FindFirstFile will return first entry, we have to cache it.
        Entry mCachedEntry;
#endif

        bool IsOpened() const;
    };
#if defined(__linux__)
    typedef struct stat RawStatType;
#elif defined(_MSC_VER)
    typedef struct _stat64 RawStatType;
#endif

    class PathStat {
        std::string mPath;
        RawStatType mRawStat;

    public:
        PathStat();
        ~PathStat();

        RawStatType* GetRawStat() { return &mRawStat; }

        // stat wrappers ::stat.
        static bool stat(const std::string& path, PathStat& ps);
        bool IsDir() const;
        bool IsRegFile() const;

        // lstat wrappers Linux ::lstat and implements it with ::stat on Windows.
        static bool lstat(const std::string& path, PathStat& ps);
        bool IsLink() const;

        // fstat wrappers ::fstat.
        // @resolvePath only works on Windows.
        //   If it is true, fstat will get corresponding file path according to @file and
        //   set it to @ps.mPath, however, if this procedure failed, false will be returned
        //   although the call to ::fstat succeeded.
        //   **Use it when you want to call GetDevInode() or GetLastWriteTime().**
        //   **Disable it if you only want to call GetMtime() or GetFileSize().**
        static bool fstat(FILE* file, PathStat& ps, bool resolvePath = true);
        static bool fstat(int fd, PathStat& ps, bool resolvePath = true);
#if defined(_MSC_VER)
        static bool fstat(HANDLE hFile, PathStat& ps, bool resolvePath = true);
#endif

        // For Windows, GetDevInode and GetLastWriteTime have to call other system APIs to
        // get information (by mPath), this will spend extra costs.
        DevInode GetDevInode() const;
        void GetLastWriteTime(int64_t& sec, int64_t& nsec) const;

        // GetMtime and GetFileSize return st_mtime and st_size in struct stat. They needn't
        // to call another system APIs.
        time_t GetMtime() const;
        int64_t GetFileSize() const;

        // GetMode returns st_mode.
        int GetMode() const { return static_cast<int>(mRawStat.st_mode); }
    };

} // namespace fsutil

} // namespace logtail
