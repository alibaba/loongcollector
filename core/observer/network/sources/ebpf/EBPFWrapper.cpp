// Copyright 2022 iLogtail Authors
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

#include "app_config/AppConfig.h"
#include "logger/Logger.h"
#include "EBPFWrapper.h"
#include "RuntimeUtil.h"
#include "interface/network.h"
#include "TimeUtil.h"
#include <dlfcn.h>
#include "MachineInfoUtil.h"
#include "FileSystemUtil.h"
#include "metas/ConnectionMetaManager.h"
#include "network/protocols/utils.h"
#include <netinet/in.h>
#include "interface/layerfour.h"

DECLARE_FLAG_STRING(default_container_host_path);
DEFINE_FLAG_INT64(sls_observer_ebpf_min_kernel_version,
                  "the minimum kernel version that supported eBPF normal running, 4.19.0.0 -> 4019000000",
                  4019000000);
DEFINE_FLAG_INT64(
    sls_observer_ebpf_nobtf_kernel_version,
    "the minimum kernel version that supported eBPF normal running without self BTF file, 5.4.0.0 -> 5004000000",
    5004000000);
DEFINE_FLAG_STRING(sls_observer_ebpf_host_path,
                   "the backup real host path for store libebpf.so",
                   "/etc/ilogtail/ebpf/");

static const std::string kLowkernelCentosName = "CentOS";
static const uint16_t kLowkernelCentosMinVersion = 7006;
static const uint16_t kLowkernelSpecificVersion = 3010;


