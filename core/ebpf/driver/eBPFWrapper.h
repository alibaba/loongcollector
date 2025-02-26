// Copyright 2023 iLogtail Authors
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

#pragma once

extern "C" {
#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <unistd.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BPFMapTraits.h"
#include "Log.h"

namespace logtail {
namespace ebpf {
struct PerfBufferOps {
public:
    PerfBufferOps(const std::string& name, ssize_t size, perf_buffer_sample_fn scb, perf_buffer_lost_fn lcb)
        : name(name), size(size), sampleCb(scb), lostCb(lcb) {}
    std::string name;
    ssize_t size;
    perf_buffer_sample_fn sampleCb;
    perf_buffer_lost_fn lostCb;
};

struct AttachProgOps {
public:
    AttachProgOps(const std::string& name, bool attach) : name(name), attach(attach) {}
    std::string name;
    bool attach;
};

class BPFWrapperBase {
public:
    virtual ~BPFWrapperBase() = default;
};

template <typename T>
class BPFWrapper : public BPFWrapperBase {
public:
    static std::shared_ptr<BPFWrapper<T>> Create() { return std::make_shared<BPFWrapper<T>>(); }

    /**
     * Init will open and load bpf object, and fill caches for maps and progs
     */
    int Init() {
        if (mInited) {
            return 0;
        }
        mInited = true;
        mSkel = T::open_and_load();
        mFlag = true;
        if (!mSkel) {
            return 1;
        }
        bpf_map* map = nullptr;
        bpf_object__for_each_map(map, mSkel->obj) {
            const char* name = bpf_map__name(map);
            mBpfMaps[name] = map;
        }
        struct bpf_program* prog = nullptr;
        bpf_object__for_each_program(prog, mSkel->obj) {
            const char* name = bpf_program__name(prog);
            mBpfProgs[name] = prog;
        }
        return 0;
    }

    /**
     * attach bpf programs
     */
    int DynamicAttachBPFObject(const std::vector<AttachProgOps>& ops) {
        int err = 0;
        for (const auto& op : ops) {
            if (!op.attach) {
                continue;
            }
            auto it = mBpfProgs.find(op.name);
            if (it == mBpfProgs.end() || it->second == nullptr) {
                continue;
            }
            bpf_program* prog = it->second;
            bpf_link* link = bpf_program__attach(prog);
            err = libbpf_get_error(link);
            if (err) {
                continue;
            }
            mLinks.insert({op.name, link});
        }

        return 0;
    }

    /**
     * detach bpf programs
     */
    int DynamicDetachBPFObject(const std::vector<AttachProgOps>& ops) {
        for (const auto& op : ops) {
            auto it = mLinks.find(op.name);
            if (it == mLinks.end()) {
                continue;
            }

            auto* link = it->second;
            // do detach
            auto err = bpf_link__destroy(link);
            if (err) {
                continue;
            }
            // remove from map
            mLinks.erase(it);
        }

        return 0;
    }

    /**
     * set tail calls
     */
    int SetTailCall(const std::string& mapName, const std::vector<std::string>& functions) {
        int mapFd = SearchMapFd(mapName);
        if (mapFd < 0) {
            return 1;
        }

        for (int i = 0; i < (int)functions.size(); i++) {
            const auto& func = functions[i];
            int funcFd = SearchProgFd(func);
            if (funcFd <= 0) {
                continue;
            }

            int ret = bpf_map_update_elem(mapFd, &i, &funcFd, 0);
            if (ret) {
                // abnormal
            }
        }
        return 0;
    }

    template <typename MapInMapType>
    int DeleteInnerMap(const std::string& outterMapName, void* outterKey) {
        int mapFd = SearchMapFd(outterMapName);
        if (mapFd < 0) {
            return 1;
        }

        // delete bpf map
        bpf_map_delete_elem(mapFd, outterKey);

        int* key = static_cast<int*>(outterKey);

        // get inner map fd from outter map fd and outter key
        // close fd for inner map
        int innerFd = -1;
        if (mApInMapFds[mapFd].count(*key)) {
            innerFd = mApInMapFds[mapFd][*key];
        }

        if (innerFd > 0) {
            close(innerFd);
        }

        return 0;
    }

    template <typename MapInMapType>
    int DeleteInnerMapElem(const std::string& outterMapName, void* outterKey, void* innerKey) {
        int mapFd = SearchMapFd(outterMapName);
        if (mapFd < 0) {
            return 1;
        }
        int innerMapFd = -1;
        uint32_t innerMapId = 0;
        int ret = bpf_map_lookup_elem(mapFd, outterKey, &innerMapId);
        if (ret) {
            return 0;
        }

        innerMapFd = bpf_map_get_fd_by_id(innerMapId);
        if (innerMapFd < 0) {
            return 1;
        }

        ret = bpf_map_delete_elem(innerMapFd, innerKey);

        close(innerMapFd);

        return ret;
    }

    template <typename MapInMapType>
    int LookupInnerMapElem(const std::string& outterMapName, void* outterKey, void* innerKey, void* innerVal) {
        int mapFd = SearchMapFd(outterMapName);
        if (mapFd < 0) {
            return 1;
        }
        int innerMapFd = -1;
        uint32_t innerMapId = 0;
        int ret = bpf_map_lookup_elem(mapFd, outterKey, &innerMapId);
        if (ret) {
            return 1;
        }

        innerMapFd = bpf_map_get_fd_by_id(innerMapId);
        if (innerMapFd < 0) {
            return 1;
        }

        ret = bpf_map_lookup_elem(innerMapFd, innerKey, innerVal);

        close(innerMapFd);

        return ret;
    }

    template <typename MapInMapType>
    int UpdateInnerMapElem(
        const std::string& outterMapName, void* outterKey, void* innerKey, void* innerValue, uint64_t flag) {
        int mapFd = SearchMapFd(outterMapName);
        if (mapFd < 0) {
            return 1;
        }
        int innerMapFd = -1;
        uint32_t innerMapId = 0;
        int ret = bpf_map_lookup_elem(mapFd, outterKey, &innerMapId);
        if (ret) {
            struct bpf_map_create_opts* popt = nullptr;
            struct bpf_map_create_opts opt {};
            if (BPFMapTraits<MapInMapType>::map_flag != -1) {
                ::memset(&opt, 0, sizeof(struct bpf_map_create_opts));
                // opt.map_extra = ;
                opt.sz = sizeof(opt);
                opt.map_flags = BPF_F_NO_PREALLOC;
                popt = &opt;
            }

            // TODO @qianlu.kk recycle this fd when distroy
            // we don't need free inner bpf map manually, since kernel will hold ref count for every bpf map
            // when we destroy outter map, the inner maps that holds will be destroy by kernel ...
            int fd = bpf_map_create(BPFMapTraits<MapInMapType>::inner_map_type,
                                    NULL,
                                    BPFMapTraits<MapInMapType>::inner_key_size,
                                    BPFMapTraits<MapInMapType>::inner_val_size,
                                    BPFMapTraits<MapInMapType>::inner_max_entries,
                                    popt);
            if (fd < 0) {
                return 1;
            }

            int* key = static_cast<int*>(outterKey);
            mApInMapFds[mapFd][*key] = fd;

            ret = bpf_map_update_elem(mapFd, outterKey, &fd, BPF_ANY);
            close(fd);
        }

        ret = bpf_map_lookup_elem(mapFd, outterKey, &innerMapId);
        if (ret) {
            return 1;
        }

        innerMapFd = bpf_map_get_fd_by_id(innerMapId);
        if (innerMapFd < 0) {
            return 1;
        }

        ret = bpf_map_update_elem(innerMapFd, innerKey, innerValue, flag);
        close(innerMapFd);

        return ret;
    }

    /**
     * update elements from bpf map
     */
    int UpdateBPFHashMap(const std::string& mapName, void* key, void* value, uint64_t flag) {
        int mapFd = SearchMapFd(mapName);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][UpdateBPFHashMap] find map name: %s map fd: %d \n",
                 mapName.c_str(),
                 mapFd);
        if (mapFd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][UpdateBPFHashMap] find hash map failed for: %s \n",
                     mapName.c_str());
            return 1;
        }
        return bpf_map_update_elem(mapFd, key, value, flag);
    }

    /**
     * lookup element from bpf map
     */
    int LookupBPFHashMap(const std::string& mapName, void* key, void* value) {
        int mapFd = SearchMapFd(mapName);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][LookupBPFHashMap] find map name: %s map fd: %d \n",
                 mapName.c_str(),
                 mapFd);
        if (mapFd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][LookupBPFHashMap] find hash map failed for: %s \n",
                     mapName.c_str());
            return 1;
        }
        return bpf_map_lookup_elem(mapFd, key, value);
    }

    /**
     * remove element from bpf map
     */
    int RemoveBPFHashMap(const std::string& mapName, void* key) {
        int mapFd = SearchMapFd(mapName);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][RemoveBPFHashMap] find map name: %s map fd: %d \n",
                 mapName.c_str(),
                 mapFd);
        if (mapFd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][RemoveBPFHashMap] find hash map failed for: %s \n",
                     mapName.c_str());
            return 1;
        }
        bpf_map_delete_elem(mapFd, key);
        return 0;
    }

    void DeletePerfBuffer(void* pb) { perf_buffer__free((struct perf_buffer*)pb); }

    int PollPerfBuffer(void* pb, int /*maxEvents*/, int timeoutMs) {
        return perf_buffer__poll((struct perf_buffer*)pb, timeoutMs);
    }

    void* CreatePerfBuffer(
        const std::string& name, int pageCnt, void* ctx, perf_buffer_sample_fn dataCb, perf_buffer_lost_fn lossCb) {
        int mapFd = SearchMapFd(name);
        if (mapFd < 0) {
            return nullptr;
        }

        struct perf_buffer_opts pbOpts = {};
        pbOpts.sample_cb = dataCb;
        pbOpts.ctx = ctx;
        pbOpts.lost_cb = lossCb;

        struct perf_buffer* pb = NULL;
        pb = perf_buffer__new(mapFd, pageCnt == 0 ? 128 : pageCnt, &pbOpts);
        auto err = libbpf_get_error(pb);
        if (err) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[BPFWrapper][CreatePerfBuffer] error new perf buffer: %s \n",
                     strerror(-err));
            return nullptr;
        }

        if (!pb) {
            err = -errno;
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                     "[BPFWrapper][CreatePerfBuffer] failed to open perf buffer: %ld \n",
                     err);
            return nullptr;
        }
        return pb;
    }

    int DetachAllPerfBuffers() { return 0; }

    /**
     * Destroy skel and release resources.
     */
    void Destroy() {
        if (!mInited) {
            return;
        }
        //     LOG(INFO) << "begin to destroy bpf wrapper";
        // clear all links first
        for (auto& it : mLinks) {
            auto* link = it.second;
            auto err = bpf_link__destroy(link);
            if (err) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[BPFWrapper][Destroy] failed to destroy link, err: %d \n",
                         err);
            }
        }

        mLinks.clear();
        mBpfMaps.clear();
        mBpfProgs.clear();

        // destroy skel
        T::destroy(mSkel);

        // stop perf threads ...
        mFlag = false;
        DetachAllPerfBuffers();
        mInited = false;
    }
    // pin map

    int SearchProgFd(const std::string& name) {
        auto it = mBpfProgs.find(name);
        if (it == mBpfProgs.end()) {
            return -1;
        }

        return bpf_program__fd(it->second);
    }

    int SearchMapFd(const std::string& name) {
        auto it = mBpfMaps.find(name);
        if (it == mBpfMaps.end()) {
            return -1;
        }

        return bpf_map__fd(it->second);
    }

    int GetBPFMapFdById(int id) { return bpf_map_get_fd_by_id(id); }

    int GetBPFProgFdById(int id) { return bpf_prog_get_fd_by_id(id); }

    bool CreateBPFMap(enum bpf_map_type mapType, int keySize, int valueSize, int maxEntries, unsigned int mapFlags) {
        bpf_create_map(mapType, keySize, valueSize, maxEntries, mapFlags);
        return true;
    }

    bool CreateBPFMapInMap() {
        // TODO @qianlu.kk
        // bpf_create_map_in_map(enum bpf_map_type map_type, const char *name, int key_size, int inner_map_fd, int
        // max_entries, bpf_create_map_in_map();
        return true;
    }

private:
    // {map_name, map_fd}
    std::map<std::string, bpf_map*> mBpfMaps;
    // {map_name, prog_fd}
    std::map<std::string, bpf_program*> mBpfProgs;

    std::map<std::string, bpf_link*> mLinks;

    std::unordered_map<int, std::unordered_map<int, int>> mApInMapFds;

    T* mSkel = nullptr;
    volatile bool mInited = false;
    std::atomic_bool mFlag = false;
    // links, used for strore bpf programs
    friend class NetworkSecurityManager;
};
} // namespace ebpf
} // namespace logtail
