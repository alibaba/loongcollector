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

#include "CheckPointManager.h"
#include <string>
#include <fstream>
#include <thread>
#include <fcntl.h>
#include "profiler/LogtailAlarm.h"
#include "app_config/AppConfig.h"
#include "config_manager/ConfigManager.h"
#include "common/Flags.h"
#include "common/HashUtil.h"
#include "common/StringTools.h"
#include "common/FileSystemUtil.h"
#include "logger/Logger.h"

using namespace std;
#if defined(__linux__)
DEFINE_FLAG_STRING(check_point_filename, "", "/tmp/logtail_check_point");
#elif defined(_MSC_VER)
DEFINE_FLAG_STRING(check_point_filename, "", "C:\\LogtailData\\logtail_check_point");
#endif
DEFINE_FLAG_INT32(file_check_point_time_out, "seconds", 300);
DEFINE_FLAG_INT32(mem_check_point_time_out, "seconds", 7200);
DEFINE_FLAG_INT32(check_point_check_interval, "default 15 min", 14 * 60);
DEFINE_FLAG_INT32(check_point_version, "now check point version xx.xx.xx such as 0.1.0 is 100", 200);
DEFINE_FLAG_INT32(check_point_dump_interval, "default 15 min", 15 * 60);
DEFINE_FLAG_INT32(check_point_max_count, "max check point count", 100000);
DEFINE_FLAG_INT32(checkpoint_find_max_file_count, "", 1000);

namespace logtail {

bool CheckPointManager::CheckVersion() {
    return (mLoadVersion == NO_CHECKPOINT_VERSION) || (mLoadVersion / 10000 == INT32_FLAG(check_point_version) / 10000);
}

void CheckPointManager::AddCheckPoint(CheckPoint* checkPointPtr) {
    LOG_DEBUG(sLogger,
              ("add check point ", checkPointPtr->mDevInode.inode)(
                  checkPointPtr->mFileName, checkPointPtr->mRealFileName)("offset", checkPointPtr->mOffset)(
                  "sig", checkPointPtr->mSignatureHash)("config", checkPointPtr->mConfigName));
    DevInodeCheckPointHashMap::iterator it
        = mDevInodeCheckPointPtrMap.find(CheckPointKey(checkPointPtr->mDevInode, checkPointPtr->mConfigName));
    if (it != mDevInodeCheckPointPtrMap.end())
        mDevInodeCheckPointPtrMap.erase(it);
    mDevInodeCheckPointPtrMap.insert(std::make_pair<CheckPointKey, CheckPointPtr>(
        CheckPointKey(checkPointPtr->mDevInode, checkPointPtr->mConfigName), CheckPointPtr(checkPointPtr)));
}

void CheckPointManager::DeleteCheckPoint(DevInode devInode, const std::string& configName) {
    DevInodeCheckPointHashMap::iterator it = mDevInodeCheckPointPtrMap.find(CheckPointKey(devInode, configName));
    if (it != mDevInodeCheckPointPtrMap.end())
        mDevInodeCheckPointPtrMap.erase(it);
}

bool CheckPointManager::GetCheckPoint(DevInode devInode, const std::string& configName, CheckPointPtr& checkPointPtr) {
    DevInodeCheckPointHashMap::iterator it = mDevInodeCheckPointPtrMap.find(CheckPointKey(devInode, configName));
    if (it != mDevInodeCheckPointPtrMap.end()) {
        checkPointPtr = it->second;
        return true;
    }
    return false;
}

void CheckPointManager::DeleteDirCheckPoint(const std::string& filename) {
    std::unordered_map<std::string, DirCheckPointPtr>::iterator it = mDirNameMap.find(filename);
    if (it != mDirNameMap.end())
        mDirNameMap.erase(it);
}

bool CheckPointManager::GetDirCheckPoint(const std::string& dirname, DirCheckPointPtr& dirCheckPointPtr) {
    std::unordered_map<std::string, DirCheckPointPtr>::iterator it = mDirNameMap.find(dirname);
    if (it != mDirNameMap.end()) {
        dirCheckPointPtr = it->second;
        return true;
    }
    return false;
}

bool CheckPointManager::NeedDump(int32_t curTime) {
    // random 60 second to protect burst
    // remember "+1", to avoid mod 0 which will cause SIGFPE
    return curTime - mLastDumpTime
        > rand() % (INT32_FLAG(check_point_dump_interval) / 15 + 1) + INT32_FLAG(check_point_dump_interval);
}

void CheckPointManager::ResetLastDumpTime() {
    mLastDumpTime = (int32_t)time(NULL);
}

CheckPointManager::DevInodeCheckPointHashMap& CheckPointManager::GetAllFileCheckPoint() {
    return mDevInodeCheckPointPtrMap;
}


void CheckPointManager::AddDirCheckPoint(const string& dirname) {
    if (dirname.size() == 0)
        return;
    size_t last = dirname.size() - 1;
    if (dirname[last] == PATH_SEPARATOR[0])
        last--;
    last = dirname.rfind(PATH_SEPARATOR, last);
    string parent = dirname.substr(0, last);
    std::unordered_map<std::string, DirCheckPointPtr>::iterator it = mDirNameMap.find(parent);
    DirCheckPoint* ptr = NULL;
    if (it == mDirNameMap.end()) {
        ptr = new DirCheckPoint(parent);
        mDirNameMap.insert(make_pair(parent, DirCheckPointPtr(ptr)));
    } else
        ptr = it->second.get();
    ptr->mSubDir.insert(dirname);
}
void CheckPointManager::LoadCheckPoint() {
    Json::Value root;
    ParseConfResult cptRes = ParseConfig(AppConfig::GetInstance()->GetCheckPointFilePath(), root);
    // if new checkpoint file not exist, check old checkpoint file.
    if (cptRes == CONFIG_NOT_EXIST
        && AppConfig::GetInstance()->GetCheckPointFilePath() != STRING_FLAG(check_point_filename)) {
        cptRes = ParseConfig(STRING_FLAG(check_point_filename), root);
    }
    if (cptRes != CONFIG_OK) {
        if (cptRes == CONFIG_NOT_EXIST)
            LOG_INFO(sLogger, ("no check point file to load", AppConfig::GetInstance()->GetCheckPointFilePath()));
        else if (cptRes == CONFIG_INVALID_FORMAT) {
            LOG_ERROR(sLogger,
                      ("load check point file fail, file content is not valid json",
                       AppConfig::GetInstance()->GetCheckPointFilePath()));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "content of check point file is not valid json");
        }
        return;
    }
    if (root.isMember("version")) {
        mLoadVersion = root["version"].asUInt();
    } else {
        mLoadVersion = NO_CHECKPOINT_VERSION;
    }

