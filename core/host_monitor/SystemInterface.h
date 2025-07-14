/*
 * Copyright 2025 iLogtail Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "collector/MetricCalculate.h"
#include "common/Flags.h"
#include "common/ProcParser.h"

DECLARE_FLAG_INT32(system_interface_default_cache_ttl);

namespace logtail {

struct BaseInformation {
    std::chrono::steady_clock::time_point collectTime;
};

struct SystemInformation : public BaseInformation {
    int64_t bootTime;
};

// man proc: https://man7.org/linux/man-pages/man5/proc.5.html
// search key: /proc/stat
enum class EnumCpuKey : int {
    user = 1,
    nice,
    system,
    idle,
    iowait, // since Linux 2.5.41
    irq, // since Linux 2.6.0
    softirq, // since Linux 2.6.0
    steal, // since Linux 2.6.11
    guest, // since Linux 2.6.24
    guest_nice, // since Linux 2.6.33
};

struct CPUStat {
    int32_t index; // -1 means total cpu
    double user;
    double nice;
    double system;
    double idle;
    double iowait;
    double irq;
    double softirq;
    double steal;
    double guest;
    double guestNice;
};

struct CPUInformation : public BaseInformation {
    std::vector<CPUStat> stats;
};

struct ProcessListInformation : public BaseInformation {
    std::vector<pid_t> pids;
};

struct ProcessInformation : public BaseInformation {
    ProcessStat stat; // shared data structrue with eBPF process
};

// /proc/loadavg
struct SystemStat {
    double load1;
    double load5;
    double load15;
    double load1PerCore;
    double load5PerCore;
    double load15PerCore;

    // Define the field descriptors
    static inline const FieldName<SystemStat> systemMetricFields[] = {
        FIELD_ENTRY(SystemStat, load1),
        FIELD_ENTRY(SystemStat, load5),
        FIELD_ENTRY(SystemStat, load15),
        FIELD_ENTRY(SystemStat, load1PerCore),
        FIELD_ENTRY(SystemStat, load5PerCore),
        FIELD_ENTRY(SystemStat, load15PerCore),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<SystemStat, double>&)>& callback) {
        for (const auto& field : systemMetricFields) {
            callback(field);
        }
    }
};

enum EnumTcpState : int8_t {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING,
    TCP_IDLE,
    TCP_BOUND,
    TCP_UNKNOWN,
    TCP_TOTAL,
    TCP_NON_ESTABLISHED,

    TCP_STATE_END, // 仅用于状态计数
};

struct NetState {
    uint64_t tcpStates[TCP_STATE_END] = {0};
    unsigned int tcpInboundTotal = 0;
    unsigned int tcpOutboundTotal = 0;
    unsigned int allInboundTotal = 0;
    unsigned int allOutboundTotal = 0;

    void calcTcpTotalAndNonEstablished();
    std::string toString(const char* lf = "\n", const char* tab = "    ") const;
    bool operator==(const NetState&) const;

    inline bool operator!=(const NetState& r) const { return !(*this == r); }
};

struct NetLinkRequest {
    struct nlmsghdr nlh;
    struct inet_diag_req r;
};

// /proc/net/snmp  tcp:
enum class EnumNetSnmpTCPKey : int {
    RtoAlgorithm = 1,
    RtoMin,
    RtoMax,
    MaxConn,
    ActiveOpens,
    PassiveOpens,
    AttemptFails,
    EstabResets,
    CurrEstab,
    InSegs,
    OutSegs,
    RetransSegs,
    InErrs,
    OutRsts,
    InCsumErrors,
};

struct NetInterfaceMetric {
    // received
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    uint64_t rxErrors = 0;
    uint64_t rxDropped = 0;
    uint64_t rxOverruns = 0;
    uint64_t rxFrame = 0;
    // transmitted
    uint64_t txPackets = 0;
    uint64_t txBytes = 0;
    uint64_t txErrors = 0;
    uint64_t txDropped = 0;
    uint64_t txOverruns = 0;
    uint64_t txCollisions = 0;
    uint64_t txCarrier = 0;

    int64_t speed = 0;
    std::string name;
};

struct NetAddress {
    enum { SI_AF_UNSPEC, SI_AF_INET, SI_AF_INET6, SI_AF_LINK } family;
    union {
        uint32_t in;
        uint32_t in6[4];
        unsigned char mac[8];
    } addr;

    NetAddress();
    std::string str() const;
};

struct InterfaceConfig {
    std::string name;
    std::string type;
    std::string description;
    NetAddress hardWareAddr;

    NetAddress address;
    NetAddress destination;
    NetAddress broadcast;
    NetAddress netmask;

    NetAddress address6;
    int prefix6Length = 0;
    int scope6 = 0;

    uint64_t mtu = 0;
    uint64_t metric = 0;
    int txQueueLen = 0;
};

// TCP各种状态下的连接数
struct ResTCPStat {
    uint64_t tcpEstablished;
    uint64_t tcpListen;
    uint64_t tcpTotal;
    uint64_t tcpNonEstablished;

    static inline const FieldName<ResTCPStat, uint64_t> resTCPStatFields[] = {
        FIELD_ENTRY(ResTCPStat, tcpEstablished),
        FIELD_ENTRY(ResTCPStat, tcpListen),
        FIELD_ENTRY(ResTCPStat, tcpTotal),
        FIELD_ENTRY(ResTCPStat, tcpNonEstablished),
    };

    static void enumerate(const std::function<void(const FieldName<ResTCPStat, uint64_t>&)>& callback) {
        for (auto& field : resTCPStatFields) {
            callback(field);
        }
    };
};


// 每秒发包数，上行带宽，下行带宽.每秒发送错误包数量
struct ResNetRatePerSec {
    double rxPackRate;
    double txPackRate;
    double rxByteRate;
    double txByteRate;
    double txErrorRate;
    double rxErrorRate;
    double rxDropRate;
    double txDropRate;


    static inline const FieldName<ResNetRatePerSec> resRatePerSecFields[] = {
        FIELD_ENTRY(ResNetRatePerSec, rxPackRate),
        FIELD_ENTRY(ResNetRatePerSec, txPackRate),
        FIELD_ENTRY(ResNetRatePerSec, rxByteRate),
        FIELD_ENTRY(ResNetRatePerSec, txByteRate),
        FIELD_ENTRY(ResNetRatePerSec, txErrorRate),
        FIELD_ENTRY(ResNetRatePerSec, rxErrorRate),
        FIELD_ENTRY(ResNetRatePerSec, rxDropRate),
        FIELD_ENTRY(ResNetRatePerSec, txDropRate),
    };
    static void enumerate(const std::function<void(const FieldName<ResNetRatePerSec, double>&)>& callback) {
        for (auto& field : resRatePerSecFields) {
            callback(field);
        }
    };
};

struct SystemLoadInformation : public BaseInformation {
    SystemStat systemStat;
};

struct CpuCoreNumInformation : public BaseInformation {
    unsigned int cpuCoreNum;
};

struct TCPStatInformation : public BaseInformation {
    ResTCPStat stat;
};

struct NetRateInformation : public BaseInformation {
    std::vector<NetInterfaceMetric> metrics;
};

struct NetInterfaceInformation : public BaseInformation {
    std::vector<InterfaceConfig> configs;
};

struct TupleHash {
    template <typename... T>
    std::size_t operator()(const std::tuple<T...>& t) const {
        size_t seed = 0;
        std::apply(
            [&](const T&... args) { ((seed ^= std::hash<T>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)), ...); },
            t);
        return seed;
    }
};

struct MemoryStat {
    double ram = 0;
    double total = 0;
    double used = 0;
    double free = 0;
    double available = 0;
    double actualUsed = 0;
    double actualFree = 0;
    double buffers = 0;
    double cached = 0;
    double usedPercent = 0.0;
    double freePercent = 0.0;

    static inline const FieldName<MemoryStat> memStatMetas[] = {
        FIELD_ENTRY(MemoryStat, ram),
        FIELD_ENTRY(MemoryStat, total),
        FIELD_ENTRY(MemoryStat, used),
        FIELD_ENTRY(MemoryStat, free),
        FIELD_ENTRY(MemoryStat, available),
        FIELD_ENTRY(MemoryStat, actualUsed),
        FIELD_ENTRY(MemoryStat, actualFree),
        FIELD_ENTRY(MemoryStat, buffers),
        FIELD_ENTRY(MemoryStat, cached),
        FIELD_ENTRY(MemoryStat, usedPercent),
        FIELD_ENTRY(MemoryStat, freePercent),
    };

    static void enumerate(const std::function<void(const FieldName<MemoryStat>&)>& callback) {
        for (const auto& field : memStatMetas) {
            callback(field);
        }
    }
};

struct MemoryInformation : public BaseInformation {
    MemoryStat memStat;
};

class SystemInterface {
public:
    template <typename InfoT, typename... Args>
    class SystemInformationCache {
    public:
        SystemInformationCache(std::chrono::milliseconds ttl) : mTTL(ttl) {}
        bool GetWithTimeout(InfoT& info, std::chrono::milliseconds timeout, Args... args);
        bool Set(InfoT& info, Args... args);
        bool GC();

    private:
        std::mutex mMutex;
        std::unordered_map<std::tuple<Args...>, std::pair<InfoT, std::atomic_bool>, TupleHash> mCache;
        std::condition_variable mConditionVariable;
        std::chrono::milliseconds mTTL;

#ifdef APSARA_UNIT_TEST_MAIN
        friend class SystemInterfaceUnittest;
#endif
    };

    template <typename InfoT>
    class SystemInformationCache<InfoT> {
    public:
        SystemInformationCache(std::chrono::milliseconds ttl) : mTTL(ttl) {}
        bool GetWithTimeout(InfoT& info, std::chrono::milliseconds timeout);
        bool Set(InfoT& info);
        bool GC();

    private:
        std::mutex mMutex;
        std::pair<InfoT, std::atomic_bool> mCache;
        std::condition_variable mConditionVariable;
        std::chrono::milliseconds mTTL;

#ifdef APSARA_UNIT_TEST_MAIN
        friend class SystemInterfaceUnittest;
#endif
    };

    SystemInterface(const SystemInterface&) = delete;
    SystemInterface(SystemInterface&&) = delete;
    SystemInterface& operator=(const SystemInterface&) = delete;
    SystemInterface& operator=(SystemInterface&&) = delete;

    static SystemInterface* GetInstance();

    bool GetSystemInformation(SystemInformation& systemInfo);
    bool GetCPUInformation(CPUInformation& cpuInfo);
    bool GetProcessListInformation(ProcessListInformation& processListInfo);
    bool GetProcessInformation(pid_t pid, ProcessInformation& processInfo);
    bool GetSystemLoadInformation(SystemLoadInformation& systemLoadInfo);
    bool GetCPUCoreNumInformation(CpuCoreNumInformation& cpuCoreNumInfo);
    bool GetHostMemInformationStat(MemoryInformation& meminfo);

    bool GetTCPStatInformation(TCPStatInformation& tcpStatInfo);
    bool GetNetRateInformation(NetRateInformation& netRateInfo);
    bool GetNetInterfaceInformation(NetInterfaceInformation& netInterfaceInfo);
    explicit SystemInterface(std::chrono::milliseconds ttl
                             = std::chrono::milliseconds{INT32_FLAG(system_interface_default_cache_ttl)})
        : mSystemInformationCache(),
          mCPUInformationCache(ttl),
          mProcessListInformationCache(ttl),
          mProcessInformationCache(ttl),
          mSystemLoadInformationCache(ttl),
          mCPUCoreNumInformationCache(ttl),
          mMemInformationCache(ttl),
          mTCPStatInformationCache(ttl),
          mNetRateInformationCache(ttl),
          mNetInterfaceInformationCache(ttl) {}
    virtual ~SystemInterface() = default;

private:
    template <typename F, typename InfoT, typename... Args>
    bool MemoizedCall(SystemInformationCache<InfoT, Args...>& cache,
                      F&& func,
                      InfoT& info,
                      const std::string& errorType,
                      Args... args);

    virtual bool GetSystemInformationOnce(SystemInformation& systemInfo) = 0;
    virtual bool GetCPUInformationOnce(CPUInformation& cpuInfo) = 0;
    virtual bool GetProcessListInformationOnce(ProcessListInformation& processListInfo) = 0;
    virtual bool GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) = 0;
    virtual bool GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) = 0;
    virtual bool GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) = 0;
    virtual bool GetHostMemInformationStatOnce(MemoryInformation& meminfoStr) = 0;
    virtual bool GetTCPStatInformationOnce(TCPStatInformation& tcpStatInfo) = 0;
    virtual bool GetNetRateInformationOnce(NetRateInformation& netRateInfo) = 0;
    virtual bool GetNetInterfaceInformationOnce(NetInterfaceInformation& netInterfaceInfo) = 0;

    SystemInformation mSystemInformationCache;
    SystemInformationCache<CPUInformation> mCPUInformationCache;
    SystemInformationCache<ProcessListInformation> mProcessListInformationCache;
    SystemInformationCache<ProcessInformation, pid_t> mProcessInformationCache;
    SystemInformationCache<SystemLoadInformation> mSystemLoadInformationCache;
    SystemInformationCache<CpuCoreNumInformation> mCPUCoreNumInformationCache;
    SystemInformationCache<MemoryInformation> mMemInformationCache;
    SystemInformationCache<TCPStatInformation> mTCPStatInformationCache;
    SystemInformationCache<NetRateInformation> mNetRateInformationCache;
    SystemInformationCache<NetInterfaceInformation> mNetInterfaceInformationCache;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class SystemInterfaceUnittest;
#endif
};

} // namespace logtail