namespace logtail {
// copy from libbpf_print_level
enum sls_libbpf_print_level {
    SLS_LIBBPF_WARN,
    SLS_LIBBPF_INFO,
    SLS_LIBBPF_DEBUG,
};
#define LOAD_EBPF_FUNC(funcName) \
    { \
        void* funcPtr = mEBPFLib->LoadMethod(#funcName, loadErr); \
        if (funcPtr == NULL) { \
            LOG_ERROR(sLogger, ("load ebpf method", "failed")("method", #funcName)("error", loadErr)); \
            LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, \
                                                   std::string("load ebpf method failed: ") + #funcName); \
            return false; \
        } \
        g_##funcName##_func = funcName##_func(funcPtr); \
    }
#define LOAD_UPROBE_OFFSET_FAIL (-100)
#define LOAD_UPROBE_OFFSET(funcName) \
    ({ \
        Dl_info dlinfo; \
        int dlAddrErr = dladdr((const void*)(funcName), &dlinfo); \
        long res = 0; \
        if (dlAddrErr == 0) { \
            LOG_ERROR(sLogger, \
                      ("load ebpf dl address", "failed")("error", dlAddrErr)("method", "g_ebpf_cleanup_dog_func")); \
            LogtailAlarm::GetInstance()->SendAlarm( \
                OBSERVER_INIT_ALARM, \
                "cannot load ebpf dl address, method g_ebpf_cleanup_dog_func, error:" + std::to_string(dlAddrErr)); \
            res = LOAD_UPROBE_OFFSET_FAIL; \
        } else { \
            res = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase; \
            LOG_DEBUG(sLogger, ("func", #funcName)("offset", res)); \
        } \
        res; \
    })

typedef void (*ebpf_setup_net_data_process_func_func)(net_data_process_func_t func, void* custom_data);

typedef void (*ebpf_setup_net_event_process_func_func)(net_ctrl_process_func_t func, void* custom_data);

typedef void (*ebpf_setup_net_statistics_process_func_func)(net_statistics_process_func_t func, void* custom_data);

typedef void (*ebpf_setup_net_lost_func_func)(net_lost_func_t func, void* custom_data);

typedef void (*ebpf_setup_print_func_func)(net_print_fn_t func);

typedef int32_t (*ebpf_config_func)(
    int32_t opt1, int32_t opt2, int32_t params_count, void** params, int32_t* params_len);

typedef int32_t (*ebpf_poll_events_func)(int32_t max_events, int32_t* stop_flag);

typedef int32_t (*ebpf_init_func)(char* btf,
                                  int32_t btf_size,
                                  char* so,
                                  int32_t so_size,
                                  long uprobe_offset,
                                  long upca_offset,
                                  long upps_offset,
                                  long upcr_offset);

typedef int32_t (*ebpf_start_func)();

typedef int32_t (*ebpf_stop_func)();

typedef int32_t (*ebpf_get_fd_func)(void);

typedef int32_t (*ebpf_get_next_key_func)(int32_t fd, const void* key, void* next_key);

typedef void (*ebpf_delete_map_value_func)(void* key, int32_t size);

typedef void (*ebpf_cleanup_dog_func)(void* key, int32_t size);
typedef void (*ebpf_update_conn_addr_func)(struct connect_id_t* conn_id,
                                           union sockaddr_t* dest_addr,
                                           uint16_t local_port,
                                           bool drop);
typedef void (*ebpf_disable_process_func)(uint32_t pid, bool drop);
typedef void (*ebpf_update_conn_role_func)(struct connect_id_t* conn_id, enum support_role_e role_type);


ebpf_setup_net_data_process_func_func g_ebpf_setup_net_data_process_func_func = NULL;
ebpf_setup_net_event_process_func_func g_ebpf_setup_net_event_process_func_func = NULL;
ebpf_setup_net_statistics_process_func_func g_ebpf_setup_net_statistics_process_func_func = NULL;
ebpf_setup_net_lost_func_func g_ebpf_setup_net_lost_func_func = NULL;
ebpf_setup_print_func_func g_ebpf_setup_print_func_func = NULL;
ebpf_config_func g_ebpf_config_func = NULL;
ebpf_poll_events_func g_ebpf_poll_events_func = NULL;
ebpf_init_func g_ebpf_init_func = NULL;
ebpf_start_func g_ebpf_start_func = NULL;
ebpf_stop_func g_ebpf_stop_func = NULL;
ebpf_get_fd_func g_ebpf_get_fd_func = NULL;
ebpf_get_next_key_func g_ebpf_get_next_key_func = NULL;
ebpf_delete_map_value_func g_ebpf_delete_map_value_func = NULL;
ebpf_cleanup_dog_func g_ebpf_cleanup_dog_func = NULL;
ebpf_update_conn_addr_func g_ebpf_update_conn_addr_func = NULL;
ebpf_disable_process_func g_ebpf_disable_process_func = NULL;
ebpf_update_conn_role_func g_ebpf_update_conn_role_func = NULL;


static bool EBPFLoadSuccess() {
    return g_ebpf_setup_net_data_process_func_func != NULL && g_ebpf_setup_net_event_process_func_func != NULL
        && g_ebpf_setup_net_statistics_process_func_func != NULL && g_ebpf_setup_net_lost_func_func != NULL
        && g_ebpf_setup_print_func_func != NULL && g_ebpf_config_func != NULL && g_ebpf_poll_events_func != NULL
        && g_ebpf_init_func != NULL && g_ebpf_start_func != NULL && g_ebpf_stop_func != NULL
        && g_ebpf_get_fd_func != NULL && g_ebpf_get_next_key_func != NULL && g_ebpf_delete_map_value_func != NULL
        && g_ebpf_cleanup_dog_func != NULL && g_ebpf_update_conn_addr_func != NULL
        && g_ebpf_disable_process_func != NULL;
}

static void set_ebpf_int_config(int32_t opt, int32_t opt2, int32_t value) {
    if (g_ebpf_config_func == NULL) {
        return;
    }
    int32_t* params[] = {&value};
    int32_t paramsLen[] = {4};
    g_ebpf_config_func(opt, opt2, 1, (void**)params, paramsLen);
}

static int ebpf_print_callback(int16_t level, const char* format, va_list args) {
    sls_libbpf_print_level printLevel = (sls_libbpf_print_level)level;
    char buffer[1024] = {0};
    vsnprintf(buffer, 1023, format, args);
    switch (printLevel) {
        case SLS_LIBBPF_WARN:
            LOG_WARNING(sLogger, ("module", "ebpf")("msg", buffer));
            break;
        case SLS_LIBBPF_INFO:
            LOG_INFO(sLogger, ("module", "ebpf")("msg", buffer));
            break;
        case SLS_LIBBPF_DEBUG:
            LOG_DEBUG(sLogger, ("module", "ebpf")("msg", buffer));
            break;
        default:
            LOG_INFO(sLogger, ("module", "ebpf")("level", int(level))("msg", buffer));
            break;
    }
    return 0;
}

// static void test_print(int16_t level,
//                        const char *format, ...) {
//     va_list args;

//     va_start(args, format);
//     ebpf_print_callback(level, format, args);
//     va_end(args);
// }

static void ebpf_data_process_callback(void* custom_data, struct conn_data_event_t* event_data) {
    if (custom_data == NULL || event_data == NULL) {
        return;
    }
    ((EBPFWrapper*)custom_data)->OnData(event_data);
}

static void ebpf_ctrl_process_callback(void* custom_data, struct conn_ctrl_event_t* event_data) {
    if (custom_data == NULL || event_data == NULL) {
        return;
    }
    ((EBPFWrapper*)custom_data)->OnCtrl(event_data);
}

static void ebpf_stat_process_callback(void* custom_data, struct conn_stats_event_t* event_data) {
    if (custom_data == NULL || event_data == NULL) {
        return;
    }
    ((EBPFWrapper*)custom_data)->OnStat(event_data);
}

static void ebpf_lost_callback(void* custom_data, enum callback_type_e type, uint64_t lost_count) {
    if (custom_data == NULL) {
        return;
    }
    ((EBPFWrapper*)custom_data)->OnLost(type, lost_count);
}

static std::string GetValidBTFPath(const int64_t& kernelVersion, const std::string& kernelRelease) {
    char* configedBTFPath = getenv("ALIYUN_SLS_EBPF_BTF_PATH");
    if (configedBTFPath != nullptr) {
        return {configedBTFPath};
    }
    // ebpf lib load
    std::string execDir = GetProcessExecutionDir();
    fsutil::Dir dir(execDir);
    if (!dir.Open()) {
        return "";
    }
    std::string lastMatch;
    fsutil::Entry entry;
    while (true) {
        entry = dir.ReadNext();
        if (!entry) {
            break;
        }
        if (!entry.IsRegFile()) {
            continue;
        }
        if (entry.Name().find(kernelRelease) != std::string::npos) {
            return execDir + entry.Name();
        }
        if (entry.Name().find("vmlinux-") == (size_t)0) {
            lastMatch = entry.Name();
        }
    }
    if (!lastMatch.empty()) {
        return execDir + lastMatch;
    }
    return "";
}

bool EBPFWrapper::isSupportedOS(int64_t& kernelVersion, std::string& kernelRelease) {
    GetKernelInfo(kernelRelease, kernelVersion);
    LOG_INFO(sLogger, ("kernel version", kernelRelease));
    if (kernelRelease.empty()) {
        return false;
    }
    if (kernelVersion >= INT64_FLAG(sls_observer_ebpf_min_kernel_version)) {
        return true;
    }
    if (kernelVersion / 1000000 != kLowkernelSpecificVersion) {
        return false;
    }
    std::string os;
    int64_t osVersion;

    if (GetRedHatReleaseInfo(os, osVersion, STRING_FLAG(default_container_host_path))
        || GetRedHatReleaseInfo(os, osVersion)) {
        return os == kLowkernelCentosName && osVersion >= kLowkernelCentosMinVersion;
    }
    return false;
}

bool EBPFWrapper::loadEbpfLib(int64_t kernelVersion, std::string& soPath) {
    if (mEBPFLib != nullptr) {
        if (!EBPFLoadSuccess()) {
            LOG_INFO(sLogger, ("load ebpf dynamic library", "failed")("error", "last load failed"));
            LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "cannot load ebpf dynamic library");
            return false;
        }
        return true;
    }
    LOG_INFO(sLogger, ("load ebpf dynamic library", "begin"));
    // load ebpf lib
    std::string dlPrefix = GetProcessExecutionDir();
    soPath = dlPrefix + "libebpf.so";
    if (kernelVersion < INT64_FLAG(sls_observer_ebpf_min_kernel_version)) {
        fsutil::PathStat buf;
        // overlayfs has a reference bug in low kernel version, so copy docker inner file to host path to avoid using
        // overlay fs. detail: https://lore.kernel.org/lkml/20180228004014.445-1-hmclauchlan@fb.com/
        if (fsutil::PathStat::stat(STRING_FLAG(default_container_host_path).c_str(), buf)) {
            std::string cmd
                = std::string("\\cp ").append(soPath).append(" ").append(GetObserverEbpfHostPath());
            LOG_INFO(sLogger, ("invoke cp cmd:", cmd));
            system(std::string("mkdir ").append(GetObserverEbpfHostPath()).c_str());
            system(cmd.c_str());
            dlPrefix = GetObserverEbpfHostPath();
            soPath = GetObserverEbpfHostPath() + "libebpf.so";
        }
    }
    LOG_INFO(sLogger, ("load ebpf, libebpf path", soPath));
    mEBPFLib = new DynamicLibLoader;
    std::string loadErr;
    if (!mEBPFLib->LoadDynLib("ebpf", loadErr, dlPrefix)) {
        LOG_ERROR(sLogger, ("load ebpf dynamic library path", soPath)("error", loadErr));
        return false;
    }
    LOAD_EBPF_FUNC(ebpf_setup_net_data_process_func)
    LOAD_EBPF_FUNC(ebpf_setup_net_event_process_func)
    LOAD_EBPF_FUNC(ebpf_setup_net_statistics_process_func)
    LOAD_EBPF_FUNC(ebpf_setup_net_lost_func)
    LOAD_EBPF_FUNC(ebpf_setup_print_func)
    LOAD_EBPF_FUNC(ebpf_config)
    LOAD_EBPF_FUNC(ebpf_poll_events)
    LOAD_EBPF_FUNC(ebpf_init)
    LOAD_EBPF_FUNC(ebpf_start)
    LOAD_EBPF_FUNC(ebpf_stop)
    LOAD_EBPF_FUNC(ebpf_get_fd)
    LOAD_EBPF_FUNC(ebpf_get_next_key)
    LOAD_EBPF_FUNC(ebpf_delete_map_value)
    LOAD_EBPF_FUNC(ebpf_cleanup_dog)
    LOAD_EBPF_FUNC(ebpf_update_conn_addr)
    LOAD_EBPF_FUNC(ebpf_disable_process)
    LOAD_EBPF_FUNC(ebpf_update_conn_role)
    LOG_INFO(sLogger, ("load ebpf dynamic library", "success"));
    return true;
}

bool EBPFWrapper::Init(std::function<int(StringPiece)> processor) {
    if (mInitSuccess) {
        return true;
    }
    LOG_INFO(sLogger, ("init ebpf source", "begin"));
    int64_t kernelVersion;
    std::string kernelRelease;
    if (!isSupportedOS(kernelVersion, kernelRelease)) {
        LOG_ERROR(sLogger, ("init ebpf source", "failed")("reason", "not supported kernel or OS"));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "not supported kernel or OS:" + kernelRelease);
        return false;
    }

    std::string btfPath = "/sys/kernel/btf/vmlinux";
    if (kernelVersion < INT64_FLAG(sls_observer_ebpf_nobtf_kernel_version)) {
        btfPath = GetValidBTFPath(kernelVersion, kernelRelease);
        if (btfPath.empty()) {
            LOG_ERROR(sLogger, ("not found any btf files", kernelRelease));
            LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "not found any btf files:" + kernelRelease);
            return false;
        }
    }


    std::string soPath;
    if (!loadEbpfLib(kernelVersion, soPath)) {
        return false;
    }
    mPacketProcessor = processor;

    g_ebpf_setup_print_func_func(ebpf_print_callback);
    g_ebpf_setup_net_event_process_func_func(ebpf_ctrl_process_callback, this);
    g_ebpf_setup_net_data_process_func_func(ebpf_data_process_callback, this);
    g_ebpf_setup_net_statistics_process_func_func(ebpf_stat_process_callback, this);
    g_ebpf_setup_net_lost_func_func(ebpf_lost_callback, this);

    long cleanup_offset = LOAD_UPROBE_OFFSET(g_ebpf_cleanup_dog_func);
    long update_conn_offset = LOAD_UPROBE_OFFSET(g_ebpf_update_conn_addr_func);
    long disable_process_offset = LOAD_UPROBE_OFFSET(g_ebpf_disable_process_func);
    long update_role_offset = LOAD_UPROBE_OFFSET(g_ebpf_update_conn_role_func);
    if (cleanup_offset == LOAD_UPROBE_OFFSET_FAIL || update_conn_offset == LOAD_UPROBE_OFFSET_FAIL
        || disable_process_offset == LOAD_UPROBE_OFFSET_FAIL || update_role_offset == LOAD_UPROBE_OFFSET_FAIL) {
        return false;
    }

    int err = g_ebpf_init_func(&btfPath.at(0),
                               static_cast<int32_t>(btfPath.size()),
                               &soPath.at(0),
                               static_cast<int32_t>(soPath.size()),
                               cleanup_offset,
                               update_conn_offset,
                               disable_process_offset,
                               update_role_offset);
    if (err) {
        LOG_ERROR(sLogger, ("init ebpf", "failed")("error", "ebpf_init func failed"));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_INIT_ALARM, "ebpf_init func failed");
        return false;
    }
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t nowTime = GetCurrentTimeInNanoSeconds();
    mDeltaTimeNs = nowTime - (uint64_t)ts.tv_sec * 1000000000ULL - (uint64_t)ts.tv_nsec;
    set_ebpf_int_config((int32_t)PROTOCOL_FILTER, 0, -1);
    set_ebpf_int_config((int32_t)TGID_FILTER, 0, mConfig->mEBPFPid);
    set_ebpf_int_config((int32_t)SELF_FILTER, 0, getpid());
    set_ebpf_int_config((int32_t)DATA_SAMPLING, 0, mConfig->mSampling);
    set_ebpf_int_config((int32_t)PERF_BUFFER_PAGE, (int32_t)DATA_HAND, 512);
    LOG_INFO(sLogger, ("init ebpf source", "success"));
    mInitSuccess = true;
    return true;
}

bool EBPFWrapper::Start() {
    if (!mInitSuccess) {
        return false;
    }
    if (mStartSuccess) {
        return true;
    }
    set_ebpf_int_config((int32_t)PROTOCOL_FILTER, 0, mConfig->mProtocolProcessFlag);
    int err = g_ebpf_start_func == NULL ? -100 : g_ebpf_start_func();
    if (err) {
        LOG_ERROR(sLogger, ("start ebpf", "failed")("error", err));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_RUNTIME_ALARM,
                                               "start ebpf failed, error: " + std::to_string(err));
        this->Stop();
        return false;
    }
    LOG_INFO(sLogger, ("start ebpf source", "success"));
    mStartSuccess = true;
    return true;
}

bool EBPFWrapper::Stop() {
    CleanAllDisableProcesses();
    mStartSuccess = false;
    return true;
}

int32_t EBPFWrapper::ProcessPackets(int32_t maxProcessPackets, int32_t maxProcessDurationMs) {
    if (g_ebpf_poll_events_func == nullptr || !mStartSuccess) {
        return -1;
    }
    auto res = g_ebpf_poll_events_func(maxProcessPackets, &this->holdOnFlag);
    if (res < 0 && res != -100) {
        LOG_ERROR(sLogger, ("pull ebpf events", "failed")("error", "unknown polling result")("result", res));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_RUNTIME_ALARM,
                                               "pull ebpf events, error: " + std::to_string(res));
    }
    return res;
}

