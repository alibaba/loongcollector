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

#include "plugin/input/InputStaticFile.h"

#include <fnmatch.h>

#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipeline.h"
#include "collection_pipeline/plugin/PluginRegistry.h"
#include "common/ParamExtractor.h"
#include "file_server/StaticFileServer.h"
#include "plugin/processor/inner/ProcessorSplitLogStringNative.h"
#include "plugin/processor/inner/ProcessorSplitMultilineLogStringNative.h"

using namespace std;

namespace logtail {

static bool IsValidDir(const filesystem::path& dir) {
    error_code ec;
    filesystem::file_status s = filesystem::status(dir, ec);
    if (ec) {
        LOG_WARNING(sLogger,
                    ("failed to get base dir path info",
                     "skip")("dir path", dir.string())("error code", ec.value())("error msg", ec.message()));
        return false;
    }
    if (!filesystem::exists(s)) {
        LOG_WARNING(sLogger, ("base dir path not existed", "skip")("dir path", dir.string()));
        return false;
    }
    if (!filesystem::is_directory(s)) {
        LOG_WARNING(sLogger, ("base dir path is not a directory", "skip")("dir path", dir.string()));
        return false;
    }
    return true;
}

const string InputStaticFile::sName = "input_static_file_onetime";

bool InputStaticFile::Init(const Json::Value& config, Json::Value& optionalGoPipeline) {
    string errorMsg;

    if (!mFileDiscovery.Init(config, *mContext, sName)) {
        return false;
    }

    // EnableContainerDiscovery
    if (!GetOptionalBoolParam(config, "EnableContainerDiscovery", mEnableContainerDiscovery, errorMsg)) {
        PARAM_WARNING_DEFAULT(mContext->GetLogger(),
                              mContext->GetAlarm(),
                              errorMsg,
                              false,
                              sName,
                              mContext->GetConfigName(),
                              mContext->GetProjectName(),
                              mContext->GetLogstoreName(),
                              mContext->GetRegion());
    } else if (mEnableContainerDiscovery && !AppConfig::GetInstance()->IsPurageContainerMode()) {
        PARAM_ERROR_RETURN(mContext->GetLogger(),
                           mContext->GetAlarm(),
                           "iLogtail is not in container, but container discovery is required",
                           sName,
                           mContext->GetConfigName(),
                           mContext->GetProjectName(),
                           mContext->GetLogstoreName(),
                           mContext->GetRegion());
    }
    if (mEnableContainerDiscovery) {
        if (!mContainerDiscovery.Init(config, *mContext, sName)) {
            return false;
        }
        mFileDiscovery.SetEnableContainerDiscoveryFlag(true);
    }

    if (!mFileReader.Init(config, *mContext, sName)) {
        return false;
    }
    // explicitly set here to skip realtime file checkpoint loading
    mFileReader.mTailingAllMatchedFiles = true;
    mFileReader.mInputType = FileReaderOptions::InputType::InputFile;

    // Multiline
    const char* key = "Multiline";
    const Json::Value* itr = config.find(key, key + strlen(key));
    if (itr) {
        if (!itr->isObject()) {
            PARAM_WARNING_IGNORE(mContext->GetLogger(),
                                 mContext->GetAlarm(),
                                 "param Multiline is not of type object",
                                 sName,
                                 mContext->GetConfigName(),
                                 mContext->GetProjectName(),
                                 mContext->GetLogstoreName(),
                                 mContext->GetRegion());
        } else if (!mMultiline.Init(*itr, *mContext, sName)) {
            return false;
        }
    }

    if (!mFileTag.Init(config, *mContext, sName, mEnableContainerDiscovery)) {
        return false;
    }

    return CreateInnerProcessors();
}

bool InputStaticFile::Start() {
    if (mEnableContainerDiscovery) {
        // TODO: get container info
        // mFileDiscovery.SetContainerInfo();
    }
    StaticFileServer::GetInstance()->AddInput(
        mContext->GetConfigName(), mIndex, GetFiles(), &mFileDiscovery, &mFileReader, &mMultiline, &mFileTag, mContext);
    return true;
}

bool InputStaticFile::Stop(bool isPipelineRemoving) {
    StaticFileServer::GetInstance()->RemoveInput(mContext->GetConfigName(), mIndex);
    return true;
}

vector<filesystem::path> InputStaticFile::GetFiles() const {
    vector<filesystem::path> res;
    vector<filesystem::path> baseDirs;
    if (!mEnableContainerDiscovery) {
        if (mFileDiscovery.GetWildcardPaths().empty()) {
            baseDirs.emplace_back(mFileDiscovery.GetBasePath());
        } else {
            GetValidBaseDirs(mFileDiscovery.GetBasePath(), 0, baseDirs);
            if (baseDirs.empty()) {
                LOG_WARNING(sLogger,
                            ("no files found", "base dir path invalid")("base dir", mFileDiscovery.GetBasePath())(
                                "config", mContext->GetConfigName()));
                return res;
            }
        }
        for (const auto& dir : baseDirs) {
            if (IsValidDir(dir)) {
                GetFiles(dir, mFileDiscovery.mMaxDirSearchDepth, nullptr, res);
            }
        }
        LOG_INFO(sLogger, ("total files cnt", res.size())("files", ToString(res))("config", mContext->GetConfigName()));
    } else {
        for (const auto& item : *mFileDiscovery.GetContainerInfo()) {
            baseDirs.clear();
            if (mFileDiscovery.GetWildcardPaths().empty()) {
                baseDirs.emplace_back(item.mRealBaseDir);
            } else {
                GetValidBaseDirs(item.mRealBaseDir, 0, baseDirs);
                if (baseDirs.empty()) {
                    LOG_DEBUG(sLogger,
                              ("no files found", "base dir path invalid")("container id", item.mID)(
                                  "real base dir", item.mRealBaseDir)("config", mContext->GetConfigName()));
                    return res;
                }
            }
            auto prevCnt = res.size();
            for (const auto& dir : baseDirs) {
                if (IsValidDir(dir)) {
                    GetFiles(dir, mFileDiscovery.mMaxDirSearchDepth, &item.mRealBaseDir, res);
                }
            }
            if (res.size() > prevCnt) {
                LOG_INFO(sLogger,
                         ("container files cnt", res.size() - prevCnt)("container id", item.mID)(
                             "real base dir", item.mRealBaseDir)("files", ToString(res))("config",
                                                                                         mContext->GetConfigName()));
            } else {
                LOG_DEBUG(sLogger,
                          ("no files found, container id",
                           item.mID)("real base dir", item.mRealBaseDir)("config", mContext->GetConfigName()));
            }
        }
        LOG_INFO(sLogger, ("total files cnt", res.size())("config", mContext->GetConfigName()));
    }
    return res;
}

void InputStaticFile::GetValidBaseDirs(const filesystem::path& dir,
                                       uint32_t depth,
                                       vector<filesystem::path>& filepaths) const {
    bool finish = false;
    if (depth + 2 == mFileDiscovery.GetWildcardPaths().size()) {
        finish = true;
    }

    const auto& subdir = mFileDiscovery.GetConstWildcardPaths()[depth];
    if (!subdir.empty()) {
        auto path = dir / subdir;
        error_code ec;
        filesystem::file_status s = filesystem::status(path, ec);
        if (ec || !filesystem::exists(s) || !filesystem::is_directory(s)) {
            return;
        }
        if (finish) {
            filepaths.emplace_back(path);
        } else {
            GetValidBaseDirs(path, depth + 1, filepaths);
        }
    } else {
        auto pattern = filesystem::path(mFileDiscovery.GetWildcardPaths()[depth + 1]).filename();
        error_code ec;
        for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
            const auto& path = entry.path();
            const auto& status = entry.status();
            if (filesystem::is_directory(status)
                && (fnmatch(pattern.c_str(), path.stem().c_str(), FNM_PATHNAME) == 0)) {
                if (finish) {
                    filepaths.emplace_back(path);
                } else {
                    GetValidBaseDirs(path, depth + 1, filepaths);
                }
            }
        }
    }
}

