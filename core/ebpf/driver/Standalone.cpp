#include <dlfcn.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <memory>
#include <variant>
#include <sstream>
#include <assert.h>

#include "ebpf/include/export.h"
#include "NetworkObserver.h"
#include "Log.h"
#include <net.h>
// #include "eBPFWrapper.h"
// #include <security.skel.h>

using init_func = int (*)(void *);
using deinit_func = void (*)();
using remove_func = int(*)(void*);
using update_func=void(*)(void*);
using suspend_func=int(*)(void*);
using ebpf_poll_events_func = int32_t(*)(int32_t, int32_t *, int);

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}


class source_manager {
public:
    source_manager() : handle(nullptr)/*, initPluginFunc(nullptr), deinitPluginFunc(nullptr)*/ {}

    ~source_manager() {
        // clearPlugin();
    }

    bool init() {
      // load libsockettrace.so
      handle = dlopen("./libebpf_driver.so", RTLD_NOW);
      if (!handle) {
        std::cerr << "dlopen error: " << dlerror()  << std::endl;
        return false;
      }
      std::cout << "successfully open" ;

      initPluginFunc = (init_func)dlsym(handle, "start_plugin");
      noPollFunc = (ebpf_poll_events_func)dlsym(handle, "ebpf_poll_events");
      pollPbsFunc = (poll_plugin_pbs_func)dlsym(handle, "poll_plugin_pbs");
      setLoggerFunc = (set_logger_func)dlsym(handle, "set_logger");

      setLoggerFunc([](int16_t level, const char *format, va_list args){
            
          char buffer[1024] = {0};
          vsnprintf(buffer, 1023, format, args);
          std::cout << buffer;
          std::cout << std::endl;
          return 0;
      });

    //   removePluginFunc = (remove_func)dlsym(handle, "removep");
    //   deinitPluginFunc = (deinit_func)dlsym(handle, "deinit");
    //   updatePluginFunc = (update_func)dlsym(handle, "update");
    //   suspendPluginFunc = (suspend_func)dlsym(handle, "suspend");
      

      if (!initPluginFunc/* || !deinitPluginFunc || !removePluginFunc*/) {
        std::cerr << "dlsym error: " << dlerror() ;
        dlclose(handle);
        return false;
      } else {
        std::cout << "succesfully get init/call/deinit func address" ;
      }
      return true;
    }

    bool startProcessPlugin() {
        auto ebpf_config = std::make_unique<logtail::ebpf::PluginConfig>();
        logtail::ebpf::ProcessConfig config;
        config.mPerfBufferSpec = {
          {"tcpmon_map", 128, nullptr, 
          [](void *ctx, int cpu, void *data, uint32_t size) {
            std::cout << "receive event ... " << std::endl;
          },
          [](void *ctx, int cpu, unsigned long long cnt){
            std::cout << "lost " << cnt << " events on CPU:" << cpu << std::endl;
          }}
        };
        ebpf_config->mPluginType = logtail::ebpf::PluginType::PROCESS_SECURITY;
        ebpf_config->mConfig = config;

        std::cout << "begin to call init func" << std::endl;
        initPluginFunc(ebpf_config.get());
        std::cout << "after call init func" << std::endl;

        noPerfWorker = std::thread([&](){
          int32_t flag = 0;
          while(true) {
            auto ret = pollPbsFunc(logtail::ebpf::PluginType::PROCESS_SECURITY, 4096, &flag, 200);
            if (ret < 0) {
              std::cout << "poll event err, ret:" << ret << std::endl;
            } else {
              std::cout << "poll event number:" << ret << std::endl;
            }
          }
        });
        return true;
    }