uint32_t EBPFWrapper::ConvertConnIdToSockHash(struct connect_id_t* id) {
    return XXH32(id, sizeof(struct connect_id_t), 0);
}


void EBPFWrapper::GetAllConnections(std::vector<struct connect_id_t>& connIds) {
    if (!mStartSuccess || g_ebpf_get_fd_func == NULL) {
        return;
    }
    int32_t fd = g_ebpf_get_fd_func();
    struct connect_id_t key = {};
    struct connect_id_t next_key;
    while (g_ebpf_get_next_key_func(fd, &key, &next_key) == 0) {
        connIds.push_back(key);
        key = next_key;
    }
}

void EBPFWrapper::DeleteInvalidConnections(const std::vector<struct connect_id_t>& connIds) {
    if (!mStartSuccess || g_ebpf_delete_map_value_func == NULL) {
        return;
    }
    const size_t maxDeleteCount = 64;
    for (size_t i = 0; i < connIds.size(); i += maxDeleteCount) {
        size_t endIndex = std::min(connIds.size(), i + maxDeleteCount);
        g_ebpf_delete_map_value_func((void*)&connIds[i], endIndex - i);
    }
}

SocketCategory EBPFWrapper::ConvertSockAddress(union sockaddr_t& from,
                                               struct connect_id_t& connectId,
                                               SockAddress& addr,
                                               uint16_t& port) {
    static auto sConnManager = ConnectionMetaManager::GetInstance();
    if (from.sa.sa_family == AF_INET) {
        addr.Type = SockAddressType_IPV4;
        addr.Addr.IPV4 = from.in4.sin_addr.s_addr;
        port = ntohs(from.in4.sin_port);
        return SocketCategory::InetSocket;
    } else if (from.sa.sa_family == AF_INET6) {
        addr.Type = SockAddressType_IPV6;
        addr.Addr.IPV6[0] = ((uint64_t*)&from.in6.sin6_addr)[0];
        addr.Addr.IPV6[1] = ((uint64_t*)&from.in6.sin6_addr)[1];
        port = ntohs(from.in6.sin6_port);
        return SocketCategory::InetSocket;
    } else if (from.sa.sa_family == AF_UNIX) {
        addr.Type = SockAddressType_IPV4;
        addr.Addr.IPV4 = 0;
        port = 0;
        return SocketCategory::UnixSocket;
    } else {
        auto info = sConnManager->GetConnectionInfo(connectId.tgid, connectId.fd);
        if (info != nullptr) {
            if (info->family == AF_UNIX) {
                addr.Type = SockAddressType_IPV4;
                addr.Addr.IPV4 = 0;
                port = 0;
                union sockaddr_t socketAddr = {};
                socketAddr.sa.sa_family = AF_UNIX;
                // update unix connection info family and update sample status to ignore unnecessary data processing.
                g_ebpf_update_conn_addr_func(&connectId, &socketAddr, 0, this->mConfig->mDropUnixSocket);
                return SocketCategory::UnixSocket;
            } else if (info->family == AF_INET) {
                addr.Type = SockAddressType_IPV4;
                addr = info->remoteAddr;
                port = info->remotePort;

                union sockaddr_t socketAddr = {};
                socketAddr.sa.sa_family = AF_INET;
                socketAddr.in4.sin_port = htons(info->remotePort);
                socketAddr.in4.sin_addr.s_addr = info->remoteAddr.Addr.IPV4;
                g_ebpf_update_conn_addr_func(&connectId, &socketAddr, 0, false);
                return SocketCategory::InetSocket;
            } else if (info->family == AF_INET6) {
                addr.Type = SockAddressType_IPV6;
                addr = info->remoteAddr;
                port = info->remotePort;

                union sockaddr_t socketAddr = {};
                socketAddr.in6.sin6_addr = in6addr_any;
                socketAddr.sa.sa_family = AF_INET6;
                socketAddr.in6.sin6_port = htons(info->remotePort);
                ((uint64_t*)&socketAddr.in6.sin6_addr)[0] = info->remoteAddr.Addr.IPV6[0];
                ((uint64_t*)&socketAddr.in6.sin6_addr)[1] = info->remoteAddr.Addr.IPV6[1];
                g_ebpf_update_conn_addr_func(&connectId, &socketAddr, 0, false);
                return SocketCategory::InetSocket;
            }
        }
        addr.Type = SockAddressType_IPV4;
        addr.Addr.IPV4 = 0;
        port = 0;
        return SocketCategory::UnknownSocket;
    }
}

