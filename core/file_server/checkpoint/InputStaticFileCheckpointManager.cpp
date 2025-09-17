// Copyright 2025 iLogtail Authors
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

#include "file_server/checkpoint/InputStaticFileCheckpointManager.h"

#include "SelfMonitorServer.h"
#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/HashUtil.h"
#include "logger/Logger.h"
#include "monitor/AlarmManager.h"

using namespace std;

namespace logtail {

static string GetCheckpointFileName(const string& configName, size_t idx) {
    return configName + "@" + to_string(idx) + ".json";
}

InputStaticFileCheckpointManager::InputStaticFileCheckpointManager()
    : mCheckpointRootPath(filesystem::path(GetAgentDataDir()) / "input_static_file") {
    error_code ec;
    if (!filesystem::create_directories(mCheckpointRootPath, ec) && ec) {
        LOG_ERROR(sLogger,
                  ("failed to create checkpoint root path",
                   mCheckpointRootPath.string())("error code", ec.value())("error msg", ec.message()));
    }
}

bool InputStaticFileCheckpointManager::CreateCheckpoint(const string& configName,
                                                        size_t idx,
                                                        const optional<vector<filesystem::path>>& files,
                                                        uint32_t startTime,
                                                        uint32_t expireTime) {
    if (!files.has_value()) {
        InputStaticFileCheckpoint cpt;
        if (RetrieveCheckpointFromFile(configName, idx, &cpt)) {
            lock_guard<mutex> lock(mUpdateMux);
            mInputCheckpointMap.try_emplace(make_pair(configName, idx), std::move(cpt));
            LOG_INFO(sLogger, ("load checkpoint from file succeeded, config", configName)("input idx", idx));
            return true;
        } else {
            lock_guard<mutex> lock(mUpdateMux);
            auto it
                = mInputCheckpointMap
                      .try_emplace(
                          make_pair(configName, idx), configName, idx, vector<FileCheckpoint>(), startTime, expireTime)
                      .first;
            it->second.SetAbort();
            LOG_WARNING(sLogger,
                        ("failed to load checkpoint from file", "abort")("config", configName)("input idx", idx));
            if (!DumpCheckpointFile(it->second)) {
                LOG_WARNING(sLogger,
                            ("failed to dump checkpoint file on creation, config", configName)("input idx", idx));
            }
            return false;
        }
    }

    error_code ec;
    if (filesystem::remove(mCheckpointRootPath / GetCheckpointFileName(configName, idx), ec)) {
        LOG_INFO(sLogger,
                 ("config expire time checkpoint invalid, perhapes config has changed",
                  "delete obsolete checkpoint file succeeded")("config", configName)("input idx", idx));
    } else if (ec) {
        LOG_WARNING(sLogger,
                    ("config expire time checkpoint invalid, perhapes config has changed",
                     "failed to delete obsolete checkpoint file, ignore")("config", configName)("input idx", idx)(
                        "error code", ec.value())("error msg", ec.message()));
    }

    vector<FileCheckpoint> fileCpts;
    for (const auto& file : files.value()) {
        auto devInode = GetFileDevInode(file.string());
        if (!devInode.IsValid()) {
            LOG_WARNING(sLogger,
                        ("failed to get dev and inode of file", "skip")("config", configName)("input idx",
                                                                                              idx)("filepath", file));
            continue;
        }

        ifstream is(file, ios::binary);
        if (!is) {
            LOG_WARNING(sLogger,
                        ("failed to open file", "skip")("config", configName)("input idx", idx)("filepath", file));
            continue;
        }

        string signature;
        signature.resize(1024);
        is.read(&signature[0], 1024);
        if (is.bad()) {
            LOG_WARNING(sLogger,
                        ("failed to read first 1024 bytes of file",
                         "skip")("config", configName)("input idx", idx)("filepath", file));
            continue;
        }

        signature.resize(is.gcount());
        auto sigHash = static_cast<uint64_t>(HashSignatureString(signature.c_str(), signature.size()));

        fileCpts.emplace_back(file, devInode, sigHash, signature.size());
        LOG_INFO(sLogger,
                 ("create file checkpoint succeeded, config", configName)("input idx", idx)("filepath", file)(
                     "device", devInode.dev)("inode", devInode.inode)("signature hash", sigHash)("signature size",
                                                                                                 signature.size()));
    }
    LOG_INFO(sLogger,
             ("create checkpoint succeeded, config", configName)("input idx", idx)("file count", fileCpts.size()));
    {
        lock_guard<mutex> lock(mUpdateMux);
        auto it
            = mInputCheckpointMap
                  .try_emplace(make_pair(configName, idx), configName, idx, std::move(fileCpts), startTime, expireTime)
                  .first;
        if (!DumpCheckpointFile(it->second)) {
            LOG_WARNING(sLogger, ("failed to dump checkpoint file on creation, config", configName)("input idx", idx));
        }
        // auto it = mInputCheckpointMap.find(make_pair(configName, idx));
        // if (it != mInputCheckpointMap.end()) {
        //     mInputCheckpointMap.try_emplace(make_pair(configName, idx), configName, idx, move(fileCpts));
        // } else {
        //     it->second = InputStaticFileCheckpoint(configName, idx, move(fileCpts));
        //     LOG_INFO(
        //         sLogger,
        //         ("override checkpoint", "perhaps config has updated and the obsolete checkpoint has not been
        //         exported")(
        //             "config", configName)("input idx", idx));
        // }
        // auto range = mDeletedInputs.equal_range(configName);
        // for (auto it = range.first; it != range.second; ++it) {
        //     if (it->second == idx) {
        //         mDeletedInputs.erase(it);
        //         break;
        //     }
        // }
    }
    return true;
}

bool InputStaticFileCheckpointManager::DeleteCheckpoint(const string& configName, size_t idx) {
    lock_guard<mutex> lock(mUpdateMux);
    auto it = mInputCheckpointMap.find(make_pair(configName, idx));
    if (it == mInputCheckpointMap.end()) {
        // should not happen
        return false;
    }
    it->second.SetAbort();
    // mDeletedInputs.emplace(configName, idx);
    mInputCheckpointMap.erase(it);

    error_code ec;
    if (!filesystem::remove(mCheckpointRootPath / GetCheckpointFileName(configName, idx), ec)) {
        if (ec) {
            LOG_WARNING(sLogger,
                        ("failed to delete checkpoint file", "skip")("config", configName)("input idx", idx)(
                            "error code", ec.value())("error msg", ec.message()));
        } else {
            LOG_WARNING(sLogger,
                        ("failed to delete checkpoint file",
                         "skip")("config", configName)("input idx", idx)("error msg", "file not existed"));
        }
    }
    return true;
}

bool InputStaticFileCheckpointManager::UpdateCurrentFileCheckpoint(const string& configName,
                                                                   size_t idx,
                                                                   uint64_t offset,
                                                                   uint64_t size) {
    lock_guard<mutex> lock(mUpdateMux);
    auto it = mInputCheckpointMap.find(make_pair(configName, idx));
    if (it == mInputCheckpointMap.end()) {
        // should not happen
        return false;
    }
    bool needDump = false;
    if (!it->second.UpdateCurrentFileCheckpoint(offset, size, needDump)) {
        // should not happen
        return false;
    }
    if (needDump) {
        if (!DumpCheckpointFile(it->second)) {
            LOG_WARNING(sLogger,
                        ("failed to update file checkpoint",
                         "failed to dump checkpoint file")("config", configName)("input idx", idx));
            return false;
        }
    }
    return true;
}

bool InputStaticFileCheckpointManager::InvalidateCurrentFileCheckpoint(const string& configName, size_t idx) {
    lock_guard<mutex> lock(mUpdateMux);
    auto it = mInputCheckpointMap.find(make_pair(configName, idx));
    if (it == mInputCheckpointMap.end()) {
        // should not happen
        return false;
    }
    if (!it->second.InvalidateCurrentFileCheckpoint()) {
        // should not happen
        return false;
    }
    if (!DumpCheckpointFile(it->second)) {
        LOG_WARNING(sLogger,
                    ("failed to update file checkpoint",
                     "failed to dump checkpoint file")("config", configName)("input idx", idx));
        return false;
    }
    return true;
}

bool InputStaticFileCheckpointManager::GetCurrentFileFingerprint(const string& configName,
                                                                 size_t idx,
                                                                 FileFingerprint* cpt) {
    lock_guard<mutex> lock(mUpdateMux);
    auto it = mInputCheckpointMap.find(make_pair(configName, idx));
    if (it == mInputCheckpointMap.end()) {
        // should not happen
        return false;
    }
    return it->second.GetCurrentFileFingerprint(cpt);
}

void InputStaticFileCheckpointManager::DumpAllCheckpointFiles() const {
    lock_guard<mutex> lock(mUpdateMux);
    for (const auto& item : mInputCheckpointMap) {
        if (!DumpCheckpointFile(item.second)) {
            LOG_WARNING(sLogger,
                        ("failed to dump checkpoint file, config",
                         item.second.GetConfigName())("input idx", item.second.GetInputIndex()));
        } else {
#ifdef __ENTERPRISE__
            SelfMonitorServer::GetInstance()->SendTaskStatus();
#endif
        }
    }
}

void InputStaticFileCheckpointManager::GetAllCheckpointFileNames() {
    error_code ec;
    filesystem::file_status s = filesystem::status(mCheckpointRootPath, ec);
    if (ec) {
        LOG_WARNING(sLogger,
                    ("failed to get checkpoint path status", "skip")("filepath", mCheckpointRootPath.string())(
                        "error code", ec.value())("error msg", ec.message()));
        return;
    }
    if (!filesystem::exists(s)) {
        LOG_WARNING(sLogger, ("checkpoint path not existed", "skip")("filepath", mCheckpointRootPath.string()));
        return;
    }
    if (!filesystem::is_directory(s)) {
        LOG_WARNING(sLogger, ("checkpoint path is not a directory", "skip")("filepath", mCheckpointRootPath.string()));
        return;
    }
    for (auto const& entry : filesystem::directory_iterator(mCheckpointRootPath, ec)) {
        const filesystem::path& filepath = entry.path();
        if (!filesystem::is_regular_file(entry.status(ec))) {
            LOG_DEBUG(sLogger, ("checkpoint file is not a regular file", "skip")("filepath", filepath.string()));
            continue;
        }
        if (filepath.extension().string() != ".json") {
            LOG_WARNING(sLogger, ("unsupported checkpoint file format", "skip")("filepath", filepath.string()));
            continue;
        }

        const auto& cptKey = filepath.stem().string();
        auto idx = cptKey.rfind('@');
        if (idx == string::npos) {
            LOG_WARNING(sLogger, ("invalid checkpoint file name: no @", "skip")("filepath", filepath));
            continue;
        }

        uint64_t inputIdx = 0;
        try {
            inputIdx = stoull(cptKey.substr(idx + 1));
        } catch (const std::invalid_argument&) {
            LOG_WARNING(sLogger,
                        ("invalid checkpoint file name: input idx is not digit", "skip")("filepath", filepath));
            continue;
        } catch (const std::out_of_range&) {
            LOG_WARNING(sLogger,
                        ("invalid checkpoint file name: input idx is out of range", "skip")("filepath", filepath));
            continue;
        }
        string configName = cptKey.substr(0, idx);
        mCheckpointFileNamesOnInit.emplace(configName, inputIdx);
    }
    LOG_INFO(sLogger, ("get all checkpoint file names succeeded, file count", mCheckpointFileNamesOnInit.size()));
}

void InputStaticFileCheckpointManager::ClearUnusedCheckpoints() {
    for (const auto& [configName, idx] : mCheckpointFileNamesOnInit) {
        error_code ec;
        if (!filesystem::remove(mCheckpointRootPath / GetCheckpointFileName(configName, idx), ec)) {
            if (ec) {
                LOG_WARNING(sLogger,
                            ("failed to delete ununsed checkpoint file", "skip")("config", configName)(
                                "input idx", idx)("error code", ec.value())("error msg", ec.message()));
            } else {
                LOG_WARNING(sLogger,
                            ("failed to delete unused checkpoint file",
                             "skip")("config", configName)("input idx", idx)("error msg", "file not existed"));
            }
        }
    }
    mCheckpointFileNamesOnInit.clear();
}

bool InputStaticFileCheckpointManager::RetrieveCheckpointFromFile(const string& configName,
                                                                  size_t idx,
                                                                  InputStaticFileCheckpoint* cpt) {
    if (!cpt) {
        // should not happen
        return false;
    }
    auto it = mCheckpointFileNamesOnInit.find(make_pair(configName, idx));
    if (it == mCheckpointFileNamesOnInit.end()) {
        LOG_WARNING(sLogger, ("no checkpoint file found, config", configName)("input idx", idx));
        return false;
    }
    mCheckpointFileNamesOnInit.erase(it);
    return LoadCheckpointFile(mCheckpointRootPath / GetCheckpointFileName(configName, idx), cpt);
}


bool InputStaticFileCheckpointManager::DumpCheckpointFile(const InputStaticFileCheckpoint& cpt) const {
    string res;
    if (!cpt.Serialize(&res)) {
        // should not happen
        return false;
    }
#ifdef __ENTERPRISE__
    if (!cpt.SerializeToLogEvents()) {
        // should not happen
        return false;
    }
#endif

    string errMsg;
    return UpdateFileContent(
        mCheckpointRootPath / GetCheckpointFileName(cpt.GetConfigName(), cpt.GetInputIndex()), res, errMsg);
}

bool InputStaticFileCheckpointManager::LoadCheckpointFile(const filesystem::path& filepath,
                                                          InputStaticFileCheckpoint* cpt) {
    if (!cpt) {
        // should not happen
        return false;
    }
    error_code ec;
    filesystem::file_status s = filesystem::status(filepath, ec);
    if (ec) {
        LOG_WARNING(sLogger, ("failed to get checkpoint file status", "skip")("filepath", filepath));
        return false;
    }
    if (!filesystem::exists(s)) {
        LOG_WARNING(sLogger, ("checkpoint file not existed", "skip")("filepath", filepath));
        return false;
    }
    if (!filesystem::is_regular_file(s)) {
        LOG_WARNING(sLogger, ("checkpoint file is not a regular file", "skip")("filepath", filepath));
        return false;
    }
    string content;
    if (!ReadFile(filepath.string(), content)) {
        LOG_WARNING(sLogger, ("failed to open checkpoint file", "skip")("filepath", filepath));
        return false;
    }
    if (content.empty()) {
        LOG_WARNING(sLogger, ("empty checkpoint file", "skip")("filepath", filepath));
        return false;
    }
    string errMsg;
    if (!cpt->Deserialize(content, &errMsg)) {
        LOG_WARNING(sLogger, ("checkpoint file corrupted", "skip")("error msg", errMsg)("filepath", filepath));
        return false;
    }
    return true;
}

// bool InputStaticFileCheckpointManager::DeleteCheckpoint(const string& configName, size_t idx) {
//     auto it = mInputCheckpointMap.find(make_pair(configName, idx));
//     if (it == mInputCheckpointMap.end()) {
//         return false;
//     }
//     mInputCheckpointMap.erase(it);

//     error_code ec;
//     if (!filesystem::remove(mCheckpointRootPath / GetCheckpointFileName(configName, idx), ec)) {
//         if (ec) {
//             LOG_WARNING(sLogger,
//                         ("failed to delete checkpoint file", "skip")("config", configName)("input idx", idx)(
//                             "error code", ec.value())("error msg", ec.message()));
//         } else {
//             LOG_WARNING(sLogger,
//                         ("failed to delete checkpoint file",
//                          "skip")("config", configName)("input idx", idx)("error msg", "file not existed"));
//         }
//         return false;
//     }
//     return true;
// }

// vector<Json::Value> InputStaticFileCheckpointManager::ExportAllCheckpoints() {
//     lock_guard<mutex> lock(mUpdateMux);
//     vector<Json::Value> res;
//     for (const auto& [key, cpt] : mInputCheckpointMap) {
//         Json::Value root;
//         if (!cpt.Serialize(&root)) {
//             // should not happen
//             continue;
//         }
//         res.emplace_back(std::move(root));
//     }

//     for (const auto& [configName, idx] : mDeletedInputs) {
//         DeleteCheckpoint(configName, idx);
//     }
//     mDeletedInputs.clear();
//     return res;
// }

} // namespace logtail