    bool startNetworkPlugin() {
      

      // void* init_param = nullptr;
      auto ebpf_config = std::make_unique<logtail::ebpf::PluginConfig>();

        // compose param
        logtail::ebpf::NetworkObserveConfig config;
        std::string coolbpfPaht = "./libcoolbpf.so.1.0.0";
        config.mSo = coolbpfPaht;
        Dl_info dlinfo;
        int err;
        void* cleanup_dog_ptr = dlsym(handle, "ebpf_cleanup_dog");
        if (nullptr == cleanup_dog_ptr) {
          std::cout << "[SourceManager] get ebpf_cleanup_dog address failed!"<< std::endl ;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_cleanup_dog address" << std::endl ;
        }

        err = dladdr(cleanup_dog_ptr, &dlinfo);
        if (!err)
        {
          std::cout << "[SourceManager] ebpf_cleanup_dog laddr failed, err:" << strerror(err);
          // printf(:%s\n", strerror(err));
        } else {
          config.mUprobeOffset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_cleanup_dog dlinfo, uprobe_offset:" << config.mUprobeOffset<< std::endl;
        }

        void* ebpf_update_conn_addr_ptr = dlsym(handle, "ebpf_update_conn_addr");
        if (nullptr == ebpf_update_conn_addr_ptr) {
          std::cout << "[SourceManager] get ebpf_update_conn_addr address failed!" << std::endl;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_update_conn_addr address" << std::endl;
        }
        err = dladdr(ebpf_update_conn_addr_ptr, &dlinfo);
        if (!err)
        {
          printf("[SourceManager] ebpf_update_conn_addr laddr failed, err:%s\n", strerror(err));
        } else {
          config.mUpcaOffset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_update_conn_addr dlinfo, upca_offset" << config.mUpcaOffset << std::endl;
        }

        void* ebpf_disable_process_ptr = dlsym(handle, "ebpf_disable_process");
        if (nullptr == ebpf_disable_process_ptr) {
          std::cout << "[SourceManager] get ebpf_disable_process address failed!" ;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_disable_process address" ;
        }
        err = dladdr(ebpf_disable_process_ptr, &dlinfo);
        if (!err)
        {
          printf("[SourceManager] ebpf_disable_process laddr failed, err:%s\n", strerror(err));
        } else {
          config.mUppsOffset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_disable_process dlinfo, upps_offset:" << config.mUppsOffset ;
        }

        void* ebpf_update_conn_role_ptr = dlsym(handle, "ebpf_update_conn_role");
        if (nullptr == ebpf_update_conn_role_ptr) {
          std::cout << "[SourceManager] get ebpf_update_conn_role address failed!" ;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_update_conn_role address" ;
        }
        err = dladdr(ebpf_update_conn_role_ptr, &dlinfo);
        if (!err)
        {
          printf("[SourceManager] ebpf_update_conn_role laddr failed, err:%s\n", strerror(err));
        } else {
          config.mUpcrOffset = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_update_conn_role dlinfo, upcr_offset:" << config.mUpcrOffset ;
        }
        // config.enable_cid_filter = false;
        config.mCtrlHandler = [](void *custom_data, struct conn_ctrl_event_t *event){
            std::cout << "recv ctrl event" << std::endl;
        };
        config.mDataHandler = [](void *custom_data, struct conn_data_event_t *event){
            std::cout << "recv data event" << std::endl;
        };
        config.mStatsHandler = [](void *custom_data, struct conn_stats_event_t *event){
            std::cout << "recv stats event" << std::endl;
        };
        config.mLostHandler = [](void *custom_data, enum callback_type_e type, uint64_t lost_count){
            std::cout << "lost events ..." << std::endl;
        };
        config.mLogHandler = [](int16_t level, const char *format, va_list args){
            
            char buffer[1024] = {0};
            vsnprintf(buffer, 1023, format, args);
            std::cout << buffer;
            std::cout << std::endl;
            return 0;
        };
        // config.enable_container_ids_ = {"5cb30fc9cfc3d30a2b561a20760d69a191bcd710dda5e258e1be05a0af1a5ed1"};
        ebpf_config->mPluginType = logtail::ebpf::PluginType::NETWORK_OBSERVE;
        ebpf_config->mConfig = config;        

      std::cout << "begin to call init func" << std::endl;

      initPluginFunc(ebpf_config.get());

      noPerfWorker = std::thread([&](){
        int32_t flag = 0;
        while(true) {
          auto ret = noPollFunc(4096, &flag, 200);
          if (ret < 0) {
            std::cout << "poll event err, ret:" << ret << std::endl;
          }
        }
        
      });

      return true;
    }

