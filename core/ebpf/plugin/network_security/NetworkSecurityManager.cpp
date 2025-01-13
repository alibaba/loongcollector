#include "NetworkSecurityManager.h"
#include "logger/Logger.h"
#include "common/MachineInfoUtil.h"
#include "models/PipelineEventGroup.h"
#include "pipeline/queue/ProcessQueueManager.h"

namespace logtail {
namespace ebpf {

NetworkSecurityManager::NetworkSecurityManager(std::unique_ptr<BaseManager>& base,
                                             std::shared_ptr<SourceManager> sourceManager)
    : AbstractManager(base, sourceManager) {
}

NetworkSecurityManager::~NetworkSecurityManager() {
    Destroy();
}

int NetworkSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) {
    auto securityOpts = std::get_if<SecurityOptions*>(&options);
    if (!securityOpts) {
        LOG_ERROR(sLogger, ("Invalid options type for NetworkSecurityManager", ""));
        return -1;
    }

    mRunning = true;
    // 启动一个线程，调用 PollPerfBuffers

    mRunnerThread = std::thread(&NetworkSecurityManager::RunnerThread, this);
    
    LOG_INFO(sLogger, ("NetworkSecurityManager initialized", ""));
    return 0;
}

void NetworkSecurityManager::Stop() {
    {
        std::lock_guard<std::mutex> lock(mContextMutex);
        mRunning = false;
        mPipelineCtx = nullptr;
        mPluginIndex = -1;
    }
    
    mRunnerCV.notify_one();
    
    if (mRunnerThread.joinable()) {
        mRunnerThread.join();
    }
    if (mPollerThread.joinable()) {
        mPollerThread.join();
    }
}

int NetworkSecurityManager::Destroy() {
    Stop();
    return 0;
}

void NetworkSecurityManager::PollerThread() {    
    while (true) {
        std::unique_lock<std::mutex> lock(mRunnerMutex);
        
        {
            std::lock_guard<std::mutex> contextLock(mContextMutex);
            if (!mRunning) {
                break;
            }
            
            if (mSuspendFlag) {
                mRunnerCV.wait_for(lock, std::chrono::milliseconds(100));
                continue;
            }

            // 从eBPF maps中读取事件
            int32_t flag = 0;
            int ret = mSourceManager->PollPerfBuffers(PluginType::NETWORK_SECURITY, 64, &flag, 0);
            if (ret < 0) {
                LOG_WARNING(sLogger, ("poll event error, ret", ret));
                continue;
            }
        }
    }
}

void NetworkSecurityManager::RunnerThread() {
    std::vector<std::unique_ptr<BaseSecurityEvent>> events;
    events.reserve(64);
    
    while (true) {
        std::unique_lock<std::mutex> lock(mRunnerMutex);
        
        {
            std::lock_guard<std::mutex> contextLock(mContextMutex);
            if (!mRunning) {
                break;
            }
            
            if (mSuspendFlag) {
                mRunnerCV.wait_for(lock, std::chrono::milliseconds(100));
                continue;
            }            
        }

        // events 从 concurrent queue 中读取
        ProcessEvents(events);

        if (!events.empty()) {
            events.clear();
        }
    }
}

void NetworkSecurityManager::ProcessEvents(std::vector<std::unique_ptr<BaseSecurityEvent>>& events) {
    std::lock_guard<std::mutex> lock(mContextMutex);
    if (!mPipelineCtx || mPluginIndex < 0) {
        return;
    }
    // 遍历 events，并送入 Aggregator 中
    for (auto& event : events) {
        // generate hash key
        std::array<size_t, 2> hash_result;
        hash_result.fill(0UL);
        hash_result[0] = 1;
        hash_result[1] = 2;
        mAggregateTree->Aggregate(std::move(event), hash_result);
    }
    
}

int NetworkSecurityManager::EnableCallName(const std::string& call_name, const configType config) {
    return 0;
}

int NetworkSecurityManager::DisableCallName(const std::string& call_name) {
    return 0;
}

void NetworkSecurityManager::ReportAggTree() {
    auto nodes = mAggregateTree->GetNodesWithAggDepth(1);
    if (nodes.empty()) {
        LOG_DEBUG(sLogger, ("empty nodes...", ""));
        return;
    }

    std::vector<std::unique_ptr<SecurityEventGroup>> group;
    group.reserve(nodes.size());
    for (auto& node : nodes) {
        bool init = false;
        auto event_group = std::make_unique<SecurityEventGroup>();
        mAggregateTree->ForEach(node, [&](const BaseSecurityNode* group) {
        // connection level ...
        if (!init) {
            // fill group 
        }
            
        });
        group.emplace_back(std::move(event_group));
    }

    mAggregateTree->Clear();
    // 基于 mPipelineCtx 将 group 发送出去
}

} // namespace ebpf
} // namespace logtail
