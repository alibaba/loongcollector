//
// Created by qianlu on 2024/6/19.
//

#pragma once

#include <string>
#include <variant>
#include <vector>

extern "C" {
#include <coolbpf/net.h>
}


inline constexpr int kErrDriverInternal = 1;
inline constexpr int kErrDriverInvalidParam = 2;

namespace logtail::ebpf {

using PerfBufferSampleHandler = void (*)(void* ctx, int cpu, void* data, uint32_t size);
using PerfBufferLostHandler = void (*)(void* ctx, int cpu, unsigned long long cnt);
using eBPFLogHandler = int (*)(int16_t level, const char* format, va_list args);

struct WorkloadSelector {
    std::string mWorkloadName;
    std::string mWorkloadKind;
    std::string mNamespace;
};

struct L7Config {
    bool mEnable = false;
    bool mEnableSpan = false;
    bool mEnableMetric = false;
    bool mEnableLog = false;
    double mSampleRate = 0.01;
};

struct L4Config {
    bool mEnable = false;
    // int mMaxConnections = 5000;
};

struct ApmConfig {
    std::string mWorkspace;
    std::string mAppName;
    std::string mAppId; // optional, if not configured by backend, we need calculate it ...
    std::string mServiceId;
};

struct ObserverNetworkOption {
    L7Config mL7Config; // L7 层配置，必填
    L4Config mL4Config; // L4 层配置，必填
    ApmConfig mApmConfig; // 应用配置
    std::vector<WorkloadSelector> mSelectors; // 非必填
};

struct PerfBufferSpec {
public:
    PerfBufferSpec(
        const std::string& name, ssize_t size, void* ctx, PerfBufferSampleHandler scb, PerfBufferLostHandler lcb)
        : mName(name), mSize(size), mCtx(ctx), mSampleHandler(scb), mLostHandler(lcb) {}
    std::string mName;
    ssize_t mSize = 0;
    void* mCtx;
    PerfBufferSampleHandler mSampleHandler;
    PerfBufferLostHandler mLostHandler;
};


enum class PluginType {
    NETWORK_OBSERVE,
    PROCESS_OBSERVE,
    FILE_OBSERVE,
    PROCESS_SECURITY,
    FILE_SECURITY,
    NETWORK_SECURITY,
    MAX,
};

// file
struct SecurityFileFilter {
    std::vector<std::string> mFilePathList;
    bool operator==(const SecurityFileFilter& other) const { return mFilePathList == other.mFilePathList; }
};

// network
struct SecurityNetworkFilter {
    std::vector<std::string> mDestAddrList;
    std::vector<uint32_t> mDestPortList;
    std::vector<std::string> mDestAddrBlackList;
    std::vector<uint32_t> mDestPortBlackList;
    std::vector<std::string> mSourceAddrList;
    std::vector<uint32_t> mSourcePortList;
    std::vector<std::string> mSourceAddrBlackList;
    std::vector<uint32_t> mSourcePortBlackList;
    bool operator==(const SecurityNetworkFilter& other) const {
        return mDestAddrList == other.mDestAddrList && mDestPortList == other.mDestPortList
            && mDestAddrBlackList == other.mDestAddrBlackList && mDestPortBlackList == other.mDestPortBlackList
            && mSourceAddrList == other.mSourceAddrList && mSourcePortList == other.mSourcePortList
            && mSourceAddrBlackList == other.mSourceAddrBlackList && mSourcePortBlackList == other.mSourcePortBlackList;
    }
};

struct SecurityOption {
    std::vector<std::string> mCallNames;
    std::variant<std::monostate, SecurityFileFilter, SecurityNetworkFilter> mFilter;
    bool operator==(const SecurityOption& other) const {
        return mCallNames == other.mCallNames && mFilter == other.mFilter;
    }
};

struct NetworkObserveConfig {
    std::string mBtf;
    std::string mSo;
    long mUprobeOffset;
    long mUpcaOffset;
    long mUppsOffset;
    long mUpcrOffset;

    void* mCustomCtx;
    eBPFLogHandler mLogHandler;

    // perfworkers ...
    net_ctrl_process_func_t mCtrlHandler = nullptr;
    net_data_process_func_t mDataHandler = nullptr;
    net_statistics_process_func_t mStatsHandler = nullptr;
    net_lost_func_t mLostHandler = nullptr;

    bool mEnableCidFilter = false;
    int mCidOffset = -1;

    std::vector<std::string> mEnableContainerIds;
    std::vector<std::string> mDisableContainerIds;
};

struct ProcessConfig {
    std::vector<SecurityOption> mOptions;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const ProcessConfig& other) const { return mOptions == other.mOptions; }
};

struct NetworkSecurityConfig {
    std::vector<SecurityOption> mOptions;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const NetworkSecurityConfig& other) const { return mOptions == other.mOptions; }
};

struct FileSecurityConfig {
    std::vector<SecurityOption> mOptions;
    std::vector<PerfBufferSpec> mPerfBufferSpec;
    bool operator==(const FileSecurityConfig& other) const { return mOptions == other.mOptions; }
};

enum class eBPFLogType {
    NAMI_LOG_TYPE_WARN = 0,
    NAMI_LOG_TYPE_INFO,
    NAMI_LOG_TYPE_DEBUG,
};

struct PluginConfig {
    PluginType mPluginType;
    // log control
    std::variant<NetworkObserveConfig, ProcessConfig, NetworkSecurityConfig, FileSecurityConfig> mConfig;
};

} // namespace logtail::ebpf
