#include "container_manager/ContainerManager.h"

#include <ctime>

#include <boost/regex.hpp>

#include "json/json.h"

#include "ConfigManager.h"
#include "app_config/AppConfig.h"
#include "common/FileSystemUtil.h"
#include "common/JsonUtil.h"
#include "common/StringTools.h"
#include "file_server/FileServer.h"
#include "go_pipeline/LogtailPlugin.h"

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
    mThread = CreateThread([this]() { pollingLoop(); });
}

void ContainerManager::Stop() {
    if (!mIsRunning) {
        return;
    }
    mIsRunning = false;
    if (mThread != NULL) {
        try {
            mThread->Wait(5 * 1000000);
        } catch (...) {
            LOG_ERROR(sLogger, ("stop polling modify thread failed", ToString((int)mThread->GetState())));
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
        if (now - lastUpdateAllTime >= 100) {
            refreshAllContainersSnapshot();
            lastUpdateAllTime = now;
        } else if (now - lastUpdateDiffTime >= 10) {
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
    }
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

bool ContainerManager::checkContainerDiffForOneConfig(FileDiscoveryOptions* options,
                                                      const CollectionPipelineContext* ctx) {
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
    if (diff.IsEmpty()) {
        return false;
    }
    LOG_INFO(sLogger, ("diff", diff.ToString()));
    mConfigContainerDiffMap[ctx->GetConfigName()] = std::make_shared<ContainerDiff>(diff);
    return true;
}

void ContainerManager::incrementallyUpdateContainersSnapshot() {
    std::string diffContainersMeta = LogtailPlugin::GetInstance()->GetDiffContainersMeta();
    LOG_INFO(sLogger, ("diffContainersMeta", diffContainersMeta));

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

    for (const auto& container : updateContainers) {
        auto containerInfo = DeserializeRawContainerInfo(container);
        if (containerInfo && !containerInfo->mID.empty()) {
            std::lock_guard<std::mutex> lock(mContainerMapMutex);
            mContainerMap[containerInfo->mID] = containerInfo;
        }
    }

    for (const auto& container : deleteContainers) {
        std::string containerId = container.asString();
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        mContainerMap.erase(containerId);
    }
    for (const auto& container : stopContainers) {
        std::string containerId = container.asString();
        mStoppedContainerIDsMutex.lock();
        mStoppedContainerIDs.push_back(containerId);
        mStoppedContainerIDsMutex.unlock();
    }
}

void ContainerManager::refreshAllContainersSnapshot() {
    std::string allContainersMeta = LogtailPlugin::GetInstance()->GetAllContainersMeta();
    LOG_INFO(sLogger, ("allContainersMeta", allContainersMeta));
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
            return false;
        }
    }

    // 检查 exclude 标签
    for (const auto& pair : filter.mExcludeFields.mFieldsMap) {
        auto it = labels.find(pair.first);
        if (it != labels.end() && (pair.second.empty() || it->second == pair.second)) {
            return false;
        }
    }

    // 检查 exclude 正则
    for (const auto& pair : filter.mExcludeFields.mFieldsRegMap) {
        auto it = labels.find(pair.first);
        if (it != labels.end() && boost::regex_match(it->second, *pair.second)) {
            return false;
        }
    }

    return true;
}


bool IsK8sFilterMatch(const K8sFilter& filter, const K8sInfo& k8sInfo) {
    // 匹配命名空间
    if (filter.mNamespaceReg && !boost::regex_match(k8sInfo.mNamespace, *filter.mNamespaceReg)) {
        return false;
    }
    // 匹配 Pod 名称
    if (filter.mPodReg && !boost::regex_match(k8sInfo.mPod, *filter.mPodReg)) {
        return false;
    }
    // 匹配容器名称
    if (filter.mContainerReg && !boost::regex_match(k8sInfo.mContainerName, *filter.mContainerReg)) {
        return false;
    }

    // 确保 Labels 不为 nullptr，使用默认的空映射初始化
    return IsMapLabelsMatch(filter.mK8sLabelFilter, k8sInfo.mLabels);
}


void ContainerManager::computeMatchedContainersDiff(
    std::set<std::string>& fullContainerIDList,
    const std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>>& matchList,
    const ContainerFilters& filters,
    ContainerDiff& diff) {
    int newCount = 0;
    int delCount = 0;

    // 移除已删除的容器
    for (auto it = fullContainerIDList.begin(); it != fullContainerIDList.end();) {
        if (mContainerMap.find(*it) == mContainerMap.end()) {
            std::string id = *it; // 复制一份，避免 erase 后引用失效
            it = fullContainerIDList.erase(it); // 删除元素并移到下一个
            if (matchList.find(id) != matchList.end()) {
                diff.mRemoved.push_back(id);
                // matchList.erase(id);
                delCount++;
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
            // 检查标签和环境匹配
            if (IsMapLabelsMatch(filters.mContainerLabelFilter, pair.second->mContainerLabels)
                && IsMapLabelsMatch(filters.mEnvFilter, pair.second->mEnv)
                && IsK8sFilterMatch(filters.mK8SFilter, pair.second->mK8sInfo)) {
                newCount++;
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
    std::string statePath = PathJoin(GetAgentDataDir(), "container_state.json");
    OverwriteFile(statePath, root.toStyledString());
    LOG_INFO(sLogger, ("save container state", statePath));
}

void ContainerManager::LoadContainerInfo() {
    std::string statePath = PathJoin(GetAgentDataDir(), "container_state.json");
    std::string content;
    if (FileReadResult::kOK != ReadFileContent(statePath, content)) {
        return;
    }
    Json::Value root;
    std::string err;
    if (!ParseJsonTable(content, root, err)) {
        LOG_WARNING(sLogger, ("invalid container state json", err));
        return;
    }

    if (root.isMember("Containers") && root["Containers"].isArray()) {
        std::unordered_map<std::string, std::shared_ptr<RawContainerInfo>> tmp;
        const auto& arr = root["Containers"];
        for (Json::ArrayIndex i = 0; i < arr.size(); ++i) {
            auto info = DeserializeRawContainerInfo(arr[i]);
            if (!info->mID.empty()) {
                tmp[info->mID] = info;
            }
        }
        std::lock_guard<std::mutex> lock(mContainerMapMutex);
        mContainerMap.swap(tmp);
    }
    LOG_INFO(sLogger, ("load container state", statePath));
}
} // namespace logtail
