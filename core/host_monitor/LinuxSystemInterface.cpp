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

#include "host_monitor/LinuxSystemInterface.h"

#include <chrono>
#include <string>

using namespace std;
using namespace std::chrono;

#include <grp.h>
#include <pwd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "logger/Logger.h"

namespace logtail {

bool LinuxSystemInterface::GetHostSystemStat(vector<string>& lines, string& errorMessage) {
    errorMessage.clear();
    if (!CheckExistance(PROCESS_DIR / PROCESS_STAT)) {
        errorMessage = "file does not exist: " + (PROCESS_DIR / PROCESS_STAT).string();
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_STAT, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}

double ParseMetric(const std::vector<std::string>& cpuMetric, EnumCpuKey key) {
    if (cpuMetric.size() <= static_cast<size_t>(key)) {
        return 0.0;
    }
    double value = 0.0;
    if (!StringTo(cpuMetric[static_cast<size_t>(key)], value)) {
        LOG_WARNING(
            sLogger,
            ("failed to parse cpu metric", static_cast<size_t>(key))("value", cpuMetric[static_cast<size_t>(key)]));
    }
    return value;
}

unsigned int Hex2Int(const std::string& s) {
    std::istringstream in(s);
    in >> std::hex;
    unsigned int res;
    in >> res;
    bool success = !in.fail();
    return success ? res : 0;
}

bool IsInterfaceExists(const std::string& interfaceName) {
    std::filesystem::path interfacePath = "/sys/class/net/" + interfaceName;
    return std::filesystem::exists(interfacePath);
}

bool LinuxSystemInterface::GetHostLoadavg(vector<string>& lines, string& errorMessage) {
    errorMessage.clear();
    if (!CheckExistance(PROCESS_DIR / PROCESS_LOADAVG)) {
        errorMessage = "file does not exist: " + (PROCESS_DIR / PROCESS_LOADAVG).string();
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_LOADAVG, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}
bool LinuxSystemInterface::ReadSocketStat(const std::filesystem::path& path, uint64_t& tcp) {
    tcp = 0;
    if (!path.empty()) {
        std::vector<std::string> sockstatLines;
        std::string errorMessage;
        if (!CheckExistance(path)) {
            errorMessage = "file does not exist: " + (path).string();
            return false;
        }

        if (GetFileLines(path, sockstatLines, true, &errorMessage) != 0 || sockstatLines.empty()) {
            return false;
        }

        for (auto const& line : sockstatLines) {
            if (line.size() >= 5 && (line.substr(0, 4) == "TCP:" || line.substr(0, 5) == "TCP6:")) {
                std::vector<std::string> metrics;
                boost::split(metrics, line, boost::is_any_of(" "), boost::token_compress_on);
                if (metrics.size() >= 9) {
                    tcp += static_cast<uint64_t>(std::stoull(metrics[6])); // tw
                    tcp += static_cast<uint64_t>(std::stoull(metrics[8])); // alloc
                }
            }
        }
    }
    return true;
}

bool LinuxSystemInterface::ReadNetLink(std::vector<uint64_t>& tcpStateCount) {
    static std::atomic_int sequence_number = 1;
    int fd;
    // struct inet_diag_msg *r;
    // 使用netlink socket与内核通信
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG);
    if (fd < 0) {
        LOG_WARNING(sLogger,
                    ("ReadNetLink, socket(AF_NETLINK, SOCK_RAW, NETLINK_INET_DIAG) failed, error msg: ",
                     std::string(strerror(errno))));
        return false;
    }


    // 存在多个netlink socket时，必须单独bind,并通过nl_pid来区分
    struct sockaddr_nl nladdr_bind {};
    memset(&nladdr_bind, 0, sizeof(nladdr_bind));
    nladdr_bind.nl_family = AF_NETLINK;
    nladdr_bind.nl_pad = 0;
    nladdr_bind.nl_pid = getpid();
    nladdr_bind.nl_groups = 0;
    if (bind(fd, (struct sockaddr*)&nladdr_bind, sizeof(nladdr_bind))) {
        LOG_WARNING(sLogger, ("ReadNetLink, bind netlink socket failed, error msg: ", std::string(strerror(errno))));
        close(fd);
        return false;
    }
    struct sockaddr_nl nladdr {};
    memset(&nladdr, 0, sizeof(nladdr));
    nladdr.nl_family = AF_NETLINK;
    struct NetLinkRequest req {};
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = sizeof(req);
    req.nlh.nlmsg_type = TCPDIAG_GETSOCK;
    req.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
    // sendto kernel
    req.nlh.nlmsg_pid = getpid();
    req.nlh.nlmsg_seq = ++sequence_number;
    req.r.idiag_family = AF_INET;
    req.r.idiag_states = 0xfff;
    req.r.idiag_ext = 0;
    struct iovec iov {};
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = &req;
    iov.iov_len = sizeof(req);
    struct msghdr msg {};
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void*)&nladdr;
    msg.msg_namelen = sizeof(nladdr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(fd, &msg, 0) < 0) {
        LOG_WARNING(sLogger, ("ReadNetLink, sendmsg(2) failed, error msg: ", std::string(strerror(errno))));
        close(fd);
        return false;
    }
    char buf[8192];
    iov.iov_base = buf;
    iov.iov_len = sizeof(buf);

    uint64_t received_count = 0;
    uint64_t MAX_RECV_COUNT = 10000;
    while (received_count < MAX_RECV_COUNT) {
        received_count++;
        // struct nlmsghdr *h;
        memset(&msg, 0, sizeof(msg));
        msg.msg_name = (void*)&nladdr;
        msg.msg_namelen = sizeof(nladdr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        ssize_t status = recvmsg(fd, (struct msghdr*)&msg, 0);
        if (status < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            LOG_WARNING(sLogger, ("ReadNetLink, recvmsg(2) failed, error msg: ", std::string(strerror(errno))));
            close(fd);
            return false;
        } else if (status == 0) {
            LOG_WARNING(sLogger,
                        ("ReadNetLink, Unexpected zero-sized  reply from netlink socket. error msg: ",
                         std::string(strerror(errno))));
            close(fd);
            return true;
        }

        // h = (struct nlmsghdr *) buf;
        for (auto h = (struct nlmsghdr*)buf; NLMSG_OK(h, status); h = NLMSG_NEXT(h, status)) {
            if (static_cast<uint64_t>(h->nlmsg_seq) != static_cast<uint64_t>(sequence_number)) {
                // sequence_number is not equal
                // h = NLMSG_NEXT(h, status);
                continue;
            }

            if (h->nlmsg_type == NLMSG_DONE) {
                close(fd);
                return true;
            } else if (h->nlmsg_type == NLMSG_ERROR) {
                if (h->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                    LOG_WARNING(sLogger, ("ReadNetLink ", "message truncated"));
                } else {
                    auto msg_error = (struct nlmsgerr*)NLMSG_DATA(h);
                    LOG_WARNING(sLogger, ("ReadNetLink, Received error, error msg: ", msg_error));
                }
                close(fd);
                return false;
            }
            auto r = (struct inet_diag_msg*)NLMSG_DATA(h);
            /*This code does not(need to) distinguish between IPv4 and IPv6.*/
            if (r->idiag_state > TCP_CLOSING || r->idiag_state < TCP_ESTABLISHED) {
                // Ignoring connection with unknown state
                continue;
            }
            tcpStateCount[r->idiag_state]++;
            // h = NLMSG_NEXT(h, status);
        }
    }
    close(fd);
    return true;
}

bool LinuxSystemInterface::GetNetStateByNetLink(NetState& netState) {
    std::vector<uint64_t> tcpStateCount(TCP_CLOSING + 1, 0);
    if (ReadNetLink(tcpStateCount) == false) {
        return false;
    }
    uint64_t tcp = 0, tcpSocketStat = 0;

    if (ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT, tcp)) {
        tcpSocketStat += tcp;
    }
    if (ReadSocketStat(PROCESS_DIR / PROCESS_NET_SOCKSTAT6, tcp)) {
        tcpSocketStat += tcp;
    }

    int total = 0;
    for (int i = TCP_ESTABLISHED; i <= TCP_CLOSING; i++) {
        if (i == TCP_SYN_SENT || i == TCP_SYN_RECV) {
            total += tcpStateCount[i];
        }
        netState.tcpStates[i] = tcpStateCount[i];
    }
    // 设置为-1表示没有采集
    netState.tcpStates[TCP_TOTAL] = total + tcpSocketStat;
    netState.tcpStates[TCP_NON_ESTABLISHED] = netState.tcpStates[TCP_TOTAL] - netState.tcpStates[TCP_ESTABLISHED];
    return true;
}

bool LinuxSystemInterface::GetHostNetDev(vector<string>& lines, string& errorMessage) {
    errorMessage.clear();
    if (!CheckExistance(PROCESS_DIR / PROCESS_NET_DEV)) {
        errorMessage = "file does not exist: " + (PROCESS_DIR / PROCESS_NET_DEV).string();
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_NET_DEV, lines, true, &errorMessage);
    if (ret != 0 || lines.empty()) {
        return false;
    }
    return true;
}


bool LinuxSystemInterface::GetInterfaceConfig(InterfaceConfig& interfaceConfig, const std::string& name) {
    // 检查网络接口是否存在
    if (!IsInterfaceExists(name)) {
        LOG_WARNING(sLogger, ("Interface does not exist.", name));
        return false;
    }
    int sock;
    ifreq ifr{};
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_WARNING(sLogger, ("socket(AF_INET, SOCK_DGRAM, 0)", std::string(strerror(errno))));
        return false;
    }
    {
        interfaceConfig.name = name;
        strncpy(ifr.ifr_name, name.c_str(), sizeof(ifr.ifr_name));
        ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = '\0';

        if (!ioctl(sock, SIOCGIFADDR, &ifr)) {
            interfaceConfig.address.addr.in = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
            interfaceConfig.address.family = NetAddress::SI_AF_INET;
        }

        if (!ioctl(sock, SIOCGIFNETMASK, &ifr)) {
            interfaceConfig.netmask.addr.in = ((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
            interfaceConfig.netmask.family = NetAddress::SI_AF_INET;
        }

        if (!ioctl(sock, SIOCGIFMTU, &ifr)) {
            interfaceConfig.mtu = ifr.ifr_mtu;
        }

        if (!ioctl(sock, SIOCGIFMETRIC, &ifr)) {
            interfaceConfig.metric = ifr.ifr_metric ? ifr.ifr_metric : 1;
        }

        if (!ioctl(sock, SIOCGIFTXQLEN, &ifr)) {
            interfaceConfig.txQueueLen = ifr.ifr_qlen;
        } else {
            interfaceConfig.txQueueLen = -1; /* net-tools behaviour */
        }
    }

    interfaceConfig.description = name;
    interfaceConfig.address6.family = NetAddress::SI_AF_INET6;
    interfaceConfig.prefix6Length = 0;
    interfaceConfig.scope6 = 0;

    // ipv6
    std::vector<std::string> netInet6Lines = {};
    std::string errorMessage;
    if (CheckExistance(PROCESS_DIR / PROCESS_NET_IF_INET6)) {
        int ret = GetFileLines(PROCESS_DIR / PROCESS_NET_IF_INET6, netInet6Lines, true, &errorMessage);
        if (ret != 0 || netInet6Lines.empty()) {
            // Failure should not be returned without "/proc/net/if_inet6"
            close(sock);
            return false;
        }
    }


    enum {
        Inet6Address, // 长度为32的16进制IPv6地址
        Inet6DevNo, // netlink设备号
        Inet6PrefixLen, // 16进制表示的 prefix length
        Inet6Scope, // scope
    };
    for (auto& devLine : netInet6Lines) {
        std::vector<std::string> netInet6Metric;
        boost::split(netInet6Metric, devLine, boost::is_any_of(" "), boost::token_compress_on);
        std::string inet6Name = netInet6Metric.back();
        boost::algorithm::trim(inet6Name);
        if (inet6Name == name) {
            // Doc: https://ata.atatech.org/articles/11020228072?spm=ata.25287382.0.0.1c647536bhA7NG#lyRD52DR
            if (Inet6Address < netInet6Metric.size()) {
                auto* addr6 = (unsigned char*)&(interfaceConfig.address6.addr.in6);

                std::string addr = netInet6Metric[Inet6Address];

                constexpr const int addrLen = 16;
                for (size_t i = 0; i < addrLen; ++i) {
                    // 确保不会越界
                    if (i * 2 + 1 >= addr.size()) {
                        break; // 或者处理错误情况
                    }
                    std::string byteStr = addr.substr(i * 2, 2); // 提取两个字符
                    addr6[i] = static_cast<unsigned char>(Hex2Int(byteStr)); // 转换为字节
                }
            }
            if (Inet6PrefixLen < netInet6Metric.size()) {
                interfaceConfig.prefix6Length = Hex2Int(netInet6Metric[Inet6PrefixLen]);
            }
            if (Inet6Scope < netInet6Metric.size()) {
                interfaceConfig.scope6 = Hex2Int(netInet6Metric[Inet6Scope]);
            }
        }
    }

    close(sock);
    return true;
}

bool LinuxSystemInterface::GetSystemInformationOnce(SystemInformation& systemInfo) {
    std::vector<std::string> lines;
    std::string errorMessage;
    if (!GetHostSystemStat(lines, errorMessage)) {
        LOG_ERROR(sLogger, ("failed to get system information", errorMessage));
        return false;
    }
    for (auto const& line : lines) {
        auto cpuMetric = SplitString(line);
        // example: btime 1719922762
        if (cpuMetric.size() >= 2 && cpuMetric[0] == "btime") {
            if (!StringTo(cpuMetric[1], systemInfo.bootTime)) {
                LOG_WARNING(sLogger,
                            ("failed to get system boot time", "use current time instead")("error msg", cpuMetric[1]));
                return false;
            }
            break;
        }
    }
    systemInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetCPUInformationOnce(CPUInformation& cpuInfo) {
    std::vector<std::string> cpuLines;
    std::string errorMessage;
    if (!GetHostSystemStat(cpuLines, errorMessage)) {
        return false;
    }
    // cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0
    // cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0
    // ...
    cpuInfo.stats.clear();
    cpuInfo.stats.reserve(cpuLines.size());
    for (auto const& line : cpuLines) {
        std::vector<std::string> cpuMetric;
        boost::split(cpuMetric, line, boost::is_any_of(" "), boost::token_compress_on);
        if (cpuMetric.size() > 0 && cpuMetric[0].substr(0, 3) == "cpu") {
            CPUStat cpuStat{};
            if (cpuMetric[0] == "cpu") {
                cpuStat.index = -1;
            } else {
                if (!StringTo(cpuMetric[0].substr(3), cpuStat.index)) {
                    LOG_ERROR(sLogger, ("failed to parse cpu index", "skip")("wrong cpu index", cpuMetric[0]));
                    continue;
                }
            }
            cpuStat.user = ParseMetric(cpuMetric, EnumCpuKey::user);
            cpuStat.nice = ParseMetric(cpuMetric, EnumCpuKey::nice);
            cpuStat.system = ParseMetric(cpuMetric, EnumCpuKey::system);
            cpuStat.idle = ParseMetric(cpuMetric, EnumCpuKey::idle);
            cpuStat.iowait = ParseMetric(cpuMetric, EnumCpuKey::iowait);
            cpuStat.irq = ParseMetric(cpuMetric, EnumCpuKey::irq);
            cpuStat.softirq = ParseMetric(cpuMetric, EnumCpuKey::softirq);
            cpuStat.steal = ParseMetric(cpuMetric, EnumCpuKey::steal);
            cpuStat.guest = ParseMetric(cpuMetric, EnumCpuKey::guest);
            cpuStat.guestNice = ParseMetric(cpuMetric, EnumCpuKey::guest_nice);
            cpuInfo.stats.push_back(cpuStat);
        }
    }
    cpuInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetProcessListInformationOnce(ProcessListInformation& processListInfo) {
    processListInfo.pids.clear();
    if (!std::filesystem::exists(PROCESS_DIR) || !std::filesystem::is_directory(PROCESS_DIR)) {
        LOG_ERROR(sLogger, ("process root path is not a directory or not exist", PROCESS_DIR));
        return false;
    }

    std::error_code ec;
    for (auto it = std::filesystem::directory_iterator(
             PROCESS_DIR, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::directory_iterator();
         ++it) {
        if (ec) {
            LOG_ERROR(sLogger, ("failed to iterate process directory", PROCESS_DIR)("error", ec.message()));
            return false;
        }
        const auto& dirEntry = *it;
        std::string dirName = dirEntry.path().filename().string();
        if (IsInt(dirName)) {
            pid_t pid{};
            if (!StringTo(dirName, pid)) {
                LOG_ERROR(sLogger, ("failed to parse pid", dirName));
            } else {
                processListInfo.pids.push_back(pid);
            }
        }
    }
    processListInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) {
    auto processStat = PROCESS_DIR / std::to_string(pid) / PROCESS_STAT;
    std::string line;
    if (FileReadResult::kOK != ReadFileContent(processStat.string(), line)) {
        LOG_ERROR(sLogger, ("read process stat", "fail")("file", processStat));
        return false;
    }
    mProcParser.ParseProcessStat(pid, line, processInfo.stat);
    processInfo.collectTime = steady_clock::now();
    return true;
}

bool LinuxSystemInterface::GetSystemLoadInformationOnce(SystemLoadInformation& systemLoadInfo) {
    std::vector<std::string> loadLines;
    std::string errorMessage;
    if (!GetHostLoadavg(loadLines, errorMessage) || loadLines.empty()) {
        LOG_WARNING(sLogger, ("failed to get system load", "invalid System collector")("error msg", errorMessage));
        return false;
    }

    // cat /proc/loadavg
    // 0.10 0.07 0.03 1/561 78450
    std::vector<std::string> loadMetric;
    boost::split(loadMetric, loadLines[0], boost::is_any_of(" "), boost::token_compress_on);

    if (loadMetric.size() < 3) {
        LOG_WARNING(sLogger, ("failed to split load metric", "invalid System collector"));
        return false;
    }

    CpuCoreNumInformation cpuCoreNumInfo;
    if (!SystemInterface::GetInstance()->GetCPUCoreNumInformation(cpuCoreNumInfo)) {
        LOG_WARNING(sLogger, ("failed to get cpu core num", "invalid System collector"));
        return false;
    }
    systemLoadInfo.systemStat.load1 = std::stod(loadMetric[0]);
    systemLoadInfo.systemStat.load5 = std::stod(loadMetric[1]);
    systemLoadInfo.systemStat.load15 = std::stod(loadMetric[2]);

    systemLoadInfo.systemStat.load1PerCore
        = systemLoadInfo.systemStat.load1 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);
    systemLoadInfo.systemStat.load5PerCore
        = systemLoadInfo.systemStat.load5 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);
    systemLoadInfo.systemStat.load15PerCore
        = systemLoadInfo.systemStat.load15 / static_cast<double>(cpuCoreNumInfo.cpuCoreNum);

    return true;
}
bool LinuxSystemInterface::GetCPUCoreNumInformationOnce(CpuCoreNumInformation& cpuCoreNumInfo) {
    cpuCoreNumInfo.cpuCoreNum = std::thread::hardware_concurrency();
    cpuCoreNumInfo.cpuCoreNum = cpuCoreNumInfo.cpuCoreNum < 1 ? 1 : cpuCoreNumInfo.cpuCoreNum;
    return true;
}

static inline double Diff(double a, double b) {
    return a - b > 0 ? a - b : 0;
}

uint64_t LinuxSystemInterface::GetMemoryValue(char unit, uint64_t value) {
    if (unit == 'k' || unit == 'K') {
        value *= 1024;
    } else if (unit == 'm' || unit == 'M') {
        value *= 1024 * 1024;
    }
    return value;
}

/*
样例: /proc/meminfo:
MemTotal:        4026104 kB
MemFree:         2246280 kB
MemAvailable:    3081592 kB
Buffers:          124380 kB
Cached:          1216756 kB
SwapCached:            0 kB
Active:           417452 kB
Inactive:        1131312 kB
 */
bool LinuxSystemInterface::GetHostMemInformationStatOnce(MemoryInformation& meminfo) {
    auto memInfoStat = PROCESS_DIR / PROCESS_MEMINFO;
    std::vector<std::string> memInfoStr;
    const uint64_t mb = 1024 * 1024;

    std::ifstream file(static_cast<std::string>(memInfoStat));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open meminfo file", "fail")("file", memInfoStat));
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        memInfoStr.push_back(line);
    }

    file.close();

    int count = 0;

    /* 字符串处理，处理成对应的类型以及值*/
    for (size_t i = 0; i < memInfoStr.size() && count < 5; i++) {
        std::vector<std::string> words;
        boost::algorithm::split(words, memInfoStr[i], boost::is_any_of(" "), boost::token_compress_on);
        // words-> MemTotal: / 12344 / kB
        if (words.size() < 2) {
            continue;
        }
        double val = 0.0;
        uint64_t orival;
        if (words.size() == 2) {
            if (!StringTo(words[1], val)) {
                val = 0.0;
            }
        } else if (words.back().size() > 0 && StringTo(words[1], orival)) {
            val = GetMemoryValue(words.back()[0], orival);
        }
        if (words[0] == "MemTotal:") {
            meminfo.memStat.total = val;
            count++;
        } else if (words[0] == "MemFree:") {
            meminfo.memStat.free = val;
            count++;
        } else if (words[0] == "MemAvailable:") {
            meminfo.memStat.available = val;
            count++;
        } else if (words[0] == "Buffers:") {
            meminfo.memStat.buffers = val;
            count++;
        } else if (words[0] == "Cached:") {
            meminfo.memStat.cached = val;
            count++;
        }
    }
    meminfo.memStat.used = Diff(meminfo.memStat.total, meminfo.memStat.free);
    meminfo.memStat.actualUsed = Diff(meminfo.memStat.total, meminfo.memStat.available);
    meminfo.memStat.actualFree = meminfo.memStat.available;
    meminfo.memStat.ram = meminfo.memStat.total / mb;

    double diff = Diff(meminfo.memStat.total, meminfo.memStat.actualFree);
    meminfo.memStat.usedPercent = meminfo.memStat.total > 0 ? diff * 100 / meminfo.memStat.total : 0.0;
    diff = Diff(meminfo.memStat.total, meminfo.memStat.actualUsed);
    meminfo.memStat.freePercent = meminfo.memStat.total > 0 ? diff * 100 / meminfo.memStat.total : 0.0;
    return true;
}

bool LinuxSystemInterface::GetTCPStatInformationOnce(TCPStatInformation& tcpStatInfo) {
    NetState netState;

    bool ret = false;

    ret = GetNetStateByNetLink(netState);

    if (ret) {
        tcpStatInfo.stat.tcpEstablished = (netState.tcpStates[TCP_ESTABLISHED]);
        tcpStatInfo.stat.tcpListen = (netState.tcpStates[TCP_LISTEN]);
        tcpStatInfo.stat.tcpTotal = (netState.tcpStates[TCP_TOTAL]);
        tcpStatInfo.stat.tcpNonEstablished = (netState.tcpStates[TCP_NON_ESTABLISHED]);
    }

    return ret;
}

bool LinuxSystemInterface::GetNetInterfaceInformationOnce(NetInterfaceInformation& netInterfaceInfo) {
    //  /proc/net/dev
    std::vector<std::string> netDevLines = {};
    std::string errorMessage;
    bool ret = GetHostNetDev(netDevLines, errorMessage);
    if (!ret || netDevLines.empty()) {
        return false;
    }

    // netInterfaceInfo.configs
    for (size_t i = 2; i < netDevLines.size(); ++i) {
        auto pos = netDevLines[i].find_first_of(':');
        std::string devCounterStr = netDevLines[i].substr(pos + 1);
        std::string devName = netDevLines[i].substr(0, pos);

        // netInterfaceInfo.configs
        boost::algorithm::trim(devName);
        InterfaceConfig ifConfig;
        ret = GetInterfaceConfig(ifConfig, devName);
        if (ret != true) {
            break;
        }
        netInterfaceInfo.configs.push_back(ifConfig);

        // netInterfaceInfo.metrics
        std::vector<std::string> netDevMetric;
        boost::algorithm::trim(devCounterStr);
        boost::split(netDevMetric, devCounterStr, boost::is_any_of(" "), boost::token_compress_on);

        if (netDevMetric.size() >= 16) {
            NetInterfaceMetric information;
            int index = 0;
            uint64_t value = 0;
            information.name = devName;
            information.rxBytes = StringTo(netDevMetric[index++], value) ? value : 0;
            information.rxPackets = StringTo(netDevMetric[index++], value) ? value : 0;
            information.rxErrors = StringTo(netDevMetric[index++], value) ? value : 0;
            information.rxDropped = StringTo(netDevMetric[index++], value) ? value : 0;
            information.rxOverruns = StringTo(netDevMetric[index++], value) ? value : 0;
            information.rxFrame = StringTo(netDevMetric[index++], value) ? value : 0;
            // skip compressed multicast
            index += 2;
            information.txBytes = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txPackets = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txErrors = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txDropped = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txOverruns = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txCollisions = StringTo(netDevMetric[index++], value) ? value : 0;
            information.txCarrier = StringTo(netDevMetric[index++], value) ? value : 0;

            information.speed = -1;
            netInterfaceInfo.metrics.push_back(information);
        }
    }

    return ret;
}

bool LinuxSystemInterface::GetProcessCmdlineStringOnce(pid_t pid, ProcessCmdlineString& cmdline) {
    auto processCMDline = PROCESS_DIR / std::to_string(pid) / PROCESS_CMDLINE;
    cmdline.cmdline.clear();

    std::ifstream file(static_cast<std::string>(processCMDline));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open process cmdline file", "fail")("file", processCMDline));
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        cmdline.cmdline.push_back(line);
    }

    file.close();

    return true;
}

bool LinuxSystemInterface::GetProcessStatmOnce(pid_t pid, ProcessMemoryInformation& processMemory) {
    auto processStatm = PROCESS_DIR / std::to_string(pid) / PROCESS_STATM;
    std::vector<std::string> processStatmString;
    char* endptr;

    std::ifstream file(static_cast<std::string>(processStatm));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open process statm file", "fail")("file", processStatm));
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        processStatmString.push_back(line);
    }
    file.close();