SocketCategory EBPFWrapper::ConvertDataToPacketHeader(struct conn_data_event_t* event, PacketEventHeader* header) {
    header->PID = event->attr.conn_id.tgid;
    header->SockHash = ConvertConnIdToSockHash(&(event->attr.conn_id));
    header->EventType = PacketEventType_Data;
    header->SrcAddr.Type = SockAddressType_IPV4;
    header->SrcAddr.Addr.IPV4 = 0;
    header->SrcPort = 0;
    header->RoleType = DetectRole(event->attr.role, event->attr.conn_id, header->DstPort);
    return ConvertSockAddress(event->attr.addr, event->attr.conn_id, header->DstAddr, header->DstPort);
}

PacketRoleType EBPFWrapper::DetectRole(enum support_role_e& kernelDetectRole,
                                       struct connect_id_t& connectId,
                                       uint16_t& remotePort) const {
    static auto sConnManager = ConnectionMetaManager::GetInstance();
    if (kernelDetectRole != IsUnknown) {
        auto role = kernelDetectRole == IsClient ? PacketRoleType::Client : PacketRoleType::Server;
        LOG_TRACE(sLogger, ("role_type_detected_by_kernel", PacketRoleTypeToString(role)));
        return role;
    }
    auto info = sConnManager->GetConnectionInfo(connectId.tgid, connectId.fd);
    if (info != nullptr && (info->family == AF_INET || info->family == AF_INET6)) {
        g_ebpf_update_conn_role_func(&connectId,
                                     info->role == PacketRoleType::Client       ? IsClient
                                         : info->role == PacketRoleType::Server ? IsServer
                                                                                : IsUnknown);
        LOG_TRACE(sLogger, ("role_type_detected_by_netlink", PacketRoleTypeToString(info->role)));
        return info->role;
    }
    return PacketRoleType::Unknown;
}


