/*
 * Copyright 2023 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/FileSystemUtil.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "file_server/ContainerInfo.h"

namespace logtail {

// Base path information structure
// Encapsulates all information for a single file discovery path configuration
struct BasePathInfo {
    std::string basePath; // Base directory path (e.g., "/var/log" or "/home/*/logs")
    std::string filePattern; // File name matching pattern (e.g., "*.log", "app.log")
    std::vector<std::string> wildcardPaths; // Wildcard path hierarchy (split by wildcards, e.g., ["/home", "*/logs"])
    std::vector<std::string> constWildcardPaths; // Non-wildcard parts from wildcardPaths (e.g., ["", "logs"])
    uint16_t wildcardDepth = 0; // Number of path separators in basePath (NOT wildcard count!)

    BasePathInfo() = default;
    explicit BasePathInfo(const std::string& path, const std::string& pattern = "*")
        : basePath(path), filePattern(pattern), wildcardDepth(0) {}

    // Check if path contains wildcards
    bool hasWildcard() const { return !wildcardPaths.empty(); }

    // Check if path is empty
    bool isEmpty() const { return basePath.empty(); }
};

class FileDiscoveryOptions {
public:
    bool Init(const Json::Value& config, const CollectionPipelineContext& ctx, const std::string& pluginType);

    // Get all base path information (each path has its own filePattern, wildcardPaths, etc.)
    const std::vector<BasePathInfo>& GetBasePathInfos() const { return mBasePathInfos; }
    bool IsContainerDiscoveryEnabled() const { return mEnableContainerDiscovery; }
    void SetEnableContainerDiscoveryFlag(bool flag) { mEnableContainerDiscovery = flag; }
    const std::shared_ptr<std::vector<ContainerInfo>>& GetContainerInfo() const { return mContainerInfos; }

    const std::shared_ptr<std::set<std::string>>& GetFullContainerList() const { return mFullContainerList; }
    void SetFullContainerList(const std::shared_ptr<std::set<std::string>>& fullList) { mFullContainerList = fullList; }

    void SetContainerDiscoveryOptions(ContainerDiscoveryOptions&& option) { mContainerDiscovery = std::move(option); }
    ContainerDiscoveryOptions GetContainerDiscoveryOptions() const { return mContainerDiscovery; }

    void SetContainerInfo(const std::shared_ptr<std::vector<ContainerInfo>>& info) { mContainerInfos = info; }
    void SetDeduceAndSetContainerBaseDirFunc(bool (*f)(ContainerInfo&,
                                                       const CollectionPipelineContext*,
                                                       const FileDiscoveryOptions*)) {
        mDeduceAndSetContainerBaseDirFunc = f;
    }

    // Check if filename matches the specific pathInfo's filePattern
    bool IsFilenameMatched(const std::string& filename, const BasePathInfo& pathInfo) const;
    bool IsFilenameInBlacklist(const std::string& filename) const;
    bool IsDirectoryInBlacklist(const std::string& dir) const;
    bool IsFilepathInBlacklist(const std::string& filepath) const;

    // Check if path/name matches this config
    bool IsMatch(const std::string& path, const std::string& name) const;

    // Check if path/name matches this config, and return the depth of matched path
    // @param outMatchedPathDepth: (optional) output the depth of matched basePath (number of path separators)
    // @return true if matched, false otherwise
    bool IsMatch(const std::string& path, const std::string& name, int32_t* outMatchedPathDepth) const;

    bool IsTimeout(const std::string& path) const;
    bool WithinMaxDepth(const std::string& path) const;

    bool UpdateRawContainerInfo(const std::shared_ptr<RawContainerInfo>& rawContainerInfo,
                                const CollectionPipelineContext*,
                                const std::string& basePathInCheckpoint = "");
    bool DeleteRawContainerInfo(const std::string& containerID);

    ContainerInfo* GetContainerPathByLogPath(const std::string& logPath) const;

    // 根据实际文件路径，找到它对应的 mRealBaseDirs 索引
    size_t GetRealBaseDirIndex(const ContainerInfo* containerInfo, const std::string& logPath) const;

    // 过渡使用
    bool IsTailingAllMatchedFiles() const { return mTailingAllMatchedFiles; }
    void SetTailingAllMatchedFiles(bool flag) { mTailingAllMatchedFiles = flag; }

    uint32_t GetLastContainerUpdateTime() const { return mLastContainerUpdateTime; }
    void SetLastContainerUpdateTime(uint32_t time) { mLastContainerUpdateTime = time; }


