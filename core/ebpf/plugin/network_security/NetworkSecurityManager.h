#pragma once

#include "ebpf/plugin/AbstractManager.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "common/timer/Timer.h"
#include "common/queue/blockingconcurrentqueue.h"

namespace logtail {
namespace ebpf {

class NetworkSecurityManager : public AbstractManager {
public:
    NetworkSecurityManager(std::unique_ptr<BaseManager>& base,
                          std::shared_ptr<SourceManager> sourceManager);
    ~NetworkSecurityManager() override;

    int Init(const std::variant<SecurityOptions*, ObserverNetworkOption*> options) override;
    int Destroy() override;
    int EnableCallName(const std::string& call_name, const configType config) override;
    int DisableCallName(const std::string& call_name) override;
    
    PluginType GetPluginType() override { return PluginType::NETWORK_SECURITY; }

private:
    void PollerThread();
    void RunnerThread();
    void ProcessEvents();
    void ReportAggTree();
    void InitTimer();
    
    std::thread mRunnerThread;
    std::thread mPollerThread;
    moodycamel::BlockingConcurrentQueue<std::unique_ptr<BaseSecurityEvent>> mEventQueue;
};

} // namespace ebpf
} // namespace logtail