    // void removePlugin() {
    //   logtail::ebpf::PluginConfig * config = new logtail::ebpf::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_OBSERVE;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::FILE_SECURITY;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_SECURITY;
    //   }
    //   removePluginFunc(config);
    //   delete config;
    // }

    // void clearPlugin() {
    //     if (handle) {
    //         if (deinitPluginFunc)
    //             deinitPluginFunc();
    //         dlclose(handle);
    //     }
    // }

    // void suspendPlugin() {
    //   logtail::ebpf::PluginConfig * config = new logtail::ebpf::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_OBSERVE;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::FILE_SECURITY;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_SECURITY;
    //   }
    //   suspendPluginFunc(config);
    //   delete config;
    // }

    // void updatePlugin() {
    //   logtail::ebpf::PluginConfig * config = new logtail::ebpf::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_OBSERVE;
    //     config->type = UpdataType::OBSERVER_UPDATE_TYPE_CHANGE_WHITELIST;
    //     logtail::ebpf::NetworkObserveConfig nconfig;
    //     nconfig.enable_container_ids_ = {"5cb30fc9cfc3d30a2b561a20760d69a191bcd710dda5e258e1be05a0af1a5ed1"};
    //     nconfig.enable_cid_filter = false;
    //     config->config_ = nconfig;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::FILE_SECURITY;
    //     logtail::ebpf::FileSecurityConfig fconfig;
    //     logtail::ebpf::SecurityOption opt;
    //     opt.call_names_ = {"security_file_permission"};
    //     if (FLAGS_update_path_filter.size()) {
    //       auto pathes = split(FLAGS_update_path_filter, ',');
    //       logtail::ebpf::SecurityFileFilter filter;
    //       for (auto path : pathes) {
    //         filter.mFilePathList.emplace_back(std::move(path));
    //       }
    //       opt.filter_ = filter;
    //     }
    //     fconfig.options_ = {opt};
    //     fconfig.file_security_cb_ = std::bind(&source_manager::handleBatchFileEvent, this, std::placeholders::_1);
    //     config->mPluginType = logtail::ebpf::PluginType::FILE_SECURITY;
    //     config->stats_handler_ = HandleStats;
    //     config->config_ = fconfig;
    //     config->host_ip_ = FLAGS_host_ip;
    //     config->host_name_ = FLAGS_host_name;
    //     config->host_path_prefix_ = FLAGS_host_path_prefix_;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->mPluginType = logtail::ebpf::PluginType::NETWORK_SECURITY;
    //     return;
    //   } else if (FLAGS_enable_plugin == "PROCESS") {
    //     return;
    //   }
    //   updatePluginFunc(config);
    //   delete config;
    // }
private:
    void *handle;
    init_func initPluginFunc;
    ebpf_poll_events_func noPollFunc;
    poll_plugin_pbs_func pollPbsFunc;
    set_logger_func setLoggerFunc;


    std::thread noPerfWorker;
    // remove_func removePluginFunc;
    // deinit_func deinitPluginFunc;
    // update_func updatePluginFunc;
    // suspend_func suspendPluginFunc;
};

int main(int argc, char *argv[]) {
    auto sm = std::make_unique<source_manager>();
    std::cout << "begin init" << std::endl;
    sm->init();
    std::cout << "init done" << std::endl;
    sm->startProcessPlugin();
    std::cout << "start done" << std::endl;


    std::this_thread::sleep_for(std::chrono::seconds(50));
    std::cout << "begin shutdown ... " << std::endl;
    return 0;
}

