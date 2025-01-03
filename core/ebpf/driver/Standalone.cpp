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
// #include "eBPFWrapper.h"
// #include <security.skel.h>

using init_func = int (*)(void *);
using deinit_func = void (*)();
using remove_func = int(*)(void*);
using update_func=void(*)(void*);
using suspend_func=int(*)(void*);

void HandleStats(std::vector<nami::eBPFStatistics>& stats) {
  for (auto& stat : stats) {
    std::cout << "==== pluginType:" << int(stat.plugin_type_) << " recv kernel events:" << stat.recv_kernel_events_total_ << " loss kernel events:" << stat.loss_kernel_events_total_ << " push events total:" << stat.push_events_total_ << std::endl;
  }
  
  return;
}

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

void HandleBatchEvent(std::vector<std::unique_ptr<ApplicationBatchEvent>>& events) {
  // assert(FLAGS_enable_event_cb == true);
  std::cout << "[HandleBatchEvent] event size:" << events.size();
  for (auto& app_events : events) {
    assert(app_events->app_id_.empty() == false);
    assert(app_events->tags_.empty() == false);
    assert(app_events->events_.empty() == false);
  }
}

void HandleBatchSpan(std::vector<std::unique_ptr<ApplicationBatchSpan>>& spans) {
  // assert(FLAGS_enable_span_cb == true);
  std::cout << "[HandleBatchSpan] span size: " << spans.size() ;
  for (auto& span : spans) {
    std::cout << "APPID: " << span->app_id_;
    std::cout << "APPNAME: " << span->app_name_;
    std::cout << "IP: " << span->host_ip_;
    std::cout << "HOST: " << span->host_name_;
    for (auto& ss : span->single_spans_) {
      for (auto& tag : ss->tags_) {
        std::cout << "      " << tag.first << " : " << tag.second;
      }
      std::cout << "  traceID: " << ss->trace_id_;
      std::cout << "  spanID: " << ss->span_id_;
      std::cout << "  spanKind: " << ss->span_kind_;
      std::cout << "  spanName: " << ss->span_name_;
      std::cout << "  startTS: " << ss->start_timestamp_;
      std::cout << "  startTS: " << ss->end_timestamp_;
    }
  }
}
void HandleBatchMetric(std::vector<std::unique_ptr<ApplicationBatchMeasure>>& measures,
                       uint64_t timestamp) {
    // assert(FLAGS_enable_metric_cb == true);
    std::cout << "[HandleBatchMetric] measures size: " << measures.size();
    std::cout << "============ [main] [HandleBatchMetric] receive measures " << measures.size()
              << " ============ ";
    for (size_t i = 0; i < measures.size(); i++) {
        auto& batch_measure = measures[i];
        std::cout << "APPID: " << batch_measure->app_id_;
        std::cout << "APPNAME: " << batch_measure->app_name_;
        std::cout << "IP: " << batch_measure->ip_;
        std::cout << "HOST: " << batch_measure->host_;
        std::cout << "TIMESTAMP: " << timestamp;

        for (auto& measure : batch_measure->measures_) {
            std::cout << "  Measure Type: " << measure->type_;
            if (MEASURE_TYPE_APP == measure->type_) {
              auto im = static_cast<AppSingleMeasure*>(measure->inner_measure_.get());
              std::cout << "  InnerMeasure duration_ms_sum_: " << im->duration_ms_sum_;
              std::cout << "  InnerMeasure request_total_: " << im->request_total_;
              std::cout << "  InnerMeasure slow_total_: " << im->slow_total_;
              std::cout << "  InnerMeasure error_total_: " << im->error_total_;
              std::cout << "  InnerMeasure status_2xx_count_: " << im->status_2xx_count_;
              std::cout << "  InnerMeasure status_3xx_count_: " << im->status_3xx_count_;
              std::cout << "  InnerMeasure status_4xx_count_: " << im->status_4xx_count_;
              std::cout << "  InnerMeasure status_5xx_count_: " << im->status_5xx_count_;
            }
            for (auto& tag : measure->tags_) {
                std::cout << "      " << tag.first << " : " << tag.second;
            }
            
        }
    }
}

