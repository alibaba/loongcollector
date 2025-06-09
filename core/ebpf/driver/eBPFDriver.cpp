// Copyright 2025 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <mutex>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <coolbpf/security.skel.h>
#pragma GCC diagnostic pop

#include "ebpf/include/export.h"

extern "C" {
#include <coolbpf/net.h>
#include <coolbpf/security/bpf_process_event_type.h>
#include <coolbpf/security/data_msg.h>
#include <coolbpf/security/msg_type.h>
#include <sys/resource.h>

#include "ebpf/driver/eBPFDriver.h"
}

#include "BPFWrapper.h"
// #include "FileFilter.h"
#include "Log.h"
#include "NetworkFilter.h"
#include "common/magic_enum.hpp"

#ifdef ENABLE_COMPATIBLE_MODE
extern "C" {
#include <string.h>
asm(".symver memcpy, memcpy@GLIBC_2.2.5");
void* __wrap_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}
}
#endif

int set_logger(logtail::ebpf::eBPFLogHandler fn) {
    set_log_handler(fn);
    return 0;
}

int bump_memlock_rlimit(void) {
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (0 != setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        return -1;
    }
    return 0;
}

std::mutex gPbMtx;
std::array<std::vector<void*>, size_t(logtail::ebpf::PluginType::MAX)> gPluginPbs;
std::array<std::atomic_bool, size_t(logtail::ebpf::PluginType::MAX)> gPluginStatus = {};

std::array<std::vector<std::string>, size_t(logtail::ebpf::PluginType::MAX)> gPluginCallNames;

void UpdatePluginPerfBuffers(logtail::ebpf::PluginType type, std::vector<void*> pbs) {
    std::lock_guard lk(gPbMtx);
    gPluginPbs[int(type)] = pbs;
}

std::shared_ptr<logtail::ebpf::BPFWrapper<security_bpf>> gWrapper = logtail::ebpf::BPFWrapper<security_bpf>::Create();
void SetCoolBpfConfig(int32_t opt, int32_t value) {
    int32_t* params[] = {&value};
    int32_t paramsLen[] = {4};
    ebpf_config(opt, 0, 1, (void**)params, paramsLen);
}

void set_networkobserver_cid_filter(const char* container_id, size_t length, bool update) {
    ebpf_set_cid_filter(container_id, length, update);
}

void set_networkobserver_config(int32_t opt, int32_t value) {
    SetCoolBpfConfig(opt, value);
}

int SetupPerfBuffers(logtail::ebpf::PluginConfig* arg) {
    std::vector<logtail::ebpf::PerfBufferSpec> specs;
    switch (arg->mPluginType) {
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     auto* cc = std::get_if<logtail::ebpf::FileSecurityConfig>(&arg->mConfig);
        //     if (cc) {
        //         specs = cc->mPerfBufferSpec;
        //     }
        //     break;
        // }
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            auto* cc = std::get_if<logtail::ebpf::ProcessConfig>(&arg->mConfig);
            if (cc) {
                specs = cc->mPerfBufferSpec;
            }
            break;
        }
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            auto* cc = std::get_if<logtail::ebpf::NetworkSecurityConfig>(&arg->mConfig);
            if (cc) {
                specs = cc->mPerfBufferSpec;
            }
            break;
        }
        case logtail::ebpf::PluginType::NETWORK_OBSERVE:
        default:
            return kErrDriverInternal;
    }
    auto config = arg->mConfig;
    // create pb and set perf buffer meta
    if (specs.size()) {
        std::vector<logtail::ebpf::PerfBufferOps> perfBuffers;
        std::vector<void*> pbs;
        for (auto& spec : specs) {
            void* pb = gWrapper->CreatePerfBuffer(spec.mName,
                                                  spec.mSize,
                                                  spec.mCtx,
                                                  static_cast<perf_buffer_sample_fn>(spec.mSampleHandler),
                                                  static_cast<perf_buffer_lost_fn>(spec.mLostHandler));
            if (!pb) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "plugin type:%s: create perfbuffer fail, name:%s, size:%ld\n",
                         magic_enum::enum_name(arg->mPluginType).data(),
                         spec.mName.c_str(),
                         spec.mSize);
                return kErrDriverInternal;
            }
            pbs.push_back(pb);
        }
        UpdatePluginPerfBuffers(arg->mPluginType, pbs);
    }
    return 0;
}

