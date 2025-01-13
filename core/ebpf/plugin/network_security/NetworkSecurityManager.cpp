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
    mAggregateTree = std::make_unique<SIZETAggTree<BaseSecurityNode, std::unique_ptr<BaseSecurityEvent>>>(
        1000, // max nodes
        [](std::unique_ptr<BaseSecurityNode>& base, const std::unique_ptr<BaseSecurityEvent>& n) {
            // 聚合逻辑
        },
        [](const std::unique_ptr<BaseSecurityEvent>& n) {
            // 生成新节点逻辑
            return std::make_unique<BaseSecurityNode>();
        }
    );
}

// 网络监控 3 个 poller parser iterator 
// poller 2 个 
// 共用 1 个 runner 
// 共用 1 个 Timer 

int NetworkSecurityManager::Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) {
    auto securityOpts = std::get_if<SecurityOptions*>(&options);
    if (!securityOpts) {
        LOG_ERROR(sLogger, ("Invalid options type for NetworkSecurityManager", ""));
        return -1;
    }

    mFlag = true;
    
    // 启动工作线程
    mPollerThread = std::thread(&NetworkSecurityManager::PollerThread, this);
    mRunnerThread = std::thread(&NetworkSecurityManager::RunnerThread, this);
    
    // 初始化定时器
    InitTimer();
    
    LOG_INFO(sLogger, ("NetworkSecurityManager initialized", ""));
    return 0;
}

void NetworkSecurityManager::RunnerThread() {
    while (mFlag) {
        if (mSuspendFlag) {
            std::unique_lock<std::mutex> lock(mContextMutex);
            mRunnerCV.wait(lock, [this]() { return !mSuspendFlag || !mFlag; });
            continue;
        }
        // consume queue && aggregate
        ProcessEvents();
        
    } 
}

int NetworkSecurityManager::Destroy() {
    if (!mFlag) {
        return 0;
    }

    mFlag = false;
    mRunnerCV.notify_all();

    if (mRunnerThread.joinable()) {
        mRunnerThread.join();
    }

    if (mPollerThread.joinable()) {
        mPollerThread.join();
    }

    {
        std::lock_guard<std::mutex> lock(mContextMutex);
        mPipelineCtx = nullptr;
        mQueueKey = 0;
        mPluginIndex = -1;
    }

    return 0;
}

void NetworkSecurityManager::InitTimer() {
    // TODO @qianlu.kk self event ...
    auto event = std::make_unique<TimerEvent>(
        std::chrono::seconds(1), // every 1 second
        [this]() {
            if (!mSuspendFlag) {
                ReportAggTree();
            }
        },
        [this]() {
            // add lock ...
            return !mSuspendFlag && mFlag;
        }
    );
    
    if (mTimer) {
        mTimer->PushEvent(std::move(event));
    }
}

void NetworkSecurityManager::ProcessEvents() {
    // std::unique_lock<std::mutex> lock(mContextMutex);
    // if (!mPipelineCtx || mPluginIndex < 0) {
    //     return;
    // }
    std::vector<std::unique_ptr<BaseSecurityEvent>> items(1024);
    size_t count = mEventQueue.wait_dequeue_bulk_timed(items.data(), 1024, std::chrono::milliseconds(200));
    LOG_DEBUG(sLogger, ("get records:", count));
    // handle ....
    if (count == 0) {
        return;
    }
    
    for (auto& event : items) {
        std::array<size_t, 2> hashResult{1, 2};
        mAggregateTree->Aggregate(std::move(event), hashResult);
    }
}

void NetworkSecurityManager::ReportAggTree() {
    std::unique_lock<std::mutex> lock(mContextMutex);
    if (!mPipelineCtx || mPluginIndex < 0) {
        return;
    }

    auto nodes = mAggregateTree->GetNodesWithAggDepth(1);
    if (nodes.empty()) {
        return;
    }
    
    for (auto* node : nodes) {
        auto eventGroup = std::make_unique<PipelineEventGroup>();
        // TODO @qianlu.kk fill event group
        mAggregateTree->ForEach(node, [&](const BaseSecurityNode* group) {

        });

        std::unique_lock<std::mutex> lock(mContextMutex);
        if (!mPipelineCtx || mPluginIndex < 0) {
            return;
        }
        
        std::unique_ptr<ProcessQueueItem> item = std::make_unique<ProcessQueueItem>(std::move(eventGroup), mPluginIndex);
        if (ProcessQueueManager::GetInstance()->PushQueue(mQueueKey, std::move(item))) {
            LOG_WARNING(
                sLogger,
                ("configName", mPipelineCtx->GetConfigName())("pluginIdx", mPluginIndex)("[Event] push queue failed!", ""));
        }
    }


    // TODO @qianlu.kk push to ProcessQueue
    // ProcessQueueManager::GetInstance()->PushEventGroup(mPipelineCtx, mPluginIndex, std::move(groups));
    
    mAggregateTree->Clear();
    
}

} // namespace ebpf
} // namespace logtail
