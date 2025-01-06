#pragma once

#include <memory>
#include <thread>
#include <mutex>

#include "ebpf/plugin/AbstractManager.h"
#include <security/type.h>

namespace logtail {
namespace ebpf {
class FileSecurityManager : public AbstractManager {
public:
    // static std::shared_ptr<FileSecurityManager> Create(std::unique_ptr<BaseManager>& mgr, std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper) {
    //     return std::make_shared<FileSecurityManager>(mgr, wrapper);
    // }
    // FileSecurityManager(std::unique_ptr<BaseManager>& mgr, std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper);
    ~FileSecurityManager();
    int Init(std::shared_ptr<nami::eBPFConfig>) override;
    int Destroy() override;

    void RecordFileEvent(file_data_t *event);

    void FlushFilekEvent();

    // virtual PluginType GetPluginType() override { return PluginType::FILE_SECURITY; }

    int EnableCallName(const std::string &call_name,
                       const configType config) override;
    int DisableCallName(const std::string &call_name) override;
private:
    // std::shared_ptr<BPFWrapper<sockettrace_secure_bpf>> wrapper_;
};

} // ebpf
} // logtail
