//
// Created by qianlu on 2024/6/17.
//

#pragma once

extern "C" {

#include <bpf/libbpf.h>
#include <coolbpf/coolbpf.h>
};

#include <unistd.h>

#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "BPFMapTraits.h"
#include "Log.h"
#include "NetworkObserver.h"

namespace logtail {
namespace ebpf {
struct PerfBufferOps {
public:
    PerfBufferOps(const std::string& name, ssize_t size, perf_buffer_sample_fn scb, perf_buffer_lost_fn lcb)
        : name_(name), size_(size), sample_cb(scb), lost_cb(lcb) {}
    std::string name_;
    ssize_t size_;
    perf_buffer_sample_fn sample_cb;
    perf_buffer_lost_fn lost_cb;
};

struct AttachProgOps {
public:
    AttachProgOps(const std::string& name, bool attach) : name_(name), attach_(attach) {}
    std::string name_;
    bool attach_;
};

void* PerfThreadWoker(void* ctx, const std::string& name, std::atomic<bool>& flag);

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
        if (inited_)
            return 0;
        inited_ = true;
        skel_ = T::open_and_load();
        flag_ = true;
        if (!skel_) {
            //       LOG(WARNING) << "[BPFWrapper] failed to load BPF Object";
            return 1;
        }
        //     LOG(INFO) << "[BPFWrapper] successfully load BPF Object";
        bpf_map* map;
        bpf_object__for_each_map(map, skel_->obj) {
            const char* name = bpf_map__name(map);
            bpf_maps_[name] = map;
            // bpf_map__fd(map);
        }
        struct bpf_program* prog;
        bpf_object__for_each_program(prog, skel_->obj) {
            const char* name = bpf_program__name(prog);
            bpf_progs_[name] = prog;
            // bpf_program__fd(prog);
        }
        return 0;
    }

    /**
     * attach bpf programs
     */
    int DynamicAttachBPFObject(const std::vector<AttachProgOps>& ops) {
        int err;
        for (auto op : ops) {
            if (!op.attach_)
                continue;
            auto it = bpf_progs_.find(op.name_);
            if (it == bpf_progs_.end() || it->second == nullptr) {
                //         LOG(WARNING) << "failed to find bpf program, name:" << op.name_;
                continue;
            }
            bpf_program* prog = it->second;
            bpf_link* link = bpf_program__attach(prog);
            err = libbpf_get_error(link);
            if (err) {
                //         LOG(WARNING) << op.name_ << " failed to attach prog, err: " << err;
                continue;
            } else {
                links_.insert({op.name_, link});
            }
        }

        return 0;
    }

    /**
     * detach bpf programs
     */
    int DynamicDetachBPFObject(const std::vector<AttachProgOps>& ops) {
        for (auto op : ops) {
            auto it = links_.find(op.name_);
            if (it == links_.end()) {
                //         LOG(INFO) << op.name_ << " was not attached.";
                continue;
            }

            auto link = it->second;
            // do detach
            auto err = bpf_link__destroy(link);
            if (err) {
                //         LOG(WARNING) << op.name_ << " failed to destroy link, err: " << err;
                continue;
            }
            // remove from map
            links_.erase(it);
        }

        return 0;
    }

    /**
     * set tail calls
     */
    int SetTailCall(const std::string& map_name, const std::vector<std::string>& functions) {
        int map_fd = SearchMapFd(map_name);
        //     LOG(INFO) << "[BPFWrapper] find " << map_name << " fd:" << map_fd ;
        if (map_fd < 0) {
            //       LOG(INFO) << "[BPFWrapper] find prog map failed for " << map_name ;
            return 1;
        }

        for (int i = 0; i < functions.size(); i++) {
            auto func = functions[i];
            int tmp = i;
            int func_fd = SearchProgFd(func);
            if (func_fd <= 0) {
                //         LOG(INFO) << "[BPFWrapper] search prog " << func << " fd failed, skip update elem. fd:" <<
                //         func_fd ;
                continue;
            }

            int ret = bpf_map_update_elem(map_fd, &tmp, &func_fd, 0);
            if (ret) {
                //         LOG(INFO) << "[BPFWrapper] update prog map " << map_name << " failed for " << func << ",
                //         err:" << ret ;
            }
        }
        return 0;
    }