    std::vector<std::string> processMemoryMetric;
    if (!processStatmString.empty()) {
        const std::string& input = processStatmString.front();
        boost::algorithm::split(processMemoryMetric, input, boost::is_any_of(" "), boost::algorithm::token_compress_on);
    }

    if (processMemoryMetric.size() < 3) {
        return false;
    }

    long pagesize = sysconf(_SC_PAGESIZE); // 获取系统页大小
    int index = 0;
    processMemory.size = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10));
    processMemory.size = processMemory.size * pagesize;
    processMemory.resident = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10));
    processMemory.resident = processMemory.resident * pagesize;
    processMemory.share = static_cast<uint64_t>(std::strtoull(processMemoryMetric[index++].c_str(), &endptr, 10));
    processMemory.share = processMemory.share * pagesize;

    return true;
}

bool LinuxSystemInterface::GetProcessCredNameOnce(pid_t pid, ProcessCredName& processCredName) {
    auto processStatus = PROCESS_DIR / std::to_string(pid) / PROCESS_STATUS;
    std::vector<std::string> processStatusString;
    std::vector<std::string> metric;

    std::ifstream file(static_cast<std::string>(processStatus));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open process status file", "fail")("file", processStatus));
    }

    std::string line;
    while (std::getline(file, line)) {
        processStatusString.push_back(line);
    }
    file.close();

    ProcessCred cred{};

    for (size_t i = 0; i < processStatusString.size(); ++i) {
        boost::algorithm::split(
            metric, processStatusString[i], boost::algorithm::is_any_of("\t"), boost::algorithm::token_compress_on);
        if (metric.front() == "Name:") {
            processCredName.name = metric[1];
        }
        if (metric.size() >= 3 && metric.front() == "Uid:") {
            int index = 1;
            cred.uid = static_cast<uint64_t>(std::stoull(metric[index++]));
            cred.euid = static_cast<uint64_t>(std::stoull(metric[index]));
        } else if (metric.size() >= 3 && metric.front() == "Gid:") {
            int index = 1;
            cred.gid = static_cast<uint64_t>(std::stoull(metric[index++]));
            cred.egid = static_cast<uint64_t>(std::stoull(metric[index]));
        }
    }

    passwd* pw = nullptr;
    passwd pwbuffer;
    char buffer[2048];
    if (getpwuid_r(cred.uid, &pwbuffer, buffer, sizeof(buffer), &pw) != 0) {
        return EXECUTE_FAIL;
    }
    if (pw == nullptr) {
        return EXECUTE_FAIL;
    }
    processCredName.user = pw->pw_name;

    group* grp = nullptr;
    group grpbuffer{};
    char groupBuffer[2048];
    if (getgrgid_r(cred.gid, &grpbuffer, groupBuffer, sizeof(groupBuffer), &grp)) {
        return EXECUTE_FAIL;
    }

    if (grp != nullptr && grp->gr_name != nullptr) {
        processCredName.group = grp->gr_name;
    }

    return true;
}

bool LinuxSystemInterface::GetExecutablePathOnce(pid_t pid, ProcessExecutePath& executePath) {
    std::filesystem::path procExePath = PROCESS_DIR / std::to_string(pid) / PROCESS_EXE;
    char buffer[4096];
    ssize_t len = readlink(procExePath.c_str(), buffer, sizeof(buffer));
    if (len < 0) {
        executePath.path = "";
        return true;
    }
    executePath.path.assign(buffer, len);
    return true;
}

bool LinuxSystemInterface::GetProcessOpenFilesOnce(pid_t pid, ProcessFd& processFd) {
    std::filesystem::path procFdPath = PROCESS_DIR / std::to_string(pid) / PROCESS_FD;

    int count = 0;
    for (const auto& dirEntry :
         std::filesystem::directory_iterator{procFdPath, std::filesystem::directory_options::skip_permission_denied}) {
        std::string filename = dirEntry.path().filename().string();
        count++;
    }

    processFd.total = count;
    processFd.exact = true;

    return true;
}
} // namespace logtail