bool EBPFWrapper::ConvertCtrlToPacketHeader(struct conn_ctrl_event_t* event, PacketEventHeader* header) {
    header->PID = event->conn_id.tgid;
    header->SockHash = ConvertConnIdToSockHash(&(event->conn_id));
    header->EventType = event->type == EventConnect ? PacketEventType_Connected : PacketEventType_Closed;
    // local address always set to 0.0.0.0
    header->SrcAddr.Type = SockAddressType_IPV4;
    header->SrcAddr.Addr.IPV4 = 0;
    header->SrcPort = 0;
    if (event->type == EventConnect) {
        ConvertSockAddress(event->connect.addr, event->conn_id, header->DstAddr, header->DstPort);
    } else {
        header->DstAddr.Type = SockAddressType_IPV4;
        header->DstAddr.Addr.IPV4 = 0;
        header->DstPort = 0;
    }
    return true;
}

void EBPFWrapper::OnData(struct conn_data_event_t* event) {
    if (event->attr.msg_buf_size > CONN_DATA_MAX_SIZE) {
        LOG_WARNING(sLogger, ("module", "ebpf")("error", "invalid ebpf data event")("size", event->attr.msg_buf_size));
        LogtailAlarm::GetInstance()->SendAlarm(OBSERVER_RUNTIME_ALARM,
                                               "invalid ebpf data event because the size is over "
                                                   + std::to_string(CONN_DATA_MAX_SIZE));
        return;
    }
    // convert data event to PacketEvent
    auto* header = (PacketEventHeader*)(&mPacketDataBuffer.at(0));
    SocketCategory socketCategory = ConvertDataToPacketHeader(event, header);
    EBPF_CONNECTION_FILTER(socketCategory, header->DstAddr, event->attr.conn_id, event->attr.addr);
    header->TimeNano = event->attr.ts + mDeltaTimeNs;
    PacketEventData* data = (PacketEventData*)(&mPacketDataBuffer.at(0) + sizeof(PacketEventHeader));
    data->PktType = (PacketType)event->attr.direction;
    data->PtlType = (ProtocolType)event->attr.protocol;
    data->BufferLen = event->attr.msg_buf_size;
    data->RealLen = event->attr.org_msg_size;
    if (header->RoleType == PacketRoleType::Unknown) {
        data->MsgType = InferRequestOrResponse(data->PktType, header);
        header->RoleType = InferServerOrClient(data->PktType, data->MsgType);
        LOG_TRACE(sLogger, ("role_type_detected_by_guess", PacketRoleTypeToString(header->RoleType)));
        LOG_TRACE(sLogger, ("message_type_detected_by_guess", MessageTypeToString(data->MsgType)));
        g_ebpf_update_conn_role_func(&event->attr.conn_id,
                                     header->RoleType == PacketRoleType::Client       ? IsClient
                                         : header->RoleType == PacketRoleType::Server ? IsServer
                                                                                      : IsUnknown);
    } else if (event->attr.direction == DirIngress) {
        data->MsgType = header->RoleType == PacketRoleType::Client ? MessageType_Response : MessageType_Request;
        LOG_TRACE(sLogger, ("message_type_detected_success", MessageTypeToString(data->MsgType)));
    } else {
        data->MsgType = header->RoleType == PacketRoleType::Client ? MessageType_Request : MessageType_Response;
        LOG_TRACE(sLogger, ("message_type_detected_success", MessageTypeToString(data->MsgType)));
    }
    LOG_TRACE(
        sLogger,
        ("data event, addr", SockAddressToString(header->DstAddr))("port", header->DstPort)(
            "family", event->attr.addr.sa.sa_family)("pid", event->attr.conn_id.tgid)("fd", event->attr.conn_id.fd)(
            "protocol", event->attr.protocol)("length_header", event->attr.length_header)(
            "role", PacketRoleTypeToString(header->RoleType))("ori_role", std::to_string(event->attr.role))(
            "ori_msg", MessageTypeToString((MessageType)event->attr.type))("msg", MessageTypeToString(data->MsgType))(
            "data", charToHexString(event->msg, event->attr.msg_buf_size, event->attr.msg_buf_size))(
            "hash", header->SockHash)("raw_data", std::string(event->msg, event->attr.msg_buf_size))

    );
    // Kafka protocol would first read 4Byte data, so kernel use try_to_prepend to identify this condition.
    // But this operation is an optional choose for Mysql protocol. So, MySQL protocol would also append 4Bytes
    // when the length_header is positive number.
    if (event->attr.try_to_prepend || isAppendMySQLMsg(event)) {
        data->Buffer = &mPacketDataBuffer.at(0) + sizeof(PacketEventHeader) + sizeof(PacketEventData);
        data->BufferLen = event->attr.msg_buf_size + 4;
        data->RealLen = event->attr.org_msg_size + 4;
        *(uint32_t*)data->Buffer = event->attr.length_header;
        memcpy(data->Buffer + 4, event->msg, event->attr.msg_buf_size);
        LOG_TRACE(sLogger, ("data event append data", charToHexString(data->Buffer, data->BufferLen, data->BufferLen)));
    } else {
        data->Buffer = event->msg;
        data->BufferLen = event->attr.msg_buf_size;
        data->RealLen = event->attr.org_msg_size;
    }
    if (mPacketProcessor) {
        mPacketProcessor(StringPiece(mPacketDataBuffer.data(), sizeof(PacketEventHeader) + sizeof(PacketEventData)));
    }
}