    template <typename MapInMapType>
    int DeleteInnerMap(const std::string& outter_map_name, void* outter_key) {
        int map_fd = SearchMapFd(outter_map_name);
        //     LOG(INFO) << "[BPFWrapper] find " << outter_map_name << " fd:" << map_fd ;
        if (map_fd < 0) {
            //       LOG(INFO) << "[BPFWrapper] find outter map failed for " << outter_map_name ;
            return 1;
        }

        // delete bpf map
        bpf_map_delete_elem(map_fd, outter_key);

        int* key = static_cast<int*>(outter_key);

        // get inner map fd from outter map fd and outter key
        // close fd for inner map
        int inner_fd = -1;
        if (map_in_map_fds_[map_fd].count(*key)) {
            inner_fd = map_in_map_fds_[map_fd][*key];
        }
        //     LOG(INFO) << "[FindInnerMapFd] outter map name:" << outter_map_name << " outter map fd:" << map_fd << "
        //     outter key:" << (*key) << " inner_fd:" << inner_fd;

        if (inner_fd > 0)
            close(inner_fd);

        return 0;
    }

    template <typename MapInMapType>
    int DeleteInnerMapElem(const std::string& outter_map_name, void* outter_key, void* inner_key) {
        int map_fd = SearchMapFd(outter_map_name);
        //     LOG(INFO) << "[BPFWrapper] find " << outter_map_name << " fd:" << map_fd ;
        if (map_fd < 0) {
            //       LOG(INFO) << "[BPFWrapper] find outter map failed for " << outter_map_name ;
            return 1;
        }
        int inner_map_fd = -1;
        uint32_t inner_map_id = 0;
        int ret = bpf_map_lookup_elem(map_fd, outter_key, &inner_map_id);
        if (ret) {
            //       LOG(WARNING) << "failed to lookup inner map id, skip delete element. outter map name: "
            // << outter_map_name << " errno: " << ret << " inner_map_id:" << inner_map_id;

            return 0;
        }

        inner_map_fd = bpf_map_get_fd_by_id(inner_map_id);
        if (inner_map_fd < 0) {
            //       LOG(ERROR) << "inner map fd less than 0, outter map name: " << outter_map_name;
            return 1;
        }

        ret = bpf_map_delete_elem(inner_map_fd, inner_key);
        //     LOG(INFO) << "delete from inner map, inner map fd:" << inner_map_fd << " inner map id:" << inner_map_id
        //     << " ret:" << ret;

        close(inner_map_fd);

        return ret;
    }

    template <typename MapInMapType>
    int LookupInnerMapElem(const std::string& outter_map_name, void* outter_key, void* inner_key, void* inner_val) {
        int map_fd = SearchMapFd(outter_map_name);
        //     LOG(INFO) << "[BPFWrapper] find " << outter_map_name << " fd:" << map_fd ;
        if (map_fd < 0) {
            //       LOG(INFO) << "[BPFWrapper] find outter map failed for " << outter_map_name ;
            return 1;
        }
        int inner_map_fd = -1;
        uint32_t inner_map_id = 0;
        int ret = bpf_map_lookup_elem(map_fd, outter_key, &inner_map_id);
        if (ret) {
            //       LOG(WARNING) << "failed to lookup inner map id. outter map name: "
            // << outter_map_name << " errno: " << ret << " inner_map_id:" << inner_map_id;

            return 1;
        }

        inner_map_fd = bpf_map_get_fd_by_id(inner_map_id);
        if (inner_map_fd < 0) {
            //       LOG(ERROR) << "inner map fd less than 0, outter map name: " << outter_map_name;
            return 1;
        }

        ret = bpf_map_lookup_elem(inner_map_fd, inner_key, inner_val);
        //     LOG(INFO) << "lookup from inner map, inner map fd:" << inner_map_fd << " inner map id:" << inner_map_id
        //     << " ret:" << ret;

        close(inner_map_fd);

        return ret;
    }