class source_manager {
public:
    source_manager() : handle(nullptr)/*, initPluginFunc(nullptr), deinitPluginFunc(nullptr)*/ {}

    ~source_manager() {
        // clearPlugin();
    }

    void handleBatchNetworkEvent(std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
      std::cout << "============ [main] [handleBatchNetworkEvent] receive event " << events.size() << " ============ " ;
      for (size_t i = 0; i < events.size(); i ++) {
        auto& event = events[i];
        auto tags = event->GetAllTags();
        std::cout << "  EventType:" << static_cast<int>(event->GetEventType()) ;
        for (auto& tag : tags) {
          std::cout << "      " << tag.first << " : " << tag.second ;
        }
      }
      std::cout << "================================================================== " ;
    }
    void handleBatchProcessEvent(std::vector<std::unique_ptr<AbstractSecurityEvent>>& events) {
      std::cout << "============ [main] [handleBatchProcessEvent] receive event " << events.size() << " ============ " ;
      for (size_t i = 0; i < events.size(); i ++) {
        auto& event = events[i];
        auto tags = event->GetAllTags();
        std::cout << "  EventType:" << static_cast<int>(event->GetEventType()) ;
        for (auto& tag : tags) {
          std::cout << "      " << tag.first << " : " << tag.second ;
        }
      }
      std::cout << "================================================================== " ;
    }
    void handleSecureEvent(std::unique_ptr<AbstractSecurityEvent>& event) {
      std::cout << "============ [main] [handleSecureEvent] receive event ============ " << event->GetTimestamp() ;
      auto tags = event->GetAllTags();
      std::cout << "  EventType:" << static_cast<int>(event->GetEventType()) ;
      for (auto& tag : tags) {
        std::cout << "      " << tag.first << " : " << tag.second ;
      }
      std::cout << "================================================================== " << event->GetTimestamp() ;
    }

    void handleBatchFileEvent(std::vector<std::unique_ptr<AbstractSecurityEvent>> &events)
    {
      std::cout << "============ [main] [handleBatchFileEvent] receive event " << events.size() << " ============ ";
      for (size_t i = 0; i < events.size(); i++)
      {
        auto &event = events[i];
        auto tags = event->GetAllTags();
        std::cout << "  EventType:" << static_cast<int>(event->GetEventType());
        for (auto &tag : tags)
        {
          std::cout << "      " << tag.first << " : " << tag.second;
        }
      }
      std::cout << "================================================================== ";
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
        auto ebpf_config = std::make_unique<nami::PluginConfig>();
        nami::ProcessConfig config;
        ebpf_config->mPluginType = nami::PluginType::PROCESS_SECURITY;
        ebpf_config->mConfig = config;

        std::cout << "begin to call init func" << std::endl;
        initPluginFunc(ebpf_config.get());
        std::cout << "after call init func" << std::endl;
        return true;
    }