void EBPFWrapper::OnCtrl(struct conn_ctrl_event_t* event) {
    PacketEventHeader* header = (PacketEventHeader*)(&mPacketDataBuffer.at(0));
    header->TimeNano = event->ts + mDeltaTimeNs;
    ConvertCtrlToPacketHeader(event, header);
    if (mPacketProcessor) {
        mPacketProcessor(StringPiece(mPacketDataBuffer.data(), sizeof(PacketEventHeader)));
    }
}

void EBPFWrapper::OnStat(struct conn_stats_event_t* event) {
    logtail::NetStatisticsKey key;
    key.SockCategory
        = ConvertSockAddress(event->addr, event->conn_id, key.AddrInfo.RemoteAddr, key.AddrInfo.RemotePort);
    EBPF_CONNECTION_FILTER(key.SockCategory, key.AddrInfo.RemoteAddr, event->conn_id, event->addr);
    key.PID = event->conn_id.tgid;
    key.SockHash = ConvertConnIdToSockHash(&(event->conn_id));
    key.AddrInfo.LocalPort = 0;
    key.AddrInfo.LocalAddr.Type = SockAddressType_IPV4;
    key.AddrInfo.LocalAddr.Addr.IPV4 = 0;
    key.RoleType = DetectRole(event->role, event->conn_id, key.AddrInfo.RemotePort);
    LOG_TRACE(sLogger,
              ("receive stat event,addr", SockAddressToString(key.AddrInfo.RemoteAddr))(
                  "family", event->addr.sa.sa_family)("socket_type", SocketCategoryToString(key.SockCategory))(
                  "port", key.AddrInfo.RemotePort)("pid", key.PID)("fd", event->conn_id.fd));
    NetStatisticsTCP& item = mStatistics.GetStatisticsItem(key);
    item.Base.SendBytes += event->wr_bytes - event->last_output_wr_bytes;
    item.Base.RecvBytes += event->rd_bytes - event->last_output_rd_bytes;
    item.Base.SendPackets += event->wr_pkts - event->last_output_wr_pkts;
    item.Base.RecvPackets += event->wr_pkts - event->last_output_wr_pkts;
}