int start_plugin(logtail::ebpf::PluginConfig* arg) {
    // 1. load skeleton
    // 2. start consumer
    // 3. attach prog
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "enter start_plugin, arg is null: %d \n", arg == nullptr);
    if (0 != bump_memlock_rlimit()) {
        return -1;
    }

    switch (arg->mPluginType) {
        case logtail::ebpf::PluginType::NETWORK_OBSERVE: {
            auto* config = std::get_if<logtail::ebpf::NetworkObserveConfig>(&arg->mConfig);
            ebpf_setup_print_func(config->mLogHandler);
            ebpf_setup_net_event_process_func(config->mCtrlHandler, config->mCustomCtx);
            ebpf_setup_net_data_process_func(config->mDataHandler, config->mCustomCtx);
            ebpf_setup_net_statistics_process_func(config->mStatsHandler, config->mCustomCtx);
            ebpf_setup_net_lost_func(config->mLostHandler, config->mCustomCtx);

            int err = ebpf_init(nullptr,
                                0,
                                config->mSo.data(),
                                static_cast<int32_t>(config->mSo.length()),
                                config->mUprobeOffset,
                                config->mUpcaOffset,
                                config->mUppsOffset,
                                config->mUpcrOffset);
            if (err) {
                ebpf_log(
                    logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "network observe: ebpf_init fail ret:%d\n", err);
                return err;
            }
            // config
            SetCoolBpfConfig((int32_t)PROTOCOL_FILTER, 1);
            SetCoolBpfConfig((int32_t)TGID_FILTER, -1);
            SetCoolBpfConfig((int32_t)PORT_FILTER, -1);
            SetCoolBpfConfig((int32_t)SELF_FILTER, getpid());
            SetCoolBpfConfig((int32_t)DATA_SAMPLING, 100);

            // TODO
            if (config->mEnableCidFilter) {
                if (config->mCidOffset <= 0) {
                    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "offset invalid!! skip cid filter... offset %d\n",
                             config->mCidOffset);
                }
                SetCoolBpfConfig((int32_t)CONTAINER_ID_FILTER, config->mCidOffset);
            }
            //
            err = ebpf_start();
            if (err) {
                ebpf_log(
                    logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "network observe: ebpf_start fail ret:%d\n", err);
                return err;
            }
            break;
        }
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     auto* config = std::get_if<logtail::ebpf::FileSecurityConfig>(&arg->mConfig);

        //     int ret = 0;
        //     ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "begin to set tail call\n");
        //     // setup tail call
        //     ret = gWrapper->SetTailCall("secure_tailcall_map", {"filter_prog", "secure_data_send"});
        //     if (ret) {
        //         ebpf_log(
        //             logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "file security: SetTailCall fail ret:%d\n", ret);
        //         return ret;
        //     }

        //     // setup pb
        //     ret = SetupPerfBuffers(arg);
        //     if (ret) {
        //         ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                  "file security: setup perfbuffer fail ret:%d\n",
        //                  ret);
        //         return ret;
        //     }

        //     // update filter config
        //     std::vector<logtail::ebpf::AttachProgOps> attach_ops;
        //     for (const auto& opt : config->mOptions) {
        //         for (const auto& cn : opt.mCallNames) {
        //             attach_ops.emplace_back("kprobe_" + cn, true);
        //             gPluginCallNames[int(arg->mPluginType)].push_back(cn);
        //             int ret = logtail::ebpf::CreateFileFilterForCallname(gWrapper, cn, opt.mFilter);
        //             if (ret) {
        //                 ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                          "[start_plugin] Failed to create filter for callname %s\n",
        //                          cn.c_str());
        //                 return kErrDriverInternal;
        //             }
        //         }
        //     }
        //     // dynamic instrument
        //     ret = gWrapper->DynamicAttachBPFObject(attach_ops);
        //     if (ret) {
        //         ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                  "file security: DynamicAttachBPFObject fail\n");
        //         return kErrDriverInternal;
        //     }
        //     ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
        //              "file security: DynamicAttachBPFObject success\n");
        //     break;
        // }
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            auto* config = std::get_if<logtail::ebpf::NetworkSecurityConfig>(&arg->mConfig);

            int ret = 0;
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG, "begin to set tail call\n");
            // set tail call
            ret = gWrapper->SetTailCall("secure_tailcall_map", {"filter_prog", "secure_data_send"});
            if (ret != 0) {
                ebpf_log(
                    logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "network security: SetTailCall fail ret:%d\n", ret);
                return ret;
            }

            // setup pb
            ret = SetupPerfBuffers(arg);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "network security: setup perfbuffer fail ret:%d\n",
                         ret);
                return ret;
            }

            // update filter config
            std::vector<logtail::ebpf::AttachProgOps> attach_ops;
            for (const auto& opt : config->mOptions) {
                for (const auto& cn : opt.mCallNames) {
                    attach_ops.emplace_back("kprobe_" + cn, true);
                    gPluginCallNames[int(arg->mPluginType)].push_back(cn);
                    int ret = logtail::ebpf::CreateNetworkFilterForCallname(gWrapper, cn, opt.mFilter);
                    if (ret) {
                        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                                 "[start_plugin] Failed to create filter for callname %s\n",
                                 cn.c_str());
                        return kErrDriverInternal;
                    }
                }
            }
            // dynamic instrument
            ret = gWrapper->DynamicAttachBPFObject(attach_ops);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "network security: DynamicAttachBPFObject fail\n");
                return kErrDriverInternal;
            }
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "network security: DynamicAttachBPFObject success\n");
            break;
        }
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            int err = gWrapper->Init();
            if (err) {
                ebpf_log(
                    logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "process security: ebpf_init fail ret:%d\n", err);
                return err;
            }
            // auto* config = std::get_if<logtail::ebpf::ProcessConfig>(&arg->mConfig);
            std::vector<logtail::ebpf::AttachProgOps> attachOps = {
                logtail::ebpf::AttachProgOps("event_exit_acct_process", true),
                logtail::ebpf::AttachProgOps("event_wake_up_new_task", true),
                logtail::ebpf::AttachProgOps("event_exit_disassociate_ctty", true),
                logtail::ebpf::AttachProgOps("event_execve", true),
                logtail::ebpf::AttachProgOps("execve_rate", false),
                logtail::ebpf::AttachProgOps("execve_send", false),
                logtail::ebpf::AttachProgOps("filter_prog", false),
            };

            int ret = 0;
            std::vector<std::pair<const std::string, const std::vector<std::string>>> tailCalls
                = {{"execve_calls", {"execve_rate", "execve_send"}}};

            // set tail call
            for (auto& tailCall : tailCalls) {
                auto ret = gWrapper->SetTailCall(tailCall.first, tailCall.second);
                if (ret != 0) {
                    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "process security: SetTailCall fail ret:%d\n",
                             ret);
                    return ret;
                }
            }

            // setup pb
            ret = SetupPerfBuffers(arg);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "process security: setup perfbuffer fail ret:%d\n",
                         ret);
                return ret;
            }

            // attach bpf object
            ret = gWrapper->DynamicAttachBPFObject(attachOps);
            if (ret != 0) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "process security: DynamicAttachBPFObject fail ret:%d\n",
                         ret);
                return kErrDriverInternal;
            }
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "process security: DynamicAttachBPFObject success\n");
            break;
        }
        default: {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[start plugin] unknown plugin type, please check. \n");
        }
    }
    gPluginStatus[int(arg->mPluginType)] = true;
    return 0;
}