    std::vector<std::string> mFilePaths;
    int32_t mMaxDirSearchDepth = 0;
    int32_t mPreservedDirDepth = -1;
    std::vector<std::string> mExcludeFilePaths;
    std::vector<std::string> mExcludeFiles;
    std::vector<std::string> mExcludeDirs;
    bool mAllowingCollectingFilesInRootDir = false;
    bool mAllowingIncludedByMultiConfigs = false;

private:
    void ParseWildcardPath(BasePathInfo& pathInfo);
    std::pair<std::string, std::string> GetDirAndFileNameFromPath(const std::string& filePath);
    bool IsObjectInBlacklist(const std::string& path, const std::string& name) const;
    bool IsWildcardPathMatch(const std::string& path, const std::string& name, const BasePathInfo& pathInfo) const;

    // Multiple base path information (including file patterns)
    std::vector<BasePathInfo> mBasePathInfos;

    // Blacklist control.
    bool mHasBlacklist = false;
    // Absolute path of directories to filter, such as /app/log. It will filter
    // subdirectories as well.
    std::vector<std::string> mDirPathBlacklist;
    // Wildcard (*/?) is included, use fnmatch with FNM_PATHNAME to filter, which
    // will also filter subdirectories. For example, /app/* filters subdirectory
    // /app/log but keep /app/text.log, because /app does not match /app/*. And
    // because /app/log is filtered, so any changes under it will be ignored, so
    // both /app/log/sub and /app/log/text.log will be blacklisted.
    std::vector<std::string> mWildcardDirPathBlacklist;
    // Multiple level wildcard (**) is included, use fnmatch with 0 as flags to filter,
    // which will blacklist /path/a/b with pattern /path/**.
    std::vector<std::string> mMLWildcardDirPathBlacklist;
    // Absolute path of files to filter, */? is supported, such as /app/log/100*.log.
    std::vector<std::string> mFilePathBlacklist;
    // Multiple level wildcard (**) is included.
    std::vector<std::string> mMLFilePathBlacklist;
    // File name only, */? is supported too, such as 100*.log. It is similar to
    // mFilePattern, but works in reversed way.
    std::vector<std::string> mFileNameBlacklist;

    bool mEnableContainerDiscovery = false;

    std::shared_ptr<std::set<std::string>> mFullContainerList = std::make_shared<std::set<std::string>>();
    std::shared_ptr<std::vector<ContainerInfo>> mContainerInfos; // must not be null if container discovery is enabled
    ContainerDiscoveryOptions mContainerDiscovery;
    bool (*mDeduceAndSetContainerBaseDirFunc)(ContainerInfo& containerInfo,
                                              const CollectionPipelineContext*,
                                              const FileDiscoveryOptions*)
        = nullptr;

    // 过渡使用
    bool mTailingAllMatchedFiles = false;

    uint32_t mLastContainerUpdateTime = 0;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class FileDiscoveryOptionsUnittest;
    friend class ModifyHandlerUnittest;
#endif
};

using FileDiscoveryConfig = std::pair<FileDiscoveryOptions*, const CollectionPipelineContext*>;

// 路径项结构体（用于路径级别分类和排序）
// 注意：必须在 FileDiscoveryConfig 定义之后
struct PathItem {
    std::string path; // 配置路径（basePath，用于排序）
    int32_t depth; // 路径深度
    FileDiscoveryConfig config; // 关联的配置
    const BasePathInfo* pathInfo; // 路径信息
    size_t pathInfoIndex; // pathInfo 在 mBasePathInfos 中的索引

    PathItem() = default;
    PathItem(const std::string& p, int32_t d, const FileDiscoveryConfig& c, const BasePathInfo* pi, size_t idx)
        : path(p), depth(d), config(c), pathInfo(pi), pathInfoIndex(idx) {}
};

// 计算路径深度（路径分隔符的数量）
inline int32_t CalculatePathDepth(const std::string& path) {
    return static_cast<int32_t>(std::count(path.begin(), path.end(), PATH_SEPARATOR[0]));
}

// 构建并排序路径项列表（用于路径级别的排序和处理）
// @param nameConfigMap: 所有配置映射
// @param outSortedPaths: 输出的精确路径列表（按深度排序）
// @param outWildcardPaths: 输出的通配符路径列表（不排序）
void BuildAndSortPathItems(const std::unordered_map<std::string, FileDiscoveryConfig>& nameConfigMap,
                           std::vector<PathItem>& outSortedPaths,
                           std::vector<PathItem>& outWildcardPaths);

} // namespace logtail
