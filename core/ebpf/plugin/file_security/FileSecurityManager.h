#pragma once

#include <security/type.h>

#include <memory>
#include <mutex>
#include <thread>

#include "common/queue/blockingconcurrentqueue.h"
#include "ebpf/Config.h"
#include "ebpf/plugin/AbstractManager.h"
#include "ebpf/plugin/BaseManager.h"
#include "ebpf/type/NetworkObserverEvent.h"

namespace logtail {
namespace ebpf {
class FileSecurityManager : public AbstractManager {
public:
    // static std::shared_ptr<FileSecurityManager> Create(std::unique_ptr<BaseManager>& mgr,
    // std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper) {
    //     return std::make_shared<FileSecurityManager>(mgr, wrapper);
    // }
    FileSecurityManager() = delete;
    FileSecurityManager(std::unique_ptr<BaseManager>& baseMgr, std::shared_ptr<SourceManager> sourceManager)
        : AbstractManager(baseMgr, sourceManager) {}

    ~FileSecurityManager();
    int Init(const std::variant<SecurityOptions*, logtail::ebpf::ObserverNetworkOption*> options) override;
    int Destroy() override;

    void RecordFileEvent(file_data_t* event);

    void FlushFilekEvent();

    virtual PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    int EnableCallName(const std::string& call_name, const configType config) override;
    int DisableCallName(const std::string& call_name) override;

private:
    mutable moodycamel::BlockingConcurrentQueue<std::unique_ptr<FileSecurityNode>> mEventQueue;
};

} // namespace ebpf
} // namespace logtail