    template <typename MapInMapType>
    int UpdateInnerMapElem(
        const std::string& outter_map_name, void* outter_key, void* inner_key, void* inner_value, uint64_t flag) {
        int map_fd = SearchMapFd(outter_map_name);
        //     LOG(INFO) << "[BPFWrapper] find " << outter_map_name << " fd:" << map_fd ;
        if (map_fd < 0) {
            //       LOG(INFO) << "[BPFWrapper] find outter map failed for " << outter_map_name ;
            return 1;
        }
        int inner_map_fd = -1;
        uint32_t inner_map_id = 0;
        int ret = bpf_map_lookup_elem(map_fd, outter_key, &inner_map_id);
        if (ret) {
            //       LOG(INFO) << "failed to lookup inner map fd, begin to init outter map. outter map name: "
            // << outter_map_name << " errno: " << ret << " inner_map_id:" << inner_map_id;
            struct bpf_map_create_opts* popt = nullptr;
            struct bpf_map_create_opts opt;
            if (BPFMapTraits<MapInMapType>::map_flag != -1) {
                ::memset(&opt, 0, sizeof(struct bpf_map_create_opts));
                // opt.map_extra = ;
                opt.sz = sizeof(opt);
                opt.map_flags = BPF_F_NO_PREALLOC;
                popt = &opt;
            }

            // TODO @qianlu.kk recycle this fd when distroy
            int fd = bpf_map_create(BPFMapTraits<MapInMapType>::inner_map_type,
                                    NULL,
                                    BPFMapTraits<MapInMapType>::inner_key_size,
                                    BPFMapTraits<MapInMapType>::inner_val_size,
                                    BPFMapTraits<MapInMapType>::inner_max_entries,
                                    popt);
            if (fd < 0) {
                //         LOG(WARNING) << "failed to create bpf map, fd:" << fd;
                return 1;
            }

            int* key = static_cast<int*>(outter_key);
            map_in_map_fds_[map_fd][*key] = fd;

            ret = bpf_map_update_elem(map_fd, outter_key, &fd, BPF_ANY);
            //       LOG(INFO) << "successfully create bpf map, fd:" << fd << " outter key:" << *(key) << " update res:"
            //       << ret;
            close(fd);
        }

        ret = bpf_map_lookup_elem(map_fd, outter_key, &inner_map_id);
        if (ret) {
            //       LOG(WARNING) << "failed to lookup inner map fd. outter map name: "
            // << outter_map_name << " errno: " << ret << " inner_map_fd:" << inner_map_fd;
            return 1;
        }

        inner_map_fd = bpf_map_get_fd_by_id(inner_map_id);
        if (inner_map_fd < 0) {
            //       LOG(WARNING) << "inner map fd less than 0, outter map name: " << outter_map_name;
            return 1;
        }

        ret = bpf_map_update_elem(inner_map_fd, inner_key, inner_value, flag);
        //     LOG(INFO) << "insert to inner map, inner map fd:" << inner_map_fd << " inner map id:" << inner_map_id <<
        //     " ret:" << ret;

        close(inner_map_fd);

        return ret;
    }