    LoadDirCheckPoint(root);
    LoadFileCheckPoint(root);
    LOG_INFO(sLogger,
             ("load checkpoint, version", mLoadVersion)("file check point", mDevInodeCheckPointPtrMap.size())(
                 "dir check point", mDirNameMap.size()));
}

void CheckPointManager::LoadDirCheckPoint(const Json::Value& root) {
    if (root.isMember("dir_check_point") == false)
        return;
    const Json::Value::Members& dirNames = root["dir_check_point"].getMemberNames();
    for (size_t index = 0; index < dirNames.size(); ++index) {
        const string& dirname = dirNames[index];
        DirCheckPoint* dir = new DirCheckPoint(dirname);
        const Json::Value& dirMeta = root["dir_check_point"][dirname];
        try {
            int32_t updateTime = dirMeta["update_time"].asInt();
            if (updateTime >= (time(NULL) - INT32_FLAG(file_check_point_time_out))) {
                for (unsigned int i = 0; i < dirMeta["sub_dir"].size(); ++i) {
                    dir->mSubDir.insert(dirMeta["sub_dir"][i].asString());
                }
                mDirNameMap.insert(make_pair(dirname, DirCheckPointPtr(dir)));
            } else {
                LOG_INFO(sLogger, ("load timeout dir check point, ignore", dirname)(ToString(updateTime), time(NULL)));
            }
        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("failed to parse dir checkpoint", e.what()));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                                   "failed to parse dir checkpoint:" + string(e.what()));
        } catch (...) {
            LOG_ERROR(sLogger, ("failed to parse dir checkpoint", "unknown exception"));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                                   "failed to parse dir checkpoint, unkonw exception");
        }
    }
}
void CheckPointManager::LoadFileCheckPoint(const Json::Value& root) {
    if (root.isMember("check_point") == false)
        return;
    const Json::Value::Members& fileKeyNames = root["check_point"].getMemberNames();
    mReaderCount = fileKeyNames.size();
    for (size_t index = 0; index < fileKeyNames.size(); ++index) {
        const string& fileKeyName = fileKeyNames[index];
        const Json::Value& meta = root["check_point"][fileKeyName];
        int32_t update_time = meta["update_time"].asInt();
        // use inode comparison instead
        try {
            int64_t offset = StringTo<int64_t>(meta["offset"].asString());
            DevInode devInode;
            uint32_t sigSize = 0;
            uint64_t sigHash = 0;
            string filePath = meta["file_name"].asString();
            string realFilePath;
            int32_t fileOpenFlag = 0; // default, we close file ptr
            if (meta.isMember("real_file_name")) {
                realFilePath = meta["real_file_name"].asString();
            }
            if (meta.isMember("inode")) {
                devInode.inode = meta["inode"].asUInt64();
            } else {
                devInode = GetFileDevInode(filePath);
            }
            if (meta.isMember("dev")) {
                devInode.dev = meta["dev"].asUInt64();
            } else if (devInode.dev == NO_BLOCK_DEV) {
                DevInode tmpDevInode = GetFileDevInode(filePath);
                if (devInode.inode != NO_BLOCK_INODE) {
                    devInode.dev = tmpDevInode.dev;
                } else {
                    devInode = tmpDevInode;
                }
            }
            if (meta.isMember("sig")) {
                string sig = meta["sig"].asString();
                SignatureToHash(sig, sigHash, sigSize);
            }
            if (meta.isMember("sig_hash")) {
                sigHash = meta["sig_hash"].asUInt64();
            }
            if (meta.isMember("sig_size")) {
                sigSize = meta["sig_size"].asUInt64();
            }
            if (meta.isMember("file_open")) {
                fileOpenFlag = meta["file_open"].asInt();
            }

            // can not get file's dev inode
            if (!devInode.IsValid()) {
                LOG_WARNING(sLogger, ("can not find check point dev inode, discard it", filePath));
                continue;
            }

            string configName;
            if (meta.isMember("config_name") && meta["config_name"].isString()) {
                // No need to check if the config still matches the file here.
                configName = meta["config_name"].asString();
                CheckPoint* ptr = new CheckPoint(
                    filePath, offset, sigSize, sigHash, devInode, configName, realFilePath, fileOpenFlag);
                ptr->mLastUpdateTime = update_time;
                AddCheckPoint(ptr);
            } else {
                // find config
                size_t lastSeparator = filePath.find_last_of(PATH_SEPARATOR[0]);
                if (lastSeparator == std::string::npos || lastSeparator == (size_t)0
                    || lastSeparator == filePath.size() - 1) {
                    continue;
                }
                string path = filePath.substr(0, lastSeparator);
                string fileName = filePath.substr(lastSeparator + 1, filePath.size() - lastSeparator - 1);
                std::vector<Config*> allConfig;
                if (AppConfig::GetInstance()->IsAcceptMultiConfig()) {
                    ConfigManager::GetInstance()->FindAllMatch(allConfig, path, fileName);
                } else {
                    ConfigManager::GetInstance()->FindMatchWithForceFlag(allConfig, path, fileName);
                }
                if (allConfig.empty()) {
                    LOG_WARNING(sLogger, ("can not find config for checkpoint", filePath));
                    continue;
                }
                for (size_t i = 0; i < allConfig.size(); ++i) {
                    CheckPoint* ptr = new CheckPoint(filePath,
                                                     offset,
                                                     sigSize,
                                                     sigHash,
                                                     devInode,
                                                     allConfig[i]->mConfigName,
                                                     realFilePath,
                                                     fileOpenFlag);
                    ptr->mLastUpdateTime = update_time;
                    AddCheckPoint(ptr);
                }
            }
        } catch (const exception& e) {
            LOG_ERROR(sLogger, ("failed to parse file checkpoint", e.what()));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                                   "failed to parse file checkpoint:" + string(e.what()));
        } catch (...) {
            LOG_ERROR(sLogger, ("failed to parse file checkpoint", "unknown exception"));
            LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                                   "failed to parse file checkpoint, unkonw exception");
        }
    }
}
bool CheckPointManager::DumpCheckPointToLocal() {
    mLastDumpTime = time(NULL);
    string checkPointFile = AppConfig::GetInstance()->GetCheckPointFilePath();
    string checkPointTempFile = checkPointFile + ".bak";

    if (!Mkdirs(ParentPath(checkPointFile))) {
        LOG_ERROR(sLogger, ("open check point file dir error", checkPointFile));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "open check point file dir failed");
        return false;
    }

    Json::Value root;
    mReaderCount = mDevInodeCheckPointPtrMap.size();
    if (mDevInodeCheckPointPtrMap.size() <= (size_t)INT32_FLAG(check_point_max_count)) {
        CheckPointManager::DevInodeCheckPointHashMap::iterator it;
        for (it = mDevInodeCheckPointPtrMap.begin(); it != mDevInodeCheckPointPtrMap.end(); ++it) {
            CheckPoint* checkPointPtr = it->second.get();
            Json::Value leaf;
            leaf["file_name"] = Json::Value(checkPointPtr->mFileName);
            leaf["real_file_name"] = Json::Value(checkPointPtr->mRealFileName);
            leaf["offset"] = Json::Value(ToString(checkPointPtr->mOffset));
            leaf["sig_size"] = Json::Value(Json::UInt(checkPointPtr->mSignatureSize));
            leaf["sig_hash"] = Json::Value(Json::UInt64(checkPointPtr->mSignatureHash));
            leaf["update_time"] = Json::Value(checkPointPtr->mLastUpdateTime);
            leaf["inode"] = Json::Value(Json::UInt64(checkPointPtr->mDevInode.inode));
            leaf["dev"] = Json::Value(Json::UInt64(checkPointPtr->mDevInode.dev));
            leaf["file_open"] = Json::Value(checkPointPtr->mFileOpenFlag);
            leaf["config_name"] = Json::Value(checkPointPtr->mConfigName);
            // forward compatible
            leaf["sig"] = Json::Value(string(""));
            // use filename + dev + inode + configName to prevent same filename conflict
            root[checkPointPtr->mFileName + "*" + ToString(checkPointPtr->mDevInode.dev) + "*"
                 + ToString(checkPointPtr->mDevInode.inode) + "*" + checkPointPtr->mConfigName]
                = leaf;
        }
    } else {
        vector<CheckPoint*> sortedCheckPointVec;
        CheckPointManager::DevInodeCheckPointHashMap::iterator it;
        for (it = mDevInodeCheckPointPtrMap.begin(); it != mDevInodeCheckPointPtrMap.end(); ++it) {
            sortedCheckPointVec.push_back(it->second.get());
        }
        sort(sortedCheckPointVec.begin(), sortedCheckPointVec.end(), CheckPointManager::CheckPointCmpByUpdateTime);
        for (int32_t i = 0; i < INT32_FLAG(check_point_max_count); ++i) {
            CheckPoint* checkPointPtr = sortedCheckPointVec[i];
            Json::Value leaf;
            leaf["file_name"] = Json::Value(checkPointPtr->mFileName);
            leaf["real_file_name"] = Json::Value(checkPointPtr->mRealFileName);
            leaf["offset"] = Json::Value(ToString(checkPointPtr->mOffset));
            leaf["sig_size"] = Json::Value(Json::UInt(checkPointPtr->mSignatureSize));
            leaf["sig_hash"] = Json::Value(Json::UInt64(checkPointPtr->mSignatureHash));
            leaf["update_time"] = Json::Value(checkPointPtr->mLastUpdateTime);
            leaf["inode"] = Json::Value(Json::UInt64(checkPointPtr->mDevInode.inode));
            leaf["dev"] = Json::Value(Json::UInt64(checkPointPtr->mDevInode.dev));
            leaf["file_open"] = Json::Value(checkPointPtr->mFileOpenFlag);
            leaf["config_name"] = Json::Value(checkPointPtr->mConfigName);
            // forward compatible
            leaf["sig"] = Json::Value(string(""));
            // use filename + dev + inode + configName to prevent same filename conflict
            root[checkPointPtr->mFileName + "*" + ToString(checkPointPtr->mDevInode.dev) + "*"
                 + ToString(checkPointPtr->mDevInode.inode) + "*" + checkPointPtr->mConfigName]
                = leaf;
        }
        LOG_WARNING(sLogger, ("Too many check point", mDevInodeCheckPointPtrMap.size()));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                               "Too many check point:" + ToString(mDevInodeCheckPointPtrMap.size()));
    }


    Json::Value dirJson;
    for (unordered_map<string, DirCheckPointPtr>::iterator it = mDirNameMap.begin(); it != mDirNameMap.end(); ++it) {
        DirCheckPoint* ptr = it->second.get();
        Json::Value value;
        for (set<string>::iterator itr = ptr->mSubDir.begin(); itr != ptr->mSubDir.end(); ++itr) {
            value.append(*itr);
        }

        dirJson[it->first] = Json::Value();
        dirJson[it->first]["update_time"] = Json::Value(ptr->mUpdateTime);
        dirJson[it->first]["sub_dir"] = value;
    }

    std::ofstream fout(checkPointTempFile.c_str());
    if (!fout) {
        LOG_ERROR(sLogger, ("open check point file error", checkPointFile));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "open check point file failed");
        return false;
    }
    Chmod(checkPointTempFile.c_str(), 0644);
    Json::Value result;
    result["check_point"] = root;
    result["dir_check_point"] = dirJson;
    result["version"] = Json::Value(Json::UInt(INT32_FLAG(check_point_version)));
    fout << result.toStyledString();
    if (!fout.good()) {
        LOG_ERROR(sLogger, ("dump check point to file failed", checkPointFile));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM, "dump check point to file failed");
        fout.close();
        return false;
    }
    fout.close();
