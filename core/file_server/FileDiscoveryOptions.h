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
#include <utility>
#include <vector>

#include "json/json.h"

#include "collection_pipeline/CollectionPipelineContext.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "file_server/ContainerInfo.h"

namespace logtail {

class FileDiscoveryOptions {
public:
    static bool CompareByPathLength(std::pair<const FileDiscoveryOptions*, const CollectionPipelineContext*> left,
                                    std::pair<const FileDiscoveryOptions*, const CollectionPipelineContext*> right);
    static bool
    CompareByDepthAndCreateTime(std::pair<const FileDiscoveryOptions*, const CollectionPipelineContext*> left,
                                std::pair<const FileDiscoveryOptions*, const CollectionPipelineContext*> right);

    bool Init(const Json::Value& config, const CollectionPipelineContext& ctx, const std::string& pluginType);
    const std::string& GetBasePath() const { return mBasePath; }
    const std::string& GetFilePattern() const { return mFilePattern; }
    const std::vector<std::string>& GetWildcardPaths() const { return mWildcardPaths; }
    const std::vector<std::string>& GetConstWildcardPaths() const { return mConstWildcardPaths; }
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

    bool IsFilenameMatched(const std::string& filename) const;
    bool IsFilenameInBlacklist(const std::string& filename) const;
    bool IsDirectoryInBlacklist(const std::string& dir) const;
    bool IsFilepathInBlacklist(const std::string& filepath) const;
    bool IsMatch(const std::string& path, const std::string& name) const;
    bool IsTimeout(const std::string& path) const;
    bool WithinMaxDepth(const std::string& path) const;

    bool UpdateRawContainerInfo(const std::shared_ptr<RawContainerInfo>& rawContainerInfo,
                                const CollectionPipelineContext*);
    bool DeleteRawContainerInfo(const std::string& containerID);

    ContainerInfo* GetContainerPathByLogPath(const std::string& logPath) const;
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
    void ParseWildcardPath();
    std::pair<std::string, std::string> GetDirAndFileNameFromPath(const std::string& filePath);
    bool IsObjectInBlacklist(const std::string& path, const std::string& name) const;
    bool IsWildcardPathMatch(const std::string& path, const std::string& name = "") const;

    std::string mBasePath;
    std::string mFilePattern;
    std::vector<std::string> mConstWildcardPaths;
    std::vector<std::string> mWildcardPaths;
    uint16_t mWildcardDepth;

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

} // namespace logtail