    /**
     * update elements from bpf map
     */
    int UpdateBPFHashMap(const std::string& map_name, void* key, void* value, uint64_t flag) {
        int map_fd = SearchMapFd(map_name);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][UpdateBPFHashMap] find map name: %s map fd: %d \n",
                 map_name,
                 map_fd);
        if (map_fd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][UpdateBPFHashMap] find hash map failed for: %s \n",
                     map_name);
            return 1;
        }
        return bpf_map_update_elem(map_fd, key, value, flag);
    }

    /**
     * lookup element from bpf map
     */
    int LookupBPFHashMap(const std::string& map_name, void* key, void* value) {
        int map_fd = SearchMapFd(map_name);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][LookupBPFHashMap] find map name: %s map fd: %d \n",
                 map_name,
                 map_fd);
        if (map_fd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][LookupBPFHashMap] find hash map failed for: %s \n",
                     map_name);
            return 1;
        }
        return bpf_map_lookup_elem(map_fd, key, value);
    }

    /**
     * remove element from bpf map
     */
    int RemoveBPFHashMap(const std::string& map_name, void* key) {
        int map_fd = SearchMapFd(map_name);
        ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                 "[BPFWrapper][RemoveBPFHashMap] find map name: %s map fd: %d \n",
                 map_name,
                 map_fd);
        if (map_fd < 0) {
            ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_DEBUG,
                     "[BPFWrapper][RemoveBPFHashMap] find hash map failed for: %s \n",
                     map_name);
            return 1;
        }
        bpf_map_delete_elem(map_fd, key);
        return 0;
    }

    void DeletePerfBuffer(void* pb) {
        struct perf_buffer* pb_ptr = static_cast<struct perf_buffer*>(pb);
        perf_buffer__free((struct perf_buffer*)pb);
    }

    int PollPerfBuffer(void* pb, int max_events, int timeout_ms) {
        return perf_buffer__poll((struct perf_buffer*)pb, timeout_ms);
        // if (err < 0 && err != -EINTR)
        // {
        //     std::cout << "error polling perf buffer: " << strerror(-err) << std::endl;
        //     goto cleanup;
        // }

        // if (err == -EINTR)
        //     goto cleanup;
    }

    void* CreatePerfBuffer(
        const std::string& name, int page_cnt, void* ctx, perf_buffer_sample_fn data_cb, perf_buffer_lost_fn loss_cb) {
        int mapFd = SearchMapFd(name);
        if (mapFd < 0) {
            return nullptr;
        }

        struct perf_buffer_opts pb_opts = {};
        pb_opts.sample_cb = data_cb;
        pb_opts.ctx = ctx;
        pb_opts.lost_cb = loss_cb;

        struct perf_buffer* pb = NULL;
        pb = perf_buffer__new(mapFd, page_cnt == 0 ? 128 : page_cnt, &pb_opts);
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
                     "[BPFWrapper][CreatePerfBuffer] failed to open perf buffer: %d \n",
                     err);
            return nullptr;
        }
        return pb;
    }

    /**
     * Attach perf buffers
     */
    std::vector<std::thread>
    AttachPerfBuffers(void* ctx, std::vector<PerfBufferOps>& perf_buffer_ops, std::atomic<bool>& flag) {
        std::vector<std::thread> res;
        for (auto op : perf_buffer_ops) {
            int map_fd = SearchMapFd(op.name_);
            //       LOG(INFO) << "[BPFWrapper] find " << op.name_ << " fd:" << map_fd ;
            if (map_fd < 0) {
                //         LOG(INFO) << "[BPFWrapper] find perf map failed for " << op.name_ ;
                continue;
            }
            struct perf_thread_arguments* perf_args
                = static_cast<struct perf_thread_arguments*>(calloc(1, sizeof(struct perf_thread_arguments)));
            if (!perf_args) {
                return {};
            }

            perf_args->mapfd = map_fd;
            perf_args->sample_cb = op.sample_cb;
            perf_args->lost_cb = op.lost_cb;
            perf_args->ctx = ctx;
            perf_args->pg_cnt = op.size_;
            res.emplace_back(std::thread(PerfThreadWoker, (void*)perf_args, op.name_, std::ref(flag)));
        }

        return res;
    }

    int DetachAllPerfBuffers() { return 0; }

    /**
     * Destroy skel and release resources.
     */
    void Destroy() {
        if (!inited_)
            return;
        //     LOG(INFO) << "begin to destroy bpf wrapper";
        // clear all links first
        for (auto& it : links_) {
            auto link = it.second;
            auto err = bpf_link__destroy(link);
            if (err) {
                ebpf_log(logtail::ebpf::eBPFLogType::NAMI_LOG_TYPE_WARN,
                         "[BPFWrapper][Destroy] failed to destroy link, err: %d \n",
                         err);
            }
        }

        links_.clear();
        bpf_maps_.clear();
        bpf_progs_.clear();

        // destroy skel
        T::destroy(skel_);

        // stop perf threads ...
        flag_ = false;
        DetachAllPerfBuffers();
        inited_ = false;
    }
    // pin map

    int SearchProgFd(const std::string& name) {
        auto it = bpf_progs_.find(name);
        if (it == bpf_progs_.end()) {
            //       LOG(WARNING) << "failed to find prog by name: " << name;
            return -1;
        }

        return bpf_program__fd(it->second);
    }

    int SearchInnerMapFd(int outter_fd, int outter_key) { return 0; }

    int SearchMapFd(const std::string& name) {
        auto it = bpf_maps_.find(name);
        if (it == bpf_maps_.end()) {
            //       LOG(WARNING) << "failed to find bpf map by name: " << name;
            return -1;
        }

        return bpf_map__fd(it->second);
    }

    int GetBPFMapFdById(int id) { return bpf_map_get_fd_by_id(id); }

    bool
    CreateBPFMap(enum bpf_map_type map_type, int key_size, int value_size, int max_entries, unsigned int map_flags) {
        bpf_create_map(map_type, key_size, value_size, max_entries, map_flags);
        return true;
    }

    bool CreateBPFMapInMap() {
        // TODO @qianlu.kk
        // bpf_create_map_in_map(enum bpf_map_type map_type, const char *name, int key_size, int inner_map_fd, int
        // max_entries, bpf_create_map_in_map();
        return true;
    }

    // {map_name, map_fd}
    std::map<std::string, bpf_map*> bpf_maps_;
    // {map_name, prog_fd}
    std::map<std::string, bpf_program*> bpf_progs_;

    std::map<std::string, bpf_link*> links_;

    std::unordered_map<int, std::unordered_map<int, int>> map_in_map_fds_;

    T* skel_ = nullptr;
    volatile bool inited_ = false;
    // std::vector<std::thread> perf_threads_;
    std::atomic_bool flag_ = false;
    // links, used for strore bpf programs
    friend class NetworkSecurityManager;
};
} // namespace ebpf
} // namespace logtail