    bool startNetworkPlugin() {
      

      // void* init_param = nullptr;
      auto ebpf_config = std::make_unique<nami::PluginConfig>();

        // compose param
        nami::NetworkObserveConfig config;
        std::string coolbpfPaht = "./libcoolbpf.so.1.0.0";
        config.so_ = coolbpfPaht;
        config.so_size_ = coolbpfPaht.length();
        Dl_info dlinfo;
        int err;
        void* cleanup_dog_ptr = dlsym(handle, "ebpf_cleanup_dog");
        if (nullptr == cleanup_dog_ptr) {
          std::cout << "[SourceManager] get ebpf_cleanup_dog address failed!" ;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_cleanup_dog address" ;
        }

        err = dladdr(cleanup_dog_ptr, &dlinfo);
        if (!err)
        {
          std::cout << "[SourceManager] ebpf_cleanup_dog laddr failed, err:" << strerror(err);
          // printf(:%s\n", strerror(err));
        } else {
          config.uprobe_offset_ = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_cleanup_dog dlinfo, uprobe_offset:" << config.uprobe_offset_;
        }

        void* ebpf_update_conn_addr_ptr = dlsym(handle, "ebpf_update_conn_addr");
        if (nullptr == ebpf_update_conn_addr_ptr) {
          std::cout << "[SourceManager] get ebpf_update_conn_addr address failed!" ;
        } else {
          std::cout << "[SourceManager] successfully get ebpf_update_conn_addr address" ;
        }
        err = dladdr(ebpf_update_conn_addr_ptr, &dlinfo);
        if (!err)
        {
          printf("[SourceManager] ebpf_update_conn_addr laddr failed, err:%s\n", strerror(err));
        } else {
          config.upca_offset_ = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_update_conn_addr dlinfo, upca_offset" << config.upca_offset_ ;
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
          config.upps_offset_ = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_disable_process dlinfo, upps_offset:" << config.upps_offset_ ;
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
          config.upcr_offset_ = (long)dlinfo.dli_saddr - (long)dlinfo.dli_fbase;
          std::cout << "[SourceManager] successfully get ebpf_update_conn_role dlinfo, upcr_offset:" << config.upcr_offset_ ;
        }
        config.enable_cid_filter = false;
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
        config.mPrintHandler = [](int16_t level, const char *format, va_list args){
            
            char buffer[1024] = {0};
            vsnprintf(buffer, 1023, format, args);
            std::cout << buffer;
            return 0;
        };
        // config.enable_container_ids_ = {"5cb30fc9cfc3d30a2b561a20760d69a191bcd710dda5e258e1be05a0af1a5ed1"};
        ebpf_config->mPluginType = nami::PluginType::NETWORK_OBSERVE;
        ebpf_config->mConfig = config;        

      std::cout << "begin to call init func" << std::endl;

      initPluginFunc(ebpf_config.get());
      return true;
    }

    // void removePlugin() {
    //   nami::PluginConfig * config = new nami::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_OBSERVE;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::FILE_SECURITY;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_SECURITY;
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
    //   nami::PluginConfig * config = new nami::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_OBSERVE;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::FILE_SECURITY;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_SECURITY;
    //   }
    //   suspendPluginFunc(config);
    //   delete config;
    // }

    // void updatePlugin() {
    //   nami::PluginConfig * config = new nami::PluginConfig;
    //   if (FLAGS_enable_plugin == "NETWORK") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_OBSERVE;
    //     config->type = UpdataType::OBSERVER_UPDATE_TYPE_CHANGE_WHITELIST;
    //     nami::NetworkObserveConfig nconfig;
    //     nconfig.enable_container_ids_ = {"5cb30fc9cfc3d30a2b561a20760d69a191bcd710dda5e258e1be05a0af1a5ed1"};
    //     nconfig.enable_cid_filter = false;
    //     config->config_ = nconfig;
    //   } else if (FLAGS_enable_plugin == "FILE_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::FILE_SECURITY;
    //     nami::FileSecurityConfig fconfig;
    //     nami::SecurityOption opt;
    //     opt.call_names_ = {"security_file_permission"};
    //     if (FLAGS_update_path_filter.size()) {
    //       auto pathes = split(FLAGS_update_path_filter, ',');
    //       nami::SecurityFileFilter filter;
    //       for (auto path : pathes) {
    //         filter.mFilePathList.emplace_back(std::move(path));
    //       }
    //       opt.filter_ = filter;
    //     }
    //     fconfig.options_ = {opt};
    //     fconfig.file_security_cb_ = std::bind(&source_manager::handleBatchFileEvent, this, std::placeholders::_1);
    //     config->plugin_type_ = nami::PluginType::FILE_SECURITY;
    //     config->stats_handler_ = HandleStats;
    //     config->config_ = fconfig;
    //     config->host_ip_ = FLAGS_host_ip;
    //     config->host_name_ = FLAGS_host_name;
    //     config->host_path_prefix_ = FLAGS_host_path_prefix_;
    //   } else if (FLAGS_enable_plugin == "NETWORK_SECURITY") {
    //     config->plugin_type_ = nami::PluginType::NETWORK_SECURITY;
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