void InputStaticFile::GetFiles(const filesystem::path& dir,
                               uint32_t depth,
                               const string* containerBaseDir,
                               vector<filesystem::path>& files) const {
    error_code ec;
    for (auto const& entry : filesystem::directory_iterator(dir, ec)) {
        const auto& path = entry.path();
        auto pathStr = path.string();
        if (containerBaseDir) {
            pathStr = mFileDiscovery.GetBasePath() + pathStr.substr(containerBaseDir->size());
        }
        const auto& status = entry.status();
        if (filesystem::is_regular_file(status)) {
            const auto& filename = path.filename().string();
            if (mFileDiscovery.IsFilenameMatched(filename) && !mFileDiscovery.IsFilenameInBlacklist(filename)
                && !mFileDiscovery.IsFilepathInBlacklist(pathStr)) {
                files.emplace_back(path);
            }
        } else if (filesystem::is_directory(status)) {
            if (depth > 0 && !AppConfig::GetInstance()->IsHostPathMatchBlacklist(path.string())
                && !mFileDiscovery.IsDirectoryInBlacklist(pathStr)) {
                GetFiles(path, depth - 1, containerBaseDir, files);
            }
        }
    }
}

bool InputStaticFile::CreateInnerProcessors() {
    unique_ptr<ProcessorInstance> processor;
    {
        Json::Value detail;
        if (mContext->IsFirstProcessorJson() || mMultiline.mMode == MultilineOptions::Mode::JSON) {
            mContext->SetRequiringJsonReaderFlag(true);
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["SplitChar"] = Json::Value('\0');
        } else if (mMultiline.IsMultiline()) {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitMultilineLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
            detail["Mode"] = Json::Value("custom");
            detail["StartPattern"] = Json::Value(mMultiline.mStartPattern);
            detail["ContinuePattern"] = Json::Value(mMultiline.mContinuePattern);
            detail["EndPattern"] = Json::Value(mMultiline.mEndPattern);
            detail["IgnoringUnmatchWarning"] = Json::Value(mMultiline.mIgnoringUnmatchWarning);
            if (mMultiline.mUnmatchedContentTreatment == MultilineOptions::UnmatchedContentTreatment::DISCARD) {
                detail["UnmatchedContentTreatment"] = Json::Value("discard");
            } else if (mMultiline.mUnmatchedContentTreatment
                       == MultilineOptions::UnmatchedContentTreatment::SINGLE_LINE) {
                detail["UnmatchedContentTreatment"] = Json::Value("single_line");
            }
        } else {
            processor = PluginRegistry::GetInstance()->CreateProcessor(
                ProcessorSplitLogStringNative::sName, mContext->GetPipeline().GenNextPluginMeta(false));
        }
        detail["EnableRawContent"]
            = Json::Value(!mContext->HasNativeProcessors() && !mContext->IsExactlyOnceEnabled()
                          && !mContext->IsFlushingThroughGoPipeline() && !mFileTag.EnableLogPositionMeta());
        if (!processor->Init(detail, *mContext)) {
            // should not happen
            return false;
        }
        mInnerProcessors.emplace_back(std::move(processor));
    }
    return true;
}

} // namespace logtail