int poll_plugin_pbs(logtail::ebpf::PluginType type, int32_t max_events, int32_t* stop_flag, int timeout_ms) {
    if (!gPluginStatus[int(type)]) {
        return 0;
    }

    if (type == logtail::ebpf::PluginType::NETWORK_OBSERVE) {
        return ebpf_poll_events(max_events, stop_flag, timeout_ms);
    }

    std::lock_guard lk(gPbMtx);
    // find pbs
    auto& pbs = gPluginPbs[int(type)];
    if (pbs.empty()) {
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "no pbs registered for type:%d \n", type);
        return -1;
    }
    int cnt = 0;
    for (auto& x : pbs) {
        if (!x) {
            continue;
        }
        int ret = gWrapper->PollPerfBuffer(x, max_events, timeout_ms);
        if (ret < 0 && errno != EINTR) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN, "poll perf buffer failed ...\n");
        } else {
            cnt += ret;
        }
    }
    return cnt;
}

// deprecated
int resume_plugin(logtail::ebpf::PluginConfig* arg) {
    switch (arg->mPluginType) {
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     auto* config = std::get_if<logtail::ebpf::FileSecurityConfig>(&arg->mConfig);
        //     int ret = 0;
        //     // update filter config
        //     std::vector<logtail::ebpf::AttachProgOps> attachOps;
        //     for (const auto& opt : config->mOptions) {
        //         for (const auto& cn : opt.mCallNames) {
        //             attachOps.emplace_back("kprobe_" + cn, true);
        //             gPluginCallNames[int(arg->mPluginType)].push_back(cn);
        //         }
        //     }
        //     // dynamic instrument
        //     ret = gWrapper->DynamicAttachBPFObject(attachOps);
        //     if (ret) {
        //         ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                  "file security: DynamicAttachBPFObject fail\n");
        //         return kErrDriverInternal;
        //     }
        //     ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
        //              "file security: DynamicAttachBPFObject success\n");
        //     break;
        // }
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            auto* config = std::get_if<logtail::ebpf::NetworkSecurityConfig>(&arg->mConfig);
            int ret = 0;
            // update filter config
            std::vector<logtail::ebpf::AttachProgOps> attachOps;
            for (const auto& opt : config->mOptions) {
                for (const auto& cn : opt.mCallNames) {
                    attachOps.emplace_back("kprobe_" + cn, true);
                    gPluginCallNames[int(arg->mPluginType)].push_back(cn);
                }
            }
            // dynamic instrument
            ret = gWrapper->DynamicAttachBPFObject(attachOps);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "network security: DynamicAttachBPFObject fail\n");
                return kErrDriverInternal;
            }
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "network security: DynamicAttachBPFObject success\n");
            break;
        }
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            break;
        }
        default: {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[resume plugin] unknown plugin type, please check. \n");
        }
    }
    gPluginStatus[int(arg->mPluginType)] = true;
    return 0;
}

