// Copyright 2022 iLogtail Authors
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

#include "FileSystemUtil.h"
#if defined(_MSC_VER)
#include <direct.h>
#include <fcntl.h>
#elif defined(__linux__)
#include <fnmatch.h>
#endif
#include <boost/filesystem.hpp>
#include "logger/Logger.h"
#include "StringTools.h"
#include "RuntimeUtil.h"
using namespace std;

namespace logtail {

#if defined(__linux__)
const std::string PATH_SEPARATOR = "/";
#elif defined(_MSC_VER)
const std::string PATH_SEPARATOR = "\\";
#endif

std::string ParentPath(const std::string& path) {
    boost::filesystem::path p(path);
    return p.parent_path().string();
}

bool CheckExistance(const std::string& path) {
    boost::system::error_code ec;
    boost::filesystem::path p(path);
    return boost::filesystem::exists(p, ec);
}

bool Mkdirs(const std::string& dirPath) {
    if (Mkdir(dirPath)) {
        return true;
    }
    if (errno != ENOENT) {
        return false;
    }
    boost::filesystem::path p(dirPath);
    if (!p.has_parent_path()) {
        return false;
    }
    if (!Mkdirs(ParentPath(dirPath))) {
        return false;
    }
    return Mkdir(dirPath);
}

bool Mkdir(const std::string& dirPath) {
#if defined(__linux__)
    if (mkdir(dirPath.c_str(), S_IXUSR | S_IWUSR | S_IRUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) != 0) {
        if (errno != EEXIST)
            return false;
    }
    return true;
#elif defined(_MSC_VER)
    if (-1 == _mkdir(dirPath.c_str())) {
        if (errno != EEXIST)
            return false;
    }
    return true;
#endif
}

bool IsRelativePath(const std::string& path) {
    boost::filesystem::path checkPointFilePath(path);
    return checkPointFilePath.is_relative();
}

std::string AbsolutePath(const std::string& path, const std::string& basepath) {
    return boost::filesystem::absolute(path, basepath).string();
}

int FSeek(FILE* stream, int64_t offset, int origin) {
#if defined(_MSC_VER)
    return _fseeki64(stream, offset, origin);
#elif defined(__linux__)
    return fseek(stream, offset, origin);
#endif
}

int64_t FTell(FILE* stream) {
#if defined(_MSC_VER)
    return _ftelli64(stream);
#elif defined(__linux__)
    return ftell(stream);
#endif
}

void TrimLastSeperator(std::string& path) {
    // do not trim '/'
    if (path.size() > 1 && path[path.size() - 1] == PATH_SEPARATOR[0]) {
        path = path.substr(0, path.size() - 1);
    }
}

bool ReadFileContent(const std::string& fileName, std::string& content) {
    FILE* pFile = fopen(fileName.c_str(), "r");
    if (pFile == NULL) {
        APSARA_LOG_DEBUG(sLogger, ("open file fail", fileName)("errno", strerror(errno)));
        return false;
    }

    content.clear();
    char* buffer = new char[8192];
    uint32_t readBytes = fread(buffer, 1, 8192, pFile);
    if (readBytes > 0) {
        content.append(buffer, readBytes);
        delete[] buffer;
    } else
        delete[] buffer;

    fclose(pFile);
    return true;
}

bool OverwriteFile(const std::string& fileName, const std::string& content) {
    FILE* pFile = fopen(fileName.c_str(), "w");
    if (pFile == NULL) {
        APSARA_LOG_ERROR(sLogger, ("open file fail", fileName)("errno", strerror(errno)));
        return false;
    }
    Chmod(fileName.c_str(), 0644);

    uint32_t writeBytes = fwrite(content.c_str(), 1, content.size(), pFile);
    if (writeBytes != content.size()) {
        APSARA_LOG_ERROR(sLogger,
                         ("write file fail", fileName)("errno", strerror(errno))("content.size", content.size())(
                             "writeBytes", writeBytes));
        fclose(pFile);
        return false;
    }
    if (fclose(pFile) != 0) {
        APSARA_LOG_ERROR(sLogger, ("close file fail", fileName)("errno", strerror(errno)));
        return false;
    }
    return true;
}

bool IsAccessibleDirectory(const std::string& dirPath) {
    boost::filesystem::directory_iterator end;
    try {
        boost::filesystem::directory_iterator dirIter(dirPath);
        return (dirIter != end);
    } catch (...) {
        return false;
    }
}

bool GetAllFiles(const std::string& dirPath, const std::string& filePattern, std::vector<std::string>& allFiles) {
    allFiles.clear();
    {
        // If no wildcard charaters in filePattern, only one file.
        size_t i = 0;
        for (; i < filePattern.size(); ++i) {
            if (filePattern[i] == '?' || filePattern[i] == '*') {
                break;
            }
        }
        if (i == filePattern.size()) {
            allFiles.push_back(filePattern);
            return true;
        }
    }

    fsutil::Dir dir(dirPath);
    if (!dir.Open()) {
        APSARA_LOG_ERROR(sLogger, ("Open dir fail", dirPath)("errno", GetErrno()));
        return false;
    }
    int32_t readCount = 0;
    while (auto ent = dir.ReadNext(false)) {
        ++readCount;
        if (ent.IsDir())
            continue;
        if (0 == fnmatch(filePattern.c_str(), ent.Name().c_str(), FNM_PATHNAME)) {
            allFiles.push_back(ent.Name());
        }
    }
    APSARA_LOG_INFO(sLogger,
                    ("load wildcard local event, dir",
                     dirPath)("file", filePattern)("read count", readCount)("matched count", allFiles.size()));
    return true;
}

#if defined(_MSC_VER)
// Convert Windows file HANDLE to FILE*.
// By _open_osfhandle -> _fdopen, only need to close the last one.
FILE* FromFileHandle(HANDLE hFile, int flags, const char* mode) {
    if (INVALID_HANDLE_VALUE == hFile)
        return NULL;
    int nHandle = _open_osfhandle((long)hFile, flags);
    if (-1 == nHandle) {
        CloseHandle(hFile);
        return NULL;
    }
    FILE* f = _fdopen(nHandle, mode);
    if (NULL == f) {
        CloseHandle(hFile);
        return NULL;
    }
    return f;
}
#endif

FILE* FileReadOnlyOpen(const char* filePath, const char* mode) {
#if defined(__linux__)
    return fopen(filePath, mode);
#elif defined(_MSC_VER)
    HANDLE hFile = CreateFile(filePath,
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    return FromFileHandle(hFile, _O_RDONLY, mode);
#endif
}

FILE* FileWriteOnlyOpen(const char* filePath, const char* mode) {
#if defined(__linux__)
    FILE* f = fopen(filePath, mode);
    if (f) {
        Chmod(filePath, 0644);
    }
    return f;
#elif defined(_MSC_VER)
    HANDLE hFile = CreateFile(filePath,
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    return FromFileHandle(hFile, _O_WRONLY, mode);
#endif
}

FILE* FileAppendOpen(const char* filePath, const char* mode) {
#if defined(__linux__)
    FILE* f = fopen(filePath, mode);
    if (f) {
        Chmod(filePath, 0644);
    }
    return f;
#elif defined(_MSC_VER)
    HANDLE hFile = CreateFile(filePath,
                              FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              NULL,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL);
    auto f = FromFileHandle(hFile, _O_APPEND, mode);
    if (NULL == f)
        return NULL;
    // We have to call fseek manually, because _fdopen will not synchronize offset
    // from HANDLE during openning.
    if (0 == FSeek(f, 0, SEEK_END))
        return f;
    LOG_WARNING(sLogger, ("Call fseek faild", errno)("FilePath", filePath));
    fclose(f);
    return NULL;
#endif
}

std::string GetFdPath(int fd) {
#if defined(_MSC_VER)
    auto oshandle = _get_osfhandle(fd);
    auto handle = (HANDLE)oshandle;
    if (INVALID_HANDLE_VALUE == handle) {
        return "";
    }
    char filePath[MAX_PATH + 1];
    auto ret = GetFinalPathNameByHandle(handle, filePath, MAX_PATH + 1, VOLUME_NAME_DOS);
    if (ret > MAX_PATH || ret <= 0) {
        return "";
    }
    if (0 == memcmp(filePath, "\\\\?\\", 4)) {
        return std::string(filePath + 4);
    }
    return std::string(filePath);
#else
    /* NOTE: readlink() does not append a null byte to buf */
    char buf[PATH_MAX + 1] = {0};
    char path[PATH_MAX];
    ssize_t ssize;
    // here we use /proc/self/fd rather than /dev/fd because /dev/fd may not exist in docker.
    snprintf(path, PATH_MAX, "/proc/self/fd/%d", fd);
    if ((ssize = readlink(path, buf, PATH_MAX)) != -1) {
        return std::string(buf);
    }
    return std::string();
#endif
}

void Chmod(const char* filePath, mode_t mode) {
#if defined(__linux__)
    if (chmod(filePath, mode) == -1) {
        APSARA_LOG_ERROR(sLogger, ("chmod error", filePath)("mode", mode)("errno", errno));
    }
#endif
}

bool IsValidSuffix(const std::string& filename) {
    // such as compress file (*.gz) or its rollback file (*.gz.*) will be ignored
    static const std::string FILTER_LIST[] = {".gz", ".bz", ".tar"};
    for (uint32_t i = 0; i < sizeof(FILTER_LIST) / sizeof(FILTER_LIST[0]); i++) {
        if (EndWith(filename, FILTER_LIST[i])) {
            return false;
        };
    }
    return true;
}

bool IsEmptyConfigJSONFile(const std::string& filePath) {
    auto fullFilePath = IsRelativePath(filePath) ? GetProcessExecutionDir() + filePath : filePath;
    std::ifstream in;
    in.open(fullFilePath.c_str());
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    return 0 == in.tellg();
}

namespace fsutil {

    Dir::Dir(const std::string& dirPath) : mDirPath(dirPath) {
#if defined(__linux__)
        mDir = nullptr;
#elif defined(_MSC_VER)
        mFind = INVALID_HANDLE_VALUE;
#endif
    }

    Dir::~Dir() { Close(); }

#if defined(_MSC_VER)
    // Invalid entry is returned if fileName starts with ., such as ., .., hidden files.
    static Entry ConstructEntry(const WIN32_FIND_DATA& findData) {
        std::string fileName(findData.cFileName);
        if (0 == fileName.find("."))
            return Entry();

        // NOTE: We assume there are only two types: DIR and REG_FILE, ignore symbolic.
        // In fact, for symbolic path, both boost::filesystem::status and Windows stat
        // will return FILE...
        return Entry(fileName,
                     (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? Entry::Type::DIR : Entry::Type::REG_FILE,
                     false);
    }
#endif

    bool Dir::IsOpened() const {
#if defined(__linux__)
        return mDir != nullptr;
#elif defined(_MSC_VER)
        return mFind != INVALID_HANDLE_VALUE;
#endif
    }

    bool Dir::Open() {
        if (IsOpened())
            return true;
        if (mDirPath.empty()) {
            // Log it but don't return, let following call to generate errno.
            LOG_WARNING(sLogger, ("Empty dir path", mDirPath));
        }

#if defined(__linux__)
        DIR* dir = opendir(mDirPath.c_str());
        if (NULL == dir) {
            // TODO: Record errno
            return false;
        }

        mDir = dir;
        return true;
#elif defined(_MSC_VER)
        auto findPath = PathJoin(mDirPath, "*");
        WIN32_FIND_DATA ffd;
        mFind = FindFirstFile(findPath.c_str(), &ffd);
        if (INVALID_HANDLE_VALUE == mFind) {
            // TODO: Record errno.
            return false;
        }
        // Cache first entry.
        mCachedEntry = ConstructEntry(ffd);
        return true;
#endif
    }

    Entry Dir::ReadNext(bool resolveWithStat) {
        if (!IsOpened())
            return Entry();

#if defined(__linux__)
        // Call readdir until it returns nullptr or non-dot entry.
        while (true) {
            struct dirent* ent = readdir(mDir);
            if (nullptr == ent)
                return Entry();

            std::string fileName(ent->d_name);
            if (0 == fileName.find('.')) {
                continue;
            }

            Entry::Type type;
            bool isSymbolic = false;
            switch (ent->d_type) {
                case DT_FIFO:
                case DT_CHR:
                case DT_BLK:
                case DT_SOCK:
                case DT_WHT:
                    // Known types that should be ignored.
                    type = Entry::Type::UNKNOWN;
                    break;
                case DT_DIR:
                    type = Entry::Type::DIR;
                    break;
                case DT_REG:
                    type = Entry::Type::REG_FILE;
                    break;

                case DT_LNK:
                // DT_UNKNOWN should also be resolved by stat again to against inaccurate
                //   meta info from file system, eg. expect DT_LNK, return DT_UNKNOWN.
                // Ref: https://aone.alibaba-inc.com/issue/36979148.
                case DT_UNKNOWN:
                // All known types have already been enumerated at above, stat for unknown
                //   new types for better compatibility.
                default: {
                    type = Entry::Type::UNKNOWN;
                    isSymbolic = (DT_LNK == ent->d_type);
                    if (!resolveWithStat) {
                        break;
                    }

                    // Try to get target type by stat, if failed, return UNKNONW to remind
                    // caller that maybe the symbolic is invalid.
                    auto fullPath = PathJoin(mDirPath, fileName);
                    struct stat fileStat;
                    if (stat(fullPath.c_str(), &fileStat) != 0) {
                        LOG_WARNING(sLogger,
                                    ("Get file info fail",
                                     fullPath)("errno", errno)("strerror", strerror(errno))("d_type", ent->d_type));
                        break;
                    }
                    if (S_ISDIR(fileStat.st_mode)) {
                        type = Entry::Type::DIR;
                    } else if (S_ISREG(fileStat.st_mode)) {
                        type = Entry::Type::REG_FILE;
                    }
                    break;
                }
            };

            return Entry(fileName, type, isSymbolic);
        }
#elif defined(_MSC_VER)
        if (mCachedEntry) {
            Entry entry = mCachedEntry;
            mCachedEntry = Entry();
            return entry;
        }

        WIN32_FIND_DATA ffd;
        while (true) {
            if (0 == FindNextFile(mFind, &ffd)) {
                auto err = GetLastError();
                if (err != ERROR_NO_MORE_FILES) {
                    LOG_WARNING(sLogger, ("Unexpected error when call FindNextFile", err)("dir path", mDirPath));
                }
                return Entry();
            }
            auto entry = ConstructEntry(ffd);
            if (entry)
                return entry;
        }
#endif
    }

    void Dir::Close() {
        if (!IsOpened())
            return;

#if defined(__linux__)
        if (closedir(mDir) != 0) {
            LOG_WARNING(sLogger, ("Close dir failed", mDirPath)("errno", errno));
            return;
        }
        mDir = nullptr;
#elif defined(_MSC_VER)
        if (!FindClose(mFind)) {
            LOG_WARNING(sLogger, ("Close dir failed", mDirPath)("errno", GetLastError()));
            return;
        }
        mFind = INVALID_HANDLE_VALUE;
#endif
    }

    PathStat::PathStat() {}

    PathStat::~PathStat() {}

    bool PathStat::stat(const std::string& path, PathStat& ps) {
        ps.mPath = path;
#if defined(__linux__)
        return (0 == ::stat(path.c_str(), &(ps.mRawStat)));
#elif defined(_MSC_VER)
        // For backward performance compatibility, check the path only
        //   when the flag is enabled by user configuration.
        if (!BOOL_FLAG(enable_root_path_collection) || path.back() != ':') {
            return 0 == ::_stat64(path.c_str(), &(ps.mRawStat));
        }
        // _stat64("D:") returns non-zero.
        return 0 == ::_stat64((path + PATH_SEPARATOR).c_str(), &(ps.mRawStat));
#endif
    }

    bool PathStat::IsDir() const {
#if defined(__linux__)
        return S_ISDIR(mRawStat.st_mode);
#elif defined(_MSC_VER)
        return mRawStat.st_mode & S_IFDIR;
#endif
    }

    bool PathStat::IsRegFile() const {
#if defined(__linux__)
        return S_ISREG(mRawStat.st_mode);
#elif defined(_MSC_VER)
        return mRawStat.st_mode & S_IFREG;
#endif
    }

    bool PathStat::lstat(const std::string& path, PathStat& ps) {
        ps.mPath = path;
#if defined(__linux__)
        return (0 == ::lstat(path.c_str(), &(ps.mRawStat)));
#elif defined(_MSC_VER)
        return (0 == ::_stat64(path.c_str(), &(ps.mRawStat)));
#endif
    }

    bool PathStat::IsLink() const {
#if defined(__linux__)
        return S_ISLNK(mRawStat.st_mode);
#elif defined(_MSC_VER)
        return false; // Windows shortcut is not symbolic link.
#endif
    }

    bool PathStat::fstat(FILE* file, PathStat& ps, bool resolvePath) {
#if defined(__linux__)
        return (0 == ::fstat(fileno(file), &(ps.mRawStat)));
#elif defined(_MSC_VER)
        return fstat(_fileno(file), ps, resolvePath);
#endif
    }

    bool PathStat::fstat(int fd, PathStat& ps, bool resolvePath) {
#if defined(__linux__)
        return (0 == ::fstat(fd, &(ps.mRawStat)));
#elif defined(_MSC_VER)
        auto fstatRet = ::_fstat64(fd, &(ps.mRawStat));
        if (fstatRet != 0)
            return false;
        if (!resolvePath)
            return true;

        ps.mPath = GetFdPath(fd);
        return !ps.mPath.empty();
#endif
    }

#if defined(_MSC_VER)
    // FILETIME2Time converts ft to time_t.
    // @return second part.
    static int64_t FILETIME2Time(const FILETIME ft, int64_t* nsec = nullptr) {
        ULARGE_INTEGER ui;
        ui.LowPart = ft.dwLowDateTime;
        ui.HighPart = ft.dwHighDateTime;
        int64_t sec = static_cast<int64_t>(ui.QuadPart / 10000000 - 11644473600);
        if (nsec != nullptr) {
            *nsec = static_cast<int64_t>(ui.QuadPart % 10000000) * 100;
        }
        return sec;
    }

    bool PathStat::fstat(HANDLE hFile, PathStat& ps, bool resolvePath) {
        // st_mtime.
        FILETIME mtim;
        if (FALSE == GetFileTime(hFile, NULL, NULL, &mtim)) {
            return false;
        }
        ps.mRawStat.st_mtime = static_cast<time_t>(FILETIME2Time(mtim));
        // st_size.
        LARGE_INTEGER liSize;
        if (FALSE == GetFileSizeEx(hFile, &liSize)) {
            return false;
        }
        ps.mRawStat.st_size = liSize.QuadPart;
        if (!resolvePath) {
            return true;
        }

        // ps.mPath.
        char filePath[MAX_PATH + 1];
        auto ret = GetFinalPathNameByHandle(hFile, filePath, MAX_PATH + 1, VOLUME_NAME_DOS);
        if (ret > MAX_PATH || ret <= 0) {
            return false;
        }
        if (0 == memcmp(filePath, "\\\\?\\", 4)) {
            ps.mPath.assign(filePath + 4);
        } else {
            ps.mPath = filePath;
        }
        return true;
    }
#endif

    time_t PathStat::GetMtime() const { return mRawStat.st_mtime; }

    void PathStat::GetLastWriteTime(int64_t& sec, int64_t& nsec) const {
#if defined(__linux__)
        sec = mRawStat.st_mtim.tv_sec;
        nsec = mRawStat.st_mtim.tv_nsec;
#elif defined(_MSC_VER)
        HANDLE hFile = CreateFile(mPath.c_str(),
                                  GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL,
                                  OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS,
                                  NULL);
        if (INVALID_HANDLE_VALUE == hFile)
            return;
        FILETIME mtim;
        auto ret = GetFileTime(hFile, NULL, NULL, &mtim);
        CloseHandle(hFile);
        if (!ret)
            return;
        sec = FILETIME2Time(mtim, &nsec);
#endif
    }

    DevInode PathStat::GetDevInode() const {
#if defined(__linux__)
        return DevInode(mRawStat.st_dev, mRawStat.st_ino);
#elif defined(_MSC_VER)
        return GetFileDevInode(mPath);
#endif
    }

    int64_t PathStat::GetFileSize() const { return mRawStat.st_size; }

} // namespace fsutil

} // namespace logtail