#if defined(_MSC_VER)
    // The rename on Windows will fail if the destination is existing.
    remove(checkPointFile.c_str());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
    if (rename(checkPointTempFile.c_str(), checkPointFile.c_str()) == -1) {
        LOG_ERROR(sLogger, ("rename check point file fail, errno", errno));
        LogtailAlarm::GetInstance()->SendAlarm(CHECKPOINT_ALARM,
                                               std::string("rename check point file fail, errno ") + ToString(errno));
        return false;
    }
    LOG_DEBUG(sLogger,
              ("dump checkpoint, version", INT32_FLAG(check_point_version))(
                  "file check point", mDevInodeCheckPointPtrMap.size())("dir check point", mDirNameMap.size()));

    return true;
}

int32_t CheckPointManager::GetReaderCount() {
    return mReaderCount;
}

void CheckPointManager::CheckTimeoutCheckPoint() {
    // do not need to clear file checkpoint, we will clear all checkpoint after DumpCheckPointToLocal
    /*
    if((time(NULL) - mLastCheckTime) > INT32_FLAG(check_point_check_interval))
    {


        CheckPointManager::DevInodeCheckPointHashMap::iterator it ;
        int now = time(NULL);
        vector<CheckPointKey> deleteVec;
        for(it = mDevInodeCheckPointPtrMap.begin();it != mDevInodeCheckPointPtrMap.end(); ++it)
        {
            if ((now - it->second.get()->mLastUpdateTime) > INT32_FLAG(mem_check_point_time_out))
                deleteVec.push_back(it->first);
        }

        for (size_t i = 0; i > deleteVec.size(); ++i)
        {
            mDevInodeCheckPointPtrMap.erase(deleteVec[i]);
        }

        std::unordered_map<std::string,DirCheckPointPtr>::iterator dirIt;
        for(dirIt = mDirNameMap.begin(); dirIt != mDirNameMap.end();)
        {
            if((now - dirIt -> second.get() -> mUpdateTime) > INT32_FLAG(mem_check_point_time_out))
                dirIt = mDirNameMap.erase(dirIt);
            else
                ++dirIt;
        }
        mLastCheckTime = time(NULL);
    }

    */
}