// just update config ...
int update_plugin(logtail::ebpf::PluginConfig* arg) {
    auto pluginType = arg->mPluginType;
    if (pluginType == logtail::ebpf::PluginType::NETWORK_OBSERVE
        || pluginType == logtail::ebpf::PluginType::PROCESS_SECURITY) {
        return 0;
    }

    switch (pluginType) {
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            auto* config = std::get_if<logtail::ebpf::NetworkSecurityConfig>(&arg->mConfig);
            for (const auto& opt : config->mOptions) {
                for (const auto& cn : opt.mCallNames) {
                    gPluginCallNames[int(arg->mPluginType)].push_back(cn);
                    int ret = logtail::ebpf::DeleteNetworkFilterForCallname(gWrapper, cn);
                    if (ret) {
                        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                                 "[update plugin] network security delete filter for callname %s failed.\n",
                                 cn.c_str());
                        return kErrDriverInternal;
                    }

                    ret = logtail::ebpf::CreateNetworkFilterForCallname(gWrapper, cn, opt.mFilter);
                    if (ret) {
                        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                                 "[update plugin] network security: create filter for callname %s falied.\n",
                                 cn.c_str());
                    }
                }
            }

            break;
        }
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     auto* config = std::get_if<logtail::ebpf::FileSecurityConfig>(&arg->mConfig);
        //     // 1. clean-up filter
        //     for (const auto& opt : config->mOptions) {
        //         for (const auto& cn : opt.mCallNames) {
        //             int ret = logtail::ebpf::DeleteFileFilterForCallname(gWrapper, cn);
        //             if (ret) {
        //                 ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                          "[update plugin] file security: delete filter for callname %s falied.\n",
        //                          cn.c_str());
        //             }
        //             ret = logtail::ebpf::CreateFileFilterForCallname(gWrapper, cn, opt.mFilter);
        //             if (ret) {
        //                 ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                          "[update plugin] file security: create filter for callname %s falied\n",
        //                          cn.c_str());
        //             }
        //         }
        //     }
        //     break;
        // }
        default:
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[update plugin] %s plugin type not supported.\n",
                     magic_enum::enum_name(arg->mPluginType).data());
            break;
    }

    return 0;
}

void DeletePerfBuffers(logtail::ebpf::PluginType pluginType) {
    std::vector<void*> pbs;
    {
        std::lock_guard lk(gPbMtx);
        // return;
        pbs = gPluginPbs[static_cast<int>(pluginType)];
        gPluginPbs[int(pluginType)] = {};
    }
    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_INFO,
             "[BPFWrapper][stop_plugin] begin clean perfbuffer for pluginType: %d  \n",
             int(pluginType));
    for (auto* pb : pbs) {
        auto* perfbuffer = static_cast<perf_buffer*>(pb);
        if (perfbuffer) {
            perf_buffer__free(perfbuffer);
        }
    }
}

