#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/util/FrequencyManager.h"
#include "pipeline/PipelineContext.h"

namespace logtail {
namespace ebpf {

class NetworkSecurityManager : public AbstractManager {
public:
    explicit NetworkSecurityManager(std::unique_ptr<BaseManager>& base, 
                                  std::shared_ptr<SourceManager> sourceManager);
    ~NetworkSecurityManager() override;

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) override;
    int Destroy() override;
    int EnableCallName(const std::string& call_name, const configType config) override;
    int DisableCallName(const std::string& call_name) override;
    PluginType GetPluginType() override { return PluginType::NETWORK_SECURITY; }

private:
    void RunnerThread();
    void ProcessEvents(std::vector<std::unique_ptr<BaseSecurityEvent>>& events);
    void PollerThread();
    void ReportAggTree();
    void Stop();

    std::atomic_bool mRunning{false};
    std::thread mPollerThread;
    std::thread mRunnerThread;
    
    // 用于线程同步
    std::condition_variable mRunnerCV;
    std::mutex mRunnerMutex;
    
    // 保护上下文相关的成员变量
    mutable std::mutex mContextMutex;
    const PipelineContext* mPipelineCtx{nullptr};
    int32_t mPluginIndex{-1};
};

} // namespace ebpf
} // namespace logtail