void EBPFWrapper::OnLost(enum callback_type_e type, uint64_t count) {
    LOG_DEBUG(sLogger, ("module", "ebpf")("lost event type", (int)type)("count", count));
    static auto sMetrics = NetworkStatistic::GetInstance();
    sMetrics->mEbpfLostCount += count;
}

bool EBPFWrapper::isAppendMySQLMsg(conn_data_event_t* pEvent) {
    if (pEvent->attr.protocol != ProtoMySQL) {
        return false;
    }
    if ((pEvent->attr.length_header & 0x00ffffff) != pEvent->attr.org_msg_size) {
        return false;
    }
    if (pEvent->attr.msg_buf_size > 4) {
        uint32_t len = (*((uint8_t*)(pEvent->msg + 2)) << 16) + (*((uint8_t*)(pEvent->msg + 1)) << 8)
            + (*((uint8_t*)(pEvent->msg)));
        if (len == pEvent->attr.org_msg_size) {
            return false;
        }
    }
    return true;
}

void EBPFWrapper::DisableProcess(uint32_t pid) {
    if (!mStartSuccess) {
        return;
    }
    if (this->mDisabledProcesses.find(pid) != this->mDisabledProcesses.end()) {
        return;
    }
    uint64_t startTime = readStat(pid);
    if (startTime == 0) {
        return;
    }
    this->mDisabledProcesses[pid] = startTime;
    LOG_DEBUG(sLogger, ("insert disable process", pid));
    g_ebpf_disable_process_func(pid, false);
}