int stop_plugin(logtail::ebpf::PluginType pluginType) {
    gPluginStatus[int(pluginType)] = false;

    switch (pluginType) {
        case logtail::ebpf::PluginType::NETWORK_OBSERVE:
            return ebpf_stop();
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            // 1. dynamic detach
            std::vector<logtail::ebpf::AttachProgOps> attachOps = {
                logtail::ebpf::AttachProgOps("event_exit_acct_process", true),
                logtail::ebpf::AttachProgOps("event_wake_up_new_task", true),
                logtail::ebpf::AttachProgOps("event_exit_disassociate_ctty", true),
                logtail::ebpf::AttachProgOps("event_execve", true),
            };
            int ret = gWrapper->DynamicDetachBPFObject(attachOps);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[stop plugin] process security: detach progs failed\n");
            }
            // 2. delete perf buffer
            DeletePerfBuffers(pluginType);
            break;
        }
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            // 1. dynamic detach
            auto callNames = gPluginCallNames[int(pluginType)];
            gPluginCallNames[int(pluginType)] = {};
            std::vector<logtail::ebpf::AttachProgOps> detachOps;
            for (const auto& cn : callNames) {
                detachOps.emplace_back("kprobe_" + cn, true);
            }
            int ret = 0;
            ret = gWrapper->DynamicDetachBPFObject(detachOps);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[stop plugin] network security: detach progs failed\n");
            }
            // 2. clean-up filter
            for (const auto& cn : callNames) {
                ret = logtail::ebpf::DeleteNetworkFilterForCallname(gWrapper, cn);
                if (ret) {
                    ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                             "[stop plugin] network security: delete filter for callname %s falied\n",
                             cn.c_str());
                }
            }
            // 3. delete perf buffer
            DeletePerfBuffers(pluginType);
            break;
        }
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     // 1. dynamic detach
        //     auto callNames = gPluginCallNames[int(pluginType)];
        //     gPluginCallNames[int(pluginType)] = {};
        //     std::vector<logtail::ebpf::AttachProgOps> detachOps;
        //     for (const auto& cn : callNames) {
        //         detachOps.emplace_back("kprobe_" + cn, true);
        //     }
        //     int ret = gWrapper->DynamicDetachBPFObject(detachOps);
        //     if (ret) {
        //         ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                  "[stop plugin] network security: detach progs failed\n");
        //     }
        //     // 2. clean-up filter
        //     for (const auto& cn : callNames) {
        //         ret = logtail::ebpf::DeleteFileFilterForCallname(gWrapper, cn);
        //         if (ret) {
        //             ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                      "[stop plugin] file security: delete filter for callname %s falied\n",
        //                      cn.c_str());
        //         }
        //     }

        //     // 3. delete perf buffer
        //     DeletePerfBuffers(pluginType);
        //     break;
        // }
        default: {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[stop plugin] unknown plugin type, please check. \n");
        }
    }
    return 0;
}

// do prog detach
int suspend_plugin(logtail::ebpf::PluginType pluginType) {
    gPluginStatus[int(pluginType)] = false;
    switch (pluginType) {
        case logtail::ebpf::PluginType::NETWORK_SECURITY: {
            auto callNames = gPluginCallNames[int(pluginType)];
            gPluginCallNames[int(pluginType)] = {};
            std::vector<logtail::ebpf::AttachProgOps> detachOps;
            for (const auto& cn : callNames) {
                detachOps.emplace_back("kprobe_" + cn, true);
            }
            int ret = 0;
            ret = gWrapper->DynamicDetachBPFObject(detachOps);
            if (ret) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[suspend plugin] network security: detach progs failed\n");
            }
            break;
        }
        // case logtail::ebpf::PluginType::FILE_SECURITY: {
        //     auto callNames = gPluginCallNames[int(pluginType)];
        //     gPluginCallNames[int(pluginType)] = {};
        //     std::vector<logtail::ebpf::AttachProgOps> detachOps;
        //     for (const auto& cn : callNames) {
        //         detachOps.emplace_back("kprobe_" + cn, true);
        //     }
        //     int ret = gWrapper->DynamicDetachBPFObject(detachOps);
        //     if (ret) {
        //         ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
        //                  "[suspend plugin] file security: detach progs failed\n");
        //     }
        //     break;
        // }
        case logtail::ebpf::PluginType::PROCESS_SECURITY: {
            break;
        }
        default: {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[suspend plugin] unknown plugin type, please check. \n");
        }
    }
    return 0;
}

int update_bpf_map_elem(logtail::ebpf::PluginType, const char* map_name, void* key, void* value, uint64_t flag) {
    return gWrapper->UpdateBPFHashMap(std::string(map_name), key, value, flag);
}
