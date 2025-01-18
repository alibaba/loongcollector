#pragma once

extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <coolbpf/security.skel.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "BPFMapTraits.h"
#include "IdAllocator.h"
#include "Log.h"
#include "eBPFWrapper.h"
#include "ebpf/include/export.h"

namespace logtail {
namespace ebpf {

int CreateFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name,
                                const std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> config);

int DeleteFileFilterForCallname(std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> wrapper,
                                const std::string& call_name);

} // namespace ebpf
} // namespace logtail
