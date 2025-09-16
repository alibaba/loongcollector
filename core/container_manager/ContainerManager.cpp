#include "container_manager/ContainerManager.h"

#include <ctime>

#include <boost/regex.hpp>
#include <chrono>

#include "json/json.h"

#include "ConfigManager.h"
#include "app_config/AppConfig.h"
#include "collection_pipeline/CollectionPipelineContext.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "constants/Constants.h"
#include "constants/TagConstants.h"
#include "container_manager/ContainerDiff.h"
#include "container_manager/ContainerDiscoveryOptions.h"
#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"
#include "monitor/Monitor.h"
#include "monitor/SelfMonitorServer.h"

namespace logtail {

// Forward declarations for helpers used across this file
static Json::Value SerializeRawContainerInfo(const std::shared_ptr<RawContainerInfo>& info);
static std::shared_ptr<RawContainerInfo> DeserializeRawContainerInfo(const Json::Value& v);

ContainerManager::ContainerManager() = default;

ContainerManager::~ContainerManager() = default;

ContainerManager* ContainerManager::GetInstance() {
    static ContainerManager instance;
    return &instance;
}

void ContainerManager::Init() {
    if (mIsRunning) {
        return;
    }
    mIsRunning = true;
    LOG_INFO(sLogger, ("ContainerManager", "init"));
    mThreadRes = std::async(std::launch::async, &ContainerManager::pollingLoop, this);
}

void ContainerManager::Stop() {
    if (!mIsRunning) {
        return;
    }
    mIsRunning = false;
    if (mThreadRes.valid()) {
        try {
            auto status = mThreadRes.wait_for(std::chrono::seconds(5));
            if (status == std::future_status::ready) {
                LOG_INFO(sLogger, ("ContainerManager", "polling thread stopped successfully"));
            } else {
                LOG_WARNING(sLogger, ("ContainerManager", "polling thread forced to stopped"));
            }
        } catch (...) {
            LOG_ERROR(sLogger, ("stop polling thread failed", ""));
        }
    }
}

void ContainerManager::pollingLoop() {
    time_t lastUpdateAllTime = 0;
    time_t lastUpdateDiffTime = 0;
    while (true) {
        if (!mIsRunning) {
            break;
        }
        time_t now = time(nullptr);
        if (now - lastUpdateAllTime >= 3600) {
            refreshAllContainersSnapshot();
            lastUpdateAllTime = now;
        } else if (now - lastUpdateDiffTime >= 1) {
            incrementallyUpdateContainersSnapshot();
            lastUpdateDiffTime = now;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void ContainerManager::ApplyContainerDiffs() {
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    for (auto& pair : mConfigContainerDiffMap) {
        const auto& itr = nameConfigMap.find(pair.first);
        if (itr == nameConfigMap.end()) {
            continue;
        }
        const auto& options = itr->second.first;
        const auto& ctx = itr->second.second;


        const auto& diff = pair.second;
        for (const auto& container : diff->mAdded) {
            options->UpdateRawContainerInfo(container, ctx);
        }
        for (const auto& container : diff->mModified) {
            options->UpdateRawContainerInfo(container, ctx);
        }
        for (const auto& container : diff->mRemoved) {
            options->DeleteRawContainerInfo(container);
        }

        // Collect all container IDs from the diff for creating config result
        std::vector<std::string> containerIDs;
        const auto& containerInfos = options->GetContainerInfo();
        if (containerInfos) {
            for (const auto& info : *containerInfos) {
                containerIDs.push_back(info.mRawContainerInfo->mID);
            }
        }
        // Create and store container config result
        ContainerConfigResult configResult = CreateContainerConfigResult(options, ctx, containerIDs);
        LOG_DEBUG(sLogger, ("configResult", configResult.ToString())("configName", ctx->GetConfigName()));
        if (options->GetContainerDiscoveryOptions().mCollectingContainersMeta) {
            mConfigContainerResultMap[ctx->GetConfigName()] = configResult;
        }
    }
    sendConfigContainerInfo();
    mConfigContainerDiffMap.clear();
}


bool ContainerManager::CheckContainerDiffForAllConfig() {
    if (!mIsRunning) {
        return false;
    }
    bool isUpdate = false;
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
        FileDiscoveryOptions* options = itr->second.first;
        if (options->IsContainerDiscoveryEnabled()) {
            bool isCurrentConfigUpdate = checkContainerDiffForOneConfig(options, itr->second.second);
            if (isCurrentConfigUpdate) {
                isUpdate = true;
            }
        }
    }
    return isUpdate;
}

void ContainerManager::UpdateConfigContainerInfoPipeline(CollectionPipelineContext* ctx, size_t inputIndex) {
    WriteLock lock(mConfigContainerInfoPipelineMux);
    mConfigContainerInfoPipelineCtx = ctx;
    mConfigContainerInfoInputIndex = inputIndex;
    LOG_INFO(sLogger, ("config container info pipeline", "updated"));
}

void ContainerManager::RemoveConfigContainerInfoPipeline() {
    WriteLock lock(mConfigContainerInfoPipelineMux);
    mConfigContainerInfoPipelineCtx = nullptr;
    mConfigContainerInfoInputIndex = 0;
    LOG_INFO(sLogger, ("config container info pipeline", "removed"));
}

void ContainerManager::sendConfigContainerInfo() {
    ReadLock lock(mConfigContainerInfoPipelineMux);
    if (mConfigContainerInfoPipelineCtx == nullptr) {
        return;
    }

    PipelineEventGroup pipelineEventGroup(std::make_shared<SourceBuffer>());
    pipelineEventGroup.SetTagNoCopy(LOG_RESERVED_KEY_SOURCE, LoongCollectorMonitor::mIpAddr);
    pipelineEventGroup.SetMetadata(EventGroupMetaKey::INTERNAL_DATA_TYPE,
                                   SelfMonitorServer::INTERNAL_DATA_TYPE_CONTAINER);

    for (const auto& pair : mConfigContainerResultMap) {
        LogEvent* logEventPtr = pipelineEventGroup.AddLogEvent();
        time_t logtime = time(nullptr);

        logEventPtr->SetTimestamp(logtime);

        // Set all fields according to Go implementation
        logEventPtr->SetContent("type", pair.second.DataType);
        logEventPtr->SetContent("project", pair.second.Project);
        logEventPtr->SetContent("logstore", pair.second.Logstore);

        // Handle config_name with $ separator logic like Go version
        std::string configName = pair.second.ConfigName;
        size_t dollarPos = configName.find('$');
        if (dollarPos != std::string::npos && dollarPos + 1 < configName.length()) {
            configName = configName.substr(dollarPos + 1);
        }
        logEventPtr->SetContent("config_name", configName);

        logEventPtr->SetContent("input.source_addresses", pair.second.SourceAddress);
        logEventPtr->SetContent("input.path_exist_container_ids", pair.second.PathExistInputContainerIDs);
        logEventPtr->SetContent("input.path_not_exist_container_ids", pair.second.PathNotExistInputContainerIDs);
        logEventPtr->SetContent("input.type", pair.second.InputType);
        logEventPtr->SetContent("input.container_file", pair.second.InputIsContainerFile);
        logEventPtr->SetContent("flusher.type", pair.second.FlusherType);
        logEventPtr->SetContent("flusher.target_addresses", pair.second.FlusherTargetAddress);
    }
    mConfigContainerResultMap.clear();

    if (pipelineEventGroup.GetEvents().size() > 0) {
        ProcessorRunner::GetInstance()->PushQueue(mConfigContainerInfoPipelineCtx->GetProcessQueueKey(),
                                                  mConfigContainerInfoInputIndex,
                                                  std::move(pipelineEventGroup));
    }
}


bool ContainerManager::checkContainerDiffForOneConfig(FileDiscoveryOptions* options,
                                                      const CollectionPipelineContext* ctx) {
    // If this config's container update time is newer than or equal to global update time,
    // return the cached result if it exists
    if (options->GetLastContainerUpdateTime() > mLastUpdateTime) {
        return false;
    }

    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> containerInfoMap;
    const auto& containerInfos = options->GetContainerInfo();
    if (containerInfos) {
        for (const auto& info : *containerInfos) {
            containerInfoMap[info.mRawContainerInfo->mID] = info.mRawContainerInfo;
        }
    }
    std::vector<std::string> removedList;
    std::vector<std::string> matchAddedList;
    ContainerDiff diff;
    computeMatchedContainersDiff(*(options->GetFullContainerList()),
                                 containerInfoMap,
                                 options->GetContainerDiscoveryOptions().mContainerFilters,
                                 diff);

    // Update the config's container update time when there are changes
    options->SetLastContainerUpdateTime(time(nullptr));

    if (diff.IsEmpty()) {
        return false;
    }

    LOG_INFO(sLogger, ("diff", diff.ToString())("configName", ctx->GetConfigName()));
    mConfigContainerDiffMap[ctx->GetConfigName()] = std::make_shared<ContainerDiff>(diff);
    return true;
}

void ContainerManager::incrementallyUpdateContainersSnapshot() {
    std::string diffContainersMeta = LogtailPlugin::GetInstance()->GetDiffContainersMeta();
    LOG_DEBUG(sLogger, ("diffContainersMeta", diffContainersMeta));

    Json::Value jsonParams;
    std::string errorMsg;
    if (!ParseJsonTable(diffContainersMeta, jsonParams, errorMsg)) {
        LOG_ERROR(sLogger, ("invalid docker container params", diffContainersMeta)("errorMsg", errorMsg));
        return;
    }
    Json::Value diffContainers = jsonParams["DiffCmd"];
    Json::Value updateContainers = diffContainers["Update"];
    Json::Value deleteContainers = diffContainers["Delete"];
    Json::Value stopContainers = diffContainers["Stop"];

    bool hasChanges = false;

    for (const auto& container : updateContainers) {
        auto containerInfo = DeserializeRawContainerInfo(container);
        if (containerInfo && !containerInfo->mID.empty()) {
            std::lock_guard<std::mutex> lock(mContainerMapMutex);
            mContainerMap[containerInfo->mID] = containerInfo;
            hasChanges = true;
        }
    }

    for (const auto& container : deleteContainers) {
        std::string containerId = container.asString();
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        if (mContainerMap.erase(containerId) > 0) {
            hasChanges = true;
        }
    }
    for (const auto& container : stopContainers) {
        std::string containerId = container.asString();
        mStoppedContainerIDsMutex.lock();
        mStoppedContainerIDs.push_back(containerId);
        mStoppedContainerIDsMutex.unlock();
        hasChanges = true;
    }

    if (hasChanges) {
        mLastUpdateTime = time(nullptr);
    }
}

void ContainerManager::refreshAllContainersSnapshot() {
    std::string allContainersMeta = LogtailPlugin::GetInstance()->GetAllContainersMeta();
    LOG_DEBUG(sLogger, ("allContainersMeta", allContainersMeta));
    // cmd 解析json
    Json::Value jsonParams;
    std::string errorMsg;
    if (!ParseJsonTable(allContainersMeta, jsonParams, errorMsg)) {
        LOG_ERROR(sLogger, ("invalid docker container params", allContainersMeta)("errorMsg", errorMsg));
        return;
    }
    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> tmpContainerMap;
    Json::Value allContainers = jsonParams["AllCmd"];
    for (const auto& container : allContainers) {
        auto containerInfo = DeserializeRawContainerInfo(container);
        if (containerInfo && !containerInfo->mID.empty()) {
            tmpContainerMap[containerInfo->mID] = containerInfo;
        }
    }
    std::lock_guard<std::mutex> lock(mContainerMapMutex);
    mContainerMap = tmpContainerMap;
    mLastUpdateTime = time(nullptr);
}


ContainerConfigResult ContainerManager::CreateContainerConfigResult(const FileDiscoveryOptions* options,
                                                                    const CollectionPipelineContext* ctx,
                                                                    const std::vector<std::string>& containerIDs) {
    ContainerConfigResult result;

    if (!options || !ctx) {
        LOG_WARNING(sLogger, ("CreateContainerConfigResult failed", "options or ctx is null"));
        return result;
    }

    // Basic config information
    result.DataType = "container_config_result";
    result.Project = ctx->GetProjectName();
    result.Logstore = ctx->GetLogstoreName();
    result.ConfigName = ctx->GetConfigName();

    // Container IDs - join with semicolon like in Go version
    if (!containerIDs.empty()) {
        std::string containerIDStr;
        for (size_t i = 0; i < containerIDs.size(); ++i) {
            if (i > 0) {
                containerIDStr += ";";
            }
            // Get short container ID (first 12 characters)
            if (containerIDs[i].length() >= 12) {
                containerIDStr += containerIDs[i].substr(0, 12);
            } else {
                containerIDStr += containerIDs[i];
            }
        }
        result.PathExistInputContainerIDs = containerIDStr;
    } else {
        result.PathExistInputContainerIDs = "";
    }

    // Source and type information
    result.SourceAddress = "stdout"; // Similar to input_docker_stdout
    result.InputType = "input_docker_stdout"; // Plugin name
    result.FlusherType = "flusher_sls";
    result.FlusherTargetAddress = ctx->GetProjectName() + "/" + ctx->GetLogstoreName();

    return result;
}

void ContainerManager::GetContainerStoppedEvents(std::vector<Event*>& eventVec) {
    auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
    mStoppedContainerIDsMutex.lock();
    std::vector<std::string> stoppedContainerIDs;
    stoppedContainerIDs.swap(mStoppedContainerIDs);
    mStoppedContainerIDsMutex.unlock();
    if (stoppedContainerIDs.empty()) {
        return;
    }
    LOG_INFO(sLogger, ("stoppedContainerIDs", ToString(stoppedContainerIDs)));

    for (const auto& containerId : stoppedContainerIDs) {
        Event* pStoppedEvent = new Event(containerId, "", EVENT_ISDIR | EVENT_CONTAINER_STOPPED, -1, 0);
        for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
            const FileDiscoveryOptions* options = itr->second.first;
            if (options->IsContainerDiscoveryEnabled()) {
                const auto& containerInfos = options->GetContainerInfo();
                if (containerInfos) {
                    for (auto& info : *containerInfos) {
                        if (info.mRawContainerInfo->mID == containerId) {
                            info.mRawContainerInfo->mStopped = true;
                            pStoppedEvent->SetConfigName(itr->first);
                            break;
                        }
                    }
                }
            }
        }
        pStoppedEvent->SetContainerID(containerId);
        eventVec.push_back(pStoppedEvent);
    }
}


bool IsMapLabelsMatch(const MatchCriteriaFilter& filter, const std::unordered_map<std::string, std::string>& labels) {
    if (!filter.mIncludeFields.mFieldsMap.empty() || !filter.mIncludeFields.mFieldsRegMap.empty()) {
        bool matchedFlag = false;

        // 检查静态 include 标签
        for (const auto& pair : filter.mIncludeFields.mFieldsMap) {
            auto it = labels.find(pair.first);
            if (it != labels.end() && (pair.second.empty() || it->second == pair.second)) {
                matchedFlag = true;
                break;
            }
        }

        // 如果匹配，则不需要检查正则表达式
        if (!matchedFlag) {
            for (const auto& pair : filter.mIncludeFields.mFieldsRegMap) {
                auto it = labels.find(pair.first);
                if (it != labels.end() && boost::regex_match(it->second, *pair.second)) {
                    matchedFlag = true;
                    break;
                }
            }
        }

        // 如果没有匹配，返回 false
        if (!matchedFlag) {
            LOG_DEBUG(sLogger, ("matchedFlag", "false"));
            return false;
        }
    }

    // 检查 exclude 标签
    for (const auto& pair : filter.mExcludeFields.mFieldsMap) {
        auto it = labels.find(pair.first);
        if (it != labels.end() && (pair.second.empty() || it->second == pair.second)) {
            LOG_DEBUG(sLogger, ("no match by exclude static", ""));
            return false;
        }
    }

    // 检查 exclude 正则
    for (const auto& pair : filter.mExcludeFields.mFieldsRegMap) {
        auto it = labels.find(pair.first);
        if (it != labels.end() && boost::regex_match(it->second, *pair.second)) {
            LOG_DEBUG(sLogger, ("no match by exclude regex", ""));
            return false;
        }
    }

    return true;
}


bool IsK8sFilterMatch(const K8sFilter& filter, const K8sInfo& k8sInfo) {
    if (k8sInfo.mPausedContainer) {
        return false;
    }
    // 匹配命名空间
    if (filter.mNamespaceReg && !boost::regex_match(k8sInfo.mNamespace, *filter.mNamespaceReg)) {
        LOG_DEBUG(sLogger,
                  ("k8sInfo.mNamespace", k8sInfo.mNamespace)("filter.mNamespaceReg", filter.mNamespaceReg->str()));
        return false;
    }
    // 匹配 Pod 名称
    if (filter.mPodReg && !boost::regex_match(k8sInfo.mPod, *filter.mPodReg)) {
        LOG_DEBUG(sLogger, ("k8sInfo.mPod", k8sInfo.mPod)("filter.mPodReg", filter.mPodReg->str()));
        return false;
    }
    // 匹配容器名称
    if (filter.mContainerReg && !boost::regex_match(k8sInfo.mContainerName, *filter.mContainerReg)) {
        LOG_DEBUG(
            sLogger,
            ("k8sInfo.mContainerName", k8sInfo.mContainerName)("filter.mContainerReg", filter.mContainerReg->str()));
        return false;
    }
    bool isK8sLabelMatch = IsMapLabelsMatch(filter.mK8sLabelFilter, k8sInfo.mLabels);
    LOG_DEBUG(sLogger, ("mK8sLabelFilter", filter.mK8sLabelFilter.ToString()));
    for (const auto& pair : k8sInfo.mLabels) {
        LOG_DEBUG(sLogger, ("K8sLabel", pair.first + "=" + pair.second));
    }
    // 确保 Labels 不为 nullptr，使用默认的空映射初始化
    return isK8sLabelMatch;
}


void ContainerManager::computeMatchedContainersDiff(
    std::set<std::string>& fullContainerIDList,
    const std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>>& matchList,
    const ContainerFilters& filters,
    ContainerDiff& diff) {
    // 移除已删除的容器
    for (auto it = fullContainerIDList.begin(); it != fullContainerIDList.end();) {
        if (mContainerMap.find(*it) == mContainerMap.end()) {
            std::string id = *it; // 复制一份，避免 erase 后引用失效
            it = fullContainerIDList.erase(it); // 删除元素并移到下一个
            if (matchList.find(id) != matchList.end()) {
                diff.mRemoved.push_back(id);
            }
        } else {
            ++it;
        }
    }

    // 更新匹配的容器状态
    for (auto& pair : matchList) {
        if (auto it = mContainerMap.find(pair.first); it != mContainerMap.end()) {
            // 更新为最新的 info
            if (*pair.second != *it->second) {
                diff.mModified.push_back(it->second);
            }
        }
    }

    // 添加新容器
    for (const auto& pair : mContainerMap) {
        // 如果 fullContainerIDList 中不存在该 id
        if (fullContainerIDList.find(pair.first) == fullContainerIDList.end()) {
            fullContainerIDList.insert(pair.first); // 加入到 fullContainerIDList
            bool isContainerLabelMatch = IsMapLabelsMatch(filters.mContainerLabelFilter, pair.second->mContainerLabels);
            LOG_DEBUG(sLogger, ("mContainerLabelFilter", filters.mContainerLabelFilter.ToString()));
            for (const auto& pair : pair.second->mContainerLabels) {
                LOG_DEBUG(sLogger, ("label", pair.first + "=" + pair.second));
            }

            bool isEnvMatch = IsMapLabelsMatch(filters.mEnvFilter, pair.second->mEnv);

            LOG_DEBUG(sLogger, ("mEnvFilter", filters.mEnvFilter.ToString()));
            for (const auto& pair : pair.second->mEnv) {
                LOG_DEBUG(sLogger, ("env", pair.first + "=" + pair.second));
            }


            bool isK8sMatch = IsK8sFilterMatch(filters.mK8SFilter, pair.second->mK8sInfo);
            LOG_DEBUG(sLogger,
                      ("isContainerLabelMatch", isContainerLabelMatch)("isEnvMatch", isEnvMatch)(
                          "isK8sMatch", isK8sMatch)("container name", pair.second->mID));
            // 检查标签和环境匹配
            if (isContainerLabelMatch && isEnvMatch && isK8sMatch) {
                diff.mAdded.push_back(pair.second); // 添加到变换列表
            }
        }
    }
}

// Serialize RawContainerInfo (complete fields)
static Json::Value SerializeRawContainerInfo(const std::shared_ptr<RawContainerInfo>& info) {
    Json::Value v(Json::objectValue);
    // basic
    v["ID"] = Json::Value(info->mID);
    v["UpperDir"] = Json::Value(info->mUpperDir);
    v["LogPath"] = Json::Value(info->mLogPath);
    v["Stopped"] = Json::Value(info->mStopped);

    // mounts
    Json::Value mounts(Json::arrayValue);
    for (const auto& m : info->mMounts) {
        Json::Value mObj(Json::objectValue);
        mObj["Source"] = Json::Value(m.mSource);
        mObj["Destination"] = Json::Value(m.mDestination);
        mounts.append(mObj);
    }
    v["Mounts"] = mounts;

    // metadata
    Json::Value metadata(Json::objectValue);
    for (const auto& p : info->mMetadatas) {
        metadata[GetDefaultTagKeyString(p.first)] = Json::Value(p.second);
    }
    for (const auto& p : info->mCustomMetadatas) {
        metadata[p.first] = Json::Value(p.second);
    }
    v["MetaDatas"] = metadata;

    // k8s
    Json::Value k8s(Json::objectValue);
    k8s["Namespace"] = Json::Value(info->mK8sInfo.mNamespace);
    k8s["Pod"] = Json::Value(info->mK8sInfo.mPod);
    k8s["ContainerName"] = Json::Value(info->mK8sInfo.mContainerName);
    k8s["PausedContainer"] = Json::Value(info->mK8sInfo.mPausedContainer);
    Json::Value k8sLabels(Json::objectValue);
    for (const auto& p : info->mK8sInfo.mLabels) {
        k8sLabels[p.first] = Json::Value(p.second);
    }
    k8s["Labels"] = k8sLabels;
    v["K8s"] = k8s;

    // env
    Json::Value env(Json::objectValue);
    for (const auto& p : info->mEnv) {
        env[p.first] = Json::Value(p.second);
    }
    v["Env"] = env;

    // container labels
    Json::Value cl(Json::objectValue);
    for (const auto& p : info->mContainerLabels) {
        cl[p.first] = Json::Value(p.second);
    }
    v["ContainerLabels"] = cl;

    return v;
}

// Deserialize RawContainerInfo (complete fields)
static std::shared_ptr<RawContainerInfo> DeserializeRawContainerInfo(const Json::Value& v) {
    auto info = std::make_shared<RawContainerInfo>();
    // basic
    if (v.isMember("ID") && v["ID"].isString()) {
        info->mID = v["ID"].asString();
    }
    if (v.isMember("UpperDir") && v["UpperDir"].isString()) {
        info->mUpperDir = v["UpperDir"].asString();
    }
    if (v.isMember("LogPath") && v["LogPath"].isString()) {
        info->mLogPath = v["LogPath"].asString();
    }
    if (v.isMember("Stopped") && v["Stopped"].isBool()) {
        info->mStopped = v["Stopped"].asBool();
    }

    // mounts
    if (v.isMember("Mounts") && v["Mounts"].isArray()) {
        const auto& mounts = v["Mounts"];
        for (Json::ArrayIndex i = 0; i < mounts.size(); ++i) {
            const auto& m = mounts[i];
            if (m.isObject()) {
                Mount mt;
                if (m.isMember("Source") && m["Source"].isString())
                    mt.mSource = m["Source"].asString();
                if (m.isMember("Destination") && m["Destination"].isString())
                    mt.mDestination = m["Destination"].asString();
                info->mMounts.emplace_back(std::move(mt));
            }
        }
    }

    // metadata
    if (v.isMember("MetaDatas") && v["MetaDatas"].isObject()) {
        const auto& metadata = v["MetaDatas"];
        auto names = metadata.getMemberNames();
        for (const auto& key : names) {
            if (metadata[key].isString()) {
                info->AddMetadata(key, metadata[key].asString());
            }
        }
    }

    // k8s
    if (v.isMember("K8s") && v["K8s"].isObject()) {
        const auto& k8s = v["K8s"];
        if (k8s.isMember("Namespace") && k8s["Namespace"].isString())
            info->mK8sInfo.mNamespace = k8s["Namespace"].asString();
        if (k8s.isMember("Pod") && k8s["Pod"].isString())
            info->mK8sInfo.mPod = k8s["Pod"].asString();
        if (k8s.isMember("ContainerName") && k8s["ContainerName"].isString())
            info->mK8sInfo.mContainerName = k8s["ContainerName"].asString();
        if (k8s.isMember("PausedContainer") && k8s["PausedContainer"].isBool())
            info->mK8sInfo.mPausedContainer = k8s["PausedContainer"].asBool();
        if (k8s.isMember("Labels") && k8s["Labels"].isObject()) {
            const auto& lbs = k8s["Labels"];
            auto names = lbs.getMemberNames();
            for (const auto& key : names) {
                if (lbs[key].isString())
                    info->mK8sInfo.mLabels[key] = lbs[key].asString();
            }
        }
    }

    // env
    if (v.isMember("Env") && v["Env"].isObject()) {
        const auto& env = v["Env"];
        auto names = env.getMemberNames();
        for (const auto& key : names) {
            if (env[key].isString())
                info->mEnv[key] = env[key].asString();
        }
    }

    // container labels
    if (v.isMember("ContainerLabels") && v["ContainerLabels"].isObject()) {
        const auto& cl = v["ContainerLabels"];
        auto names = cl.getMemberNames();
        for (const auto& key : names) {
            if (cl[key].isString())
                info->mContainerLabels[key] = cl[key].asString();
        }
    }

    return info;
}

void ContainerManager::SaveContainerInfo() {
    Json::Value root(Json::objectValue);
    Json::Value arr(Json::arrayValue);
    {
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        for (const auto& kv : mContainerMap) {
            arr.append(SerializeRawContainerInfo(kv.second));
        }
    }
    root["Containers"] = arr;
    root["version"] = "1.0.0";
    std::string configPath = PathJoin(GetAgentDataDir(), "docker_path_config.json");
    OverwriteFile(configPath, root.toStyledString());
    LOG_INFO(sLogger, ("save container state", configPath));
}

void ContainerManager::LoadContainerInfo() {
    std::string configPath = PathJoin(GetAgentDataDir(), "docker_path_config.json");
    std::string content;

    // Load from docker_path_config.json and determine logic based on version
    if (FileReadResult::kOK != ReadFileContent(configPath, content)) {
        LOG_INFO(sLogger, ("docker_path_config.json not found", configPath));
        return;
    }

    Json::Value root;
    std::string err;
    if (!ParseJsonTable(content, root, err)) {
        LOG_WARNING(sLogger, ("invalid docker_path_config.json", err));
        return;
    }

    // Check version to determine parsing logic
    std::string version = "1.0.0"; // Default to new format
    if (root.isMember("version") && root["version"].isString()) {
        version = root["version"].asString();
    }

    if (version == "0.1.0") {
        // Original docker_path_config.json format with detail array
        loadContainerInfoFromDetailFormat(root, configPath);
    } else {
        // New container_state.json style format with Containers array
        loadContainerInfoFromContainersFormat(root, configPath);
    }
    // Apply container diffs immediately after loading
    ApplyContainerDiffs();
}

void ContainerManager::loadContainerInfoFromDetailFormat(const Json::Value& root, const std::string& configPath) {
    if (!root.isMember("detail") || !root["detail"].isArray()) {
        LOG_WARNING(sLogger, ("detail array not found in docker_path_config.json", ""));
        return;
    }

    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> tmpContainerMap;
    std::unordered_map<std::string, std::vector<std::shared_ptr<RawContainerInfo>>> configContainerMap;
    const auto& arr = root["detail"];

    for (Json::ArrayIndex i = 0; i < arr.size(); ++i) {
        const auto& item = arr[i];
        std::string configName;
        if (item.isMember("config_name") && item["config_name"].isString()) {
            configName = item["config_name"].asString();
        }

        if (item.isMember("params") && item["params"].isString()) {
            std::string paramsStr = item["params"].asString();
            Json::Value paramsJson;
            std::string err;
            if (ParseJsonTable(paramsStr, paramsJson, err)) {
                auto info = std::make_shared<RawContainerInfo>();

                // Parse basic fields
                if (paramsJson.isMember("ID") && paramsJson["ID"].isString()) {
                    info->mID = paramsJson["ID"].asString();
                }
                if (paramsJson.isMember("UpperDir") && paramsJson["UpperDir"].isString()) {
                    info->mUpperDir = paramsJson["UpperDir"].asString();
                }
                if (paramsJson.isMember("LogPath") && paramsJson["LogPath"].isString()) {
                    info->mLogPath = paramsJson["LogPath"].asString();
                }

                // Parse mounts
                if (paramsJson.isMember("Mounts") && paramsJson["Mounts"].isArray()) {
                    const auto& mounts = paramsJson["Mounts"];
                    for (Json::ArrayIndex j = 0; j < mounts.size(); ++j) {
                        const auto& m = mounts[j];
                        if (m.isObject()) {
                            Mount mt;
                            if (m.isMember("Source") && m["Source"].isString()) {
                                mt.mSource = m["Source"].asString();
                            }
                            if (m.isMember("Destination") && m["Destination"].isString()) {
                                mt.mDestination = m["Destination"].asString();
                            }
                            info->mMounts.emplace_back(std::move(mt));
                        }
                    }
                }

                // Parse MetaDatas for K8s info and other metadata
                if (paramsJson.isMember("MetaDatas") && paramsJson["MetaDatas"].isArray()) {
                    const auto& metadatas = paramsJson["MetaDatas"];
                    for (Json::ArrayIndex j = 0; j < metadatas.size(); j += 2) {
                        if (j + 1 < metadatas.size() && metadatas[j].isString() && metadatas[j + 1].isString()) {
                            std::string key = metadatas[j].asString();
                            std::string value = metadatas[j + 1].asString();

                            // Store all metadata
                            info->AddMetadata(key, value);

                            // Also handle specific known fields for backward compatibility
                            if (key == "_namespace_") {
                                info->mK8sInfo.mNamespace = value;
                            } else if (key == "_pod_name_") {
                                info->mK8sInfo.mPod = value;
                            } else if (key == "_container_name_") {
                                info->mK8sInfo.mContainerName = value;
                            } else if (key == "_image_name_") {
                                // Store image name in env or container labels
                                info->mContainerLabels["_image_name_"] = value;
                            } else if (key == "_container_ip_") {
                                info->mContainerLabels["_container_ip_"] = value;
                            } else if (key == "_pod_uid_") {
                                info->mK8sInfo.mLabels["pod-uid"] = value;
                            }
                        }
                    }
                }

                // Add to container map if ID is valid
                if (!info->mID.empty()) {
                    tmpContainerMap[info->mID] = info;
                    // Also associate with config if config name is available
                    if (!configName.empty()) {
                        configContainerMap[configName].push_back(info);
                    }
                }
            } else {
                LOG_WARNING(sLogger, ("invalid params json in docker_path_config.json", err));
            }
        }
    }

    if (!tmpContainerMap.empty()) {
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        mContainerMap.swap(tmpContainerMap);

        // Update config container diffs for each config
        for (const auto& configPair : configContainerMap) {
            const std::string& configName = configPair.first;
            const auto& containers = configPair.second;

            ContainerDiff diff;
            for (const auto& container : containers) {
                diff.mAdded.push_back(container);
            }

            if (!diff.IsEmpty()) {
                mConfigContainerDiffMap[configName] = std::make_shared<ContainerDiff>(diff);
            }
        }

        LOG_INFO(sLogger, ("load container state from docker_path_config.json (v0.1.0)", configPath));
    }
}

void ContainerManager::loadContainerInfoFromContainersFormat(const Json::Value& root, const std::string& configPath) {
    if (!root.isMember("Containers") || !root["Containers"].isArray()) {
        LOG_WARNING(sLogger, ("Containers array not found in docker_path_config.json", ""));
        return;
    }

    std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> tmp;
    std::vector<std::shared_ptr<RawContainerInfo>> allContainers;
    const auto& arr = root["Containers"];

    for (Json::ArrayIndex i = 0; i < arr.size(); ++i) {
        auto info = DeserializeRawContainerInfo(arr[i]);
        if (!info->mID.empty()) {
            tmp[info->mID] = info;
            allContainers.push_back(info);
        }
    }

    if (!tmp.empty()) {
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        mContainerMap.swap(tmp);

        // Apply containers to all existing configs
        if (!allContainers.empty()) {
            auto nameConfigMap = FileServer::GetInstance()->GetAllFileDiscoveryConfigs();
            for (auto itr = nameConfigMap.begin(); itr != nameConfigMap.end(); ++itr) {
                FileDiscoveryOptions* options = itr->second.first;
                if (options->IsContainerDiscoveryEnabled()) {
                    checkContainerDiffForOneConfig(options, itr->second.second);
                }
            }
        }
        LOG_INFO(sLogger, ("load container state from docker_path_config.json (v1.0.0+)", configPath));
    }
}

} // namespace logtail