void CheckPointManager::RemoveAllCheckPoint() {
    mDirNameMap.clear();
    mDevInodeCheckPointPtrMap.clear();
}

boost::optional<std::string> SearchFilePathByDevInodeInDirectory(const std::string& baseDirPath,
                                                                 const uint16_t searchDepth,
                                                                 const DevInode& devInode,
                                                                 std::map<DevInode, SplitedFilePath>* cache) {
#define METHOD_LOG_PATTERN ("method", "SearchFilePathByDevInodeInDirectory")("inode", devInode.inode)
    std::deque<std::pair<std::string /* path */, uint16_t /* depth */>> dirs;
    dirs.emplace_back(std::make_pair(baseDirPath, 0));

    int32_t findCount = 0;
    boost::optional<std::string> result;
    while (!dirs.empty()) {
        const auto dirPath = dirs.front().first;
        const auto depth = dirs.front().second;
        dirs.pop_front();

        fsutil::Dir dir(dirPath);
        if (!dir.Open()) {
            LogtailAlarm::GetInstance()->SendAlarm(
                CHECKPOINT_ALARM, string("Failed to open dir : ") + dirPath + ";\terrno : " + ToString(GetErrno()));
            LOG_ERROR(sLogger, METHOD_LOG_PATTERN("message", "open dir error")("dir", dirPath));
            continue;
        }
        LOG_INFO(sLogger, METHOD_LOG_PATTERN("dir", dirPath));

        fsutil::Entry ent;
        DevInode entDevInode;
        std::string entName;
        const std::string entPathPrefix = PathJoin(dirPath, "");
        std::string entPath = entPathPrefix;
        while (ent = dir.ReadNext(false)) {
            if (findCount++ > INT32_FLAG(checkpoint_find_max_file_count)) {
                LOG_WARNING(sLogger,
                            METHOD_LOG_PATTERN("exceed max search count", INT32_FLAG(checkpoint_find_max_file_count)));
                return result;
            }

            entName = ent.Name();
            entPath.resize(entPathPrefix.length());
            entPath.append(entName);
            fsutil::PathStat buf;
            if (!fsutil::PathStat::stat(entPath, buf)) {
                LOG_ERROR(sLogger, METHOD_LOG_PATTERN("get file info error: ", entPath));
                continue;
            }
            if (!buf.IsRegFile()) {
                if (buf.IsDir() && depth < searchDepth) {
                    dirs.emplace_back(std::make_pair(entPath, depth + 1));
                }
                continue;
            }
            entDevInode = buf.GetDevInode();
            if (cache) {
                cache->insert(std::make_pair(entDevInode, SplitedFilePath(baseDirPath, entName)));
            }
            if (devInode == entDevInode) {
                result = entPath;
                return result;
            }
        }
    }
    return result;
#undef METHOD_LOG_PATTERN
}

#ifdef APSARA_UNIT_TEST_MAIN
void CheckPointManager::RemoveLocalCheckPoint() {
    std::string checkPointFile = AppConfig::GetInstance()->GetCheckPointFilePath();
    if (remove(checkPointFile.c_str()) == -1) {
    }
}

void CheckPointManager::PrintStatus() {
    printf("=======================================\n");

    for (DevInodeCheckPointHashMap::iterator it = mDevInodeCheckPointPtrMap.begin();
         it != mDevInodeCheckPointPtrMap.end();
         ++it) {
        printf("File %s\n", it->second.get()->mFileName.c_str());
    }
    printf("\n");
    for (std::unordered_map<std::string, DirCheckPointPtr>::iterator it = mDirNameMap.begin(); it != mDirNameMap.end();
         ++it) {
        printf("Dir %s\n", it->first.c_str());
        DirCheckPoint* ptr = it->second.get();
        for (std::set<std::string>::iterator itr = ptr->mSubDir.begin(); itr != ptr->mSubDir.end(); ++itr) {
            printf(" --------%s\n", itr->c_str());
        }
    }
    printf("=======================================\n");
}
#endif

} // namespace logtail