uint64_t EBPFWrapper::readStat(uint64_t pid) {
    std::string statPath = std::string("/proc/").append(std::to_string(pid)).append("/stat");
    std::string content;
    if (!ReadFileContent(statPath, content, 1024)) {
        return 0;
    }
    if (content.empty()) {
        return 0;
    }
    int count = 0;
    int startIndex = 0, endIndex = 0;
    for (size_t i = 0; i < content.length(); ++i) {
        if (content[i] == ' ') {
            count++;
        }
        if (count == 21) {
            startIndex = i + 1;
            continue;
        }
        if (count == 22) {
            endIndex = i;
            break;
        }
    }
    if (startIndex == 0 || endIndex == 0) {
        return 0;
    }
    return std::strtol(content.c_str() + startIndex, NULL, 10);
}

void EBPFWrapper::ProbeProcessStat() {
    if (!mStartSuccess) {
        return;
    }
    std::stringstream ss;
    for (auto item = this->mDisabledProcesses.begin(); item != this->mDisabledProcesses.end();) {
        uint64_t startTime = readStat(item->first);
        if (startTime == 0 || item->second != startTime) {
            if (sLogger->should_log(spdlog::level::debug)) {
                ss << item->first << ", ";
            }
            g_ebpf_disable_process_func(item->first, true);
            item = this->mDisabledProcesses.erase(item);
        } else {
            item++;
        }
    }
    LOG_DEBUG(sLogger,
              ("ebpf disable processes size after probe", mDisabledProcesses.size())("recover processes", ss.str()));
}

void EBPFWrapper::CleanAllDisableProcesses() {
    if (!mStartSuccess) {
        return;
    }
    std::stringstream ss;
    for (auto item = this->mDisabledProcesses.begin(); item != this->mDisabledProcesses.end();) {
        if (sLogger->should_log(spdlog::level::debug)) {
            ss << item->first << ", ";
        }
        g_ebpf_disable_process_func(item->first, true);
        item = this->mDisabledProcesses.erase(item);
    }
    LOG_DEBUG(sLogger,
              ("ebpf disable processes size after clean", mDisabledProcesses.size())("recover processes", ss.str()));
}

int32_t EBPFWrapper::GetDisablesProcessCount() {
    if (!mStartSuccess) {
        return 0;
    }
    return mDisabledProcesses.size();
}
void EBPFWrapper::HoldOn(bool exitFlag) {
    holdOnFlag = 1;
    if (exitFlag) {
        int err = g_ebpf_stop_func == NULL ? -100 : g_ebpf_stop_func();
        if (err) {
            LOG_INFO(sLogger, ("stop ebpf", "failed")("error", err));
        }
    }
}
} // namespace logtail
