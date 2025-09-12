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
#include <mntent.h>
#include <pwd.h>
#include <sys/statvfs.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>
#include <filesystem>
#include <iostream>

#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/common/FastFieldParser.h"
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
            if (FastParse::FieldStartsWith(line, 0, "TCP:") || FastParse::FieldStartsWith(line, 0, "TCP6:")) {
                auto twValue = FastParse::GetFieldAs<uint64_t>(line, 6, 0);
                auto allocValue = FastParse::GetFieldAs<uint64_t>(line, 8, 0);

                tcp += twValue + allocValue;
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
        FastFieldParser parser(devLine);

        size_t fieldCount = parser.GetFieldCount();
        if (fieldCount == 0)
            continue;

        auto inet6NameField = parser.GetField(fieldCount - 1);
        std::string inet6Name(inet6NameField);
        boost::algorithm::trim(inet6Name);

        if (inet6Name == name) {
            // Doc: https://ata.atatech.org/articles/11020228072?spm=ata.25287382.0.0.1c647536bhA7NG#lyRD52DR
            if (Inet6Address < fieldCount) {
                auto* addr6 = (unsigned char*)&(interfaceConfig.address6.addr.in6);

                auto addrField = parser.GetField(Inet6Address);
                std::string addr(addrField);

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
            if (Inet6PrefixLen < fieldCount) {
                auto prefixField = parser.GetField(Inet6PrefixLen);
                interfaceConfig.prefix6Length = Hex2Int(std::string(prefixField));
            }
            if (Inet6Scope < fieldCount) {
                auto scopeField = parser.GetField(Inet6Scope);
                interfaceConfig.scope6 = Hex2Int(std::string(scopeField));
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
    return true;
}

bool LinuxSystemInterface::GetCPUInformationOnce(CPUInformation& cpuInfo) {
    std::vector<std::string> cpuLines;
    std::string errorMessage;
    if (!GetHostSystemStat(cpuLines, errorMessage)) {
        LOG_ERROR(sLogger, ("failed to get CPU information", errorMessage));
        return false;
    }
    // cpu  1195061569 1728645 418424132 203670447952 14723544 0 773400 0 0 0
    // cpu0 14708487 14216 4613031 2108180843 57199 0 424744 0 0 0
    // ...
    cpuInfo.stats.clear();
    cpuInfo.stats.reserve(cpuLines.size());
    for (auto const& line : cpuLines) {
        CpuStatParser parser(line);

        int cpuIndex = parser.GetCpuIndex();
        if (cpuIndex < -1) {
            continue;
        }

        CPUStat cpuStat{};
        cpuStat.index = cpuIndex;

        parser.GetCpuStats(cpuStat.user,
                           cpuStat.nice,
                           cpuStat.system,
                           cpuStat.idle,
                           cpuStat.iowait,
                           cpuStat.irq,
                           cpuStat.softirq,
                           cpuStat.steal,
                           cpuStat.guest,
                           cpuStat.guestNice);

        cpuInfo.stats.push_back(cpuStat);
    }
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
    return true;
}

bool LinuxSystemInterface::GetProcessInformationOnce(pid_t pid, ProcessInformation& processInfo) {
    auto processStat = PROCESS_DIR / std::to_string(pid) / PROCESS_STAT;
    std::string line;
    if (FileReadResult::kOK != ReadFileContent(processStat.string(), line, kDefaultMaxFileSize)) {
        LOG_ERROR(sLogger, ("read process stat", "fail")("file", processStat));
        return false;
    }
    mProcParser.ParseProcessStat(pid, line, processInfo.stat);
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
    const auto& loadLine = loadLines[0];
    auto load1 = FastParse::GetFieldAs<double>(loadLine, 0, 0.0);
    auto load5 = FastParse::GetFieldAs<double>(loadLine, 1, 0.0);
    auto load15 = FastParse::GetFieldAs<double>(loadLine, 2, 0.0);

    if (load1 == 0.0 && load5 == 0.0 && load15 == 0.0) {
        LOG_WARNING(sLogger, ("failed to parse load metric", "invalid System collector"));
        return false;
    }

    CpuCoreNumInformation cpuCoreNumInfo;
    if (!SystemInterface::GetInstance()->GetCPUCoreNumInformation(cpuCoreNumInfo)) {
        LOG_WARNING(sLogger, ("failed to get cpu core num", "invalid System collector"));
        return false;
    }
    systemLoadInfo.systemStat.load1 = load1;
    systemLoadInfo.systemStat.load5 = load5;
    systemLoadInfo.systemStat.load15 = load15;

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
        FastFieldParser parser(memInfoStr[i]);

        if (parser.GetFieldCount() < 2) {
            continue;
        }

        auto field0 = parser.GetField(0); // 字段名 (MemTotal:)
        auto field1 = parser.GetField(1); // 数值 (12344)

        double val = 0.0;
        uint64_t orival;

        if (parser.GetFieldCount() == 2) {
            if (!StringTo(std::string(field1), val)) {
                val = 0.0;
            }
        } else {
            auto lastField = parser.GetField(parser.GetFieldCount() - 1); // 单位 (kB)
            if (!lastField.empty() && StringTo(std::string(field1), orival)) {
                val = GetMemoryValue(lastField[0], orival);
            }
        }

        if (field0 == "MemTotal:") {
            meminfo.memStat.total = val;
            count++;
        } else if (field0 == "MemFree:") {
            meminfo.memStat.free = val;
            count++;
        } else if (field0 == "MemAvailable:") {
            meminfo.memStat.available = val;
            count++;
        } else if (field0 == "Buffers:") {
            meminfo.memStat.buffers = val;
            count++;
        } else if (field0 == "Cached:") {
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

// 已知文件系统
static const auto& knownFileSystem = *new std::unordered_map<std::string, FileSystemType>{
    {"adfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"affs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"anon-inode FS", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"befs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"bfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"btrfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"ecryptfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"efs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"futexfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"gpfs", FILE_SYSTEM_TYPE_NETWORK},
    {"hpfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"hfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"isofs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"k-afs", FILE_SYSTEM_TYPE_NETWORK},
    {"lustre", FILE_SYSTEM_TYPE_NETWORK},
    {"nilfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"openprom", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"reiserfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vzfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"xfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"xiafs", FILE_SYSTEM_TYPE_LOCAL_DISK},

    // CommonFileSystem
    {"ntfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"smbfs", FILE_SYSTEM_TYPE_NETWORK},
    {"smb", FILE_SYSTEM_TYPE_NETWORK},
    {"swap", FILE_SYSTEM_TYPE_SWAP},
    {"afs", FILE_SYSTEM_TYPE_NETWORK},
    {"iso9660", FILE_SYSTEM_TYPE_CDROM},
    {"cvfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"cifs", FILE_SYSTEM_TYPE_NETWORK},
    {"msdos", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"minix", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vxfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vfat", FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"zfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
};
const struct {
    const char* prefix;
    const FileSystemType fsType;
} knownFileSystemPrefix[] = {{"ext", FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"gfs", FILE_SYSTEM_TYPE_NETWORK},
                             {"jffs", FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"jfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"minix", FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"ocfs", FILE_SYSTEM_TYPE_NETWORK},
                             {"psfs", FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"nfs", FILE_SYSTEM_TYPE_NETWORK},
                             {"fat", FILE_SYSTEM_TYPE_LOCAL_DISK}};

const struct {
    FileSystemType fs;
    const char* name;
} fsTypeNames[] = {
    {FILE_SYSTEM_TYPE_UNKNOWN, "unknown"},
    {FILE_SYSTEM_TYPE_NONE, "none"},
    {FILE_SYSTEM_TYPE_LOCAL_DISK, "local"},
    {FILE_SYSTEM_TYPE_NETWORK, "remote"},
    {FILE_SYSTEM_TYPE_RAM_DISK, "ram"},
    {FILE_SYSTEM_TYPE_CDROM, "cdrom"},
    {FILE_SYSTEM_TYPE_SWAP, "swap"},
};
constexpr size_t fsTypeNamesCount = sizeof(fsTypeNames) / sizeof(fsTypeNames[0]);
static_assert(FILE_SYSTEM_TYPE_MAX == fsTypeNamesCount, "fsTypeNames size not matched");
std::string GetName(FileSystemType fs) {
    int idx = static_cast<int>(fs);
    if (0 <= idx && (size_t)idx < fsTypeNamesCount && fsTypeNames[idx].fs == fs) {
        return fsTypeNames[idx].name;
    }
    return "";
}
bool GetFileSystemType(const std::string& fsTypeName, FileSystemType& fsType, std::string& fsTypeDisplayName) {
    bool found = fsType != FILE_SYSTEM_TYPE_UNKNOWN;
    if (!found) {
        auto it = knownFileSystem.find(fsTypeName);
        found = it != knownFileSystem.end();
        if (found) {
            fsType = it->second;
        } else {
            for (auto& entry : knownFileSystemPrefix) {
                found = StartWith(fsTypeName, entry.prefix);
                if (found) {
                    fsType = entry.fsType;
                    break;
                }
            }
        }
    }

    if (!found || fsType >= FILE_SYSTEM_TYPE_MAX) {
        fsType = FILE_SYSTEM_TYPE_NONE;
    }
    fsTypeDisplayName = GetName(fsType);

    return found;
}
bool LinuxSystemInterface::GetFileSystemListInformationOnce(FileSystemListInformation& fileSystemListInfo) {
    FILE* fp;

    // MOUNTED: /etc/mtab, defined in /usr/include/paths.h
    auto mountedDir = ETC_DIR / ETC_MTAB;
    if (!(fp = setmntent(mountedDir.c_str(), "r"))) {
        return false;
    }
    deferred(endmntent(fp));

    mntent ent{};
    std::vector<char> buffer((size_t)4096);
    while (getmntent_r(fp, &ent, &buffer[0], buffer.size())) {
        FileSystem fileSystem;
        fileSystem.type = FILE_SYSTEM_TYPE_UNKNOWN;
        fileSystem.dirName = ent.mnt_dir;
        fileSystem.devName = ent.mnt_fsname;
        fileSystem.sysTypeName = ent.mnt_type;
        fileSystem.options = ent.mnt_opts;

        GetFileSystemType(fileSystem.sysTypeName, fileSystem.type, fileSystem.typeName);
        fileSystemListInfo.fileSystemList.push_back(fileSystem);
    }

    return true;
}

/*
> cat /proc/uptime
183857.30 1969716.84
第一列: 系统启动到现在的时间（以秒为单位）；
第二列: 系统空闲的时间（以秒为单位）。
*/
bool LinuxSystemInterface::GetSystemUptimeInformationOnce(SystemUptimeInformation& systemUptimeInfo) {
    std::vector<std::string> uptimeLines;
    std::string errorMessage;

    if (!CheckExistance(PROCESS_DIR / PROCESS_UPTIME)) {
        return false;
    }

    int ret = GetFileLines(PROCESS_DIR / PROCESS_UPTIME, uptimeLines, true, &errorMessage);
    if (ret != 0 || uptimeLines.empty()) {
        return false;
    }

    const auto& uptimeLine = uptimeLines.empty() ? "" : uptimeLines.front();
    systemUptimeInfo.uptime = FastParse::GetFieldAs<double>(uptimeLine, 0, 0.0);

    return true;
}

bool LinuxSystemInterface::GetDiskSerialIdInformationOnce(std::string diskName, SerialIdInformation& serialIdInfo) {
    std::vector<std::string> serialIdLines = {};
    std::string errorMessage;
    auto sysSerialId = SYSTEM_BLOCK_DIR / diskName / SERIAL;

    if (!CheckExistance(SYSTEM_BLOCK_DIR / diskName / SERIAL)) {
        LOG_ERROR(sLogger, ("file does not exist", (SYSTEM_BLOCK_DIR / diskName / SERIAL).string()));
        return false;
    }

    int ret = GetFileLines(sysSerialId, serialIdLines, true, &errorMessage);
    if (ret != 0 || serialIdLines.empty()) {
        return false;
    }

    serialIdInfo.serialId = serialIdLines[0];
    return true;
}

template <typename T>
T DiffOrZero(const T& a, const T& b) {
    return a > b ? a - b : T{0};
}
bool LinuxSystemInterface::GetDiskStateInformationOnce(DiskStateInformation& diskStateInfo) {
    std::vector<std::string> diskLines = {};
    std::string errorMessage;

    if (!CheckExistance(PROCESS_DIR / PROCESS_DISKSTATS)) {
        LOG_ERROR(sLogger, ("file does not exist", (PROCESS_DIR / PROCESS_DISKSTATS).string()));
        return false;
    }
    int ret = GetFileLines(PROCESS_DIR / PROCESS_DISKSTATS, diskLines, true, &errorMessage);
    if (ret != 0 || diskLines.empty()) {
        return false;
    } else {
        for (auto const& diskLine : diskLines) {
            DiskState diskStat;

            // 去除前导空格后解析
            std::string trimmedLine = boost::algorithm::trim_left_copy(diskLine);
            FastFieldParser parser(trimmedLine);

            size_t fieldCount = parser.GetFieldCount();
            if (fieldCount < (size_t)EnumDiskState::count) {
                continue;
            }
            try {
                std::vector<uint64_t> diskValues;
                diskValues.reserve(static_cast<size_t>(EnumDiskState::count));

                auto iter = parser.begin();
                auto end = parser.end();
                for (size_t i = 0; i < static_cast<size_t>(EnumDiskState::count) && iter != end; ++i, ++iter) {
                    uint64_t value;
                    diskValues.push_back(StringTo(*iter, value) ? value : 0);
                }

                if (diskValues.size() < static_cast<size_t>(EnumDiskState::count)) {
                    continue; // 字段数量不足，跳过此行
                }

                // 直接从数组索引访问，零遍历开销
                diskStat.major = static_cast<unsigned int>(diskValues[static_cast<size_t>(EnumDiskState::major)]);
                diskStat.minor = static_cast<unsigned int>(diskValues[static_cast<size_t>(EnumDiskState::minor)]);

                // 4  reads completed successfully
                diskStat.reads = diskValues[static_cast<size_t>(EnumDiskState::reads)];
                // 6  sectors read
                diskStat.readBytes = diskValues[static_cast<size_t>(EnumDiskState::readSectors)] * 512;
                // 7  time spent reading (ms)
                diskStat.rTime = diskValues[static_cast<size_t>(EnumDiskState::rMillis)];
                // 8  writes completed
                diskStat.writes = diskValues[static_cast<size_t>(EnumDiskState::writes)];
                // 10  sectors written
                diskStat.writeBytes = diskValues[static_cast<size_t>(EnumDiskState::writeSectors)] * 512;
                // 11  time spent writing (ms)
                diskStat.wTime = diskValues[static_cast<size_t>(EnumDiskState::wMillis)];
                // 13  time spent doing I/Os (ms)
                diskStat.time = diskValues[static_cast<size_t>(EnumDiskState::rwMillis)];
                // 14  weighted time spent doing I/Os (ms)
                diskStat.qTime = diskValues[static_cast<size_t>(EnumDiskState::qMillis)];

            } catch (...) {
                LOG_ERROR(sLogger, ("failed to parse number in diskstats", diskLine));
                return false;
            }
            diskStateInfo.diskStats.push_back(diskStat);
        }
    }

    return true;
}

bool LinuxSystemInterface::GetFileSystemInformationOnce(std::string dirName, FileSystemInformation& fileSystemInfo) {
    struct statvfs buffer {};
    int status = statvfs(dirName.c_str(), &buffer);
    if (status != 0) {
        LOG_ERROR(sLogger, ("get filesystem infomation error", dirName)("error status", status));
        return false;
    }

    // 单位是: KB
    uint64_t bsize = buffer.f_frsize / 512;
    fileSystemInfo.fileSystemState.total = ((buffer.f_blocks * bsize) >> 1);
    fileSystemInfo.fileSystemState.free = ((buffer.f_bfree * bsize) >> 1);
    fileSystemInfo.fileSystemState.avail = ((buffer.f_bavail * bsize) >> 1); // 非超级用户最大可使用的磁盘量
    fileSystemInfo.fileSystemState.used
        = DiffOrZero(fileSystemInfo.fileSystemState.total, fileSystemInfo.fileSystemState.free);
    fileSystemInfo.fileSystemState.files = buffer.f_files;
    fileSystemInfo.fileSystemState.freeFiles = buffer.f_ffree;

    // 此处为用户可使用的磁盘量，可能会与fileSystemInfo.total有差异。也就是说:
    // 当total < fileSystemInfo.total时，表明即使磁盘仍有空间，用户也申请不到了
    // 毕竟OS维护磁盘，会占掉一部分，比如文件分配表，目录文件等。
    uint64_t total = fileSystemInfo.fileSystemState.used + fileSystemInfo.fileSystemState.avail;
    uint64_t used = fileSystemInfo.fileSystemState.used;
    double percent = 0;
    if (total != 0) {
        // 磁盘占用率，使用的是用户最大可用磁盘总量来的，而非物理磁盘总量
        percent = (double)used / (double)total;
    }
    fileSystemInfo.fileSystemState.use_percent = percent;

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
        const auto& line = netDevLines[i];

        NetDevParser parser(line);
        StringView deviceNameView;
        std::vector<uint64_t> stats;

        if (!parser.ParseDeviceStats(deviceNameView, stats)) {
            continue;
        }

        std::string devName(deviceNameView);

        // netInterfaceInfo.configs
        InterfaceConfig ifConfig;
        ret = GetInterfaceConfig(ifConfig, devName);
        if (ret != true) {
            break;
        }
        netInterfaceInfo.configs.push_back(ifConfig);

        // netInterfaceInfo.metrics - 直接从解析的数组访问，避免字符串转换
        if (stats.size() >= 16) {
            NetInterfaceMetric information;
            information.name = devName;
            information.rxBytes = stats[0];
            information.rxPackets = stats[1];
            information.rxErrors = stats[2];
            information.rxDropped = stats[3];
            information.rxOverruns = stats[4];
            information.rxFrame = stats[5];
            // skip compressed(6) multicast(7)
            information.txBytes = stats[8];
            information.txPackets = stats[9];
            information.txErrors = stats[10];
            information.txDropped = stats[11];
            information.txOverruns = stats[12];
            information.txCollisions = stats[13];
            information.txCarrier = stats[14];
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

    return true;
}

bool LinuxSystemInterface::GetProcessStatmOnce(pid_t pid, ProcessMemoryInformation& processMemory) {
    auto processStatm = PROCESS_DIR / std::to_string(pid) / PROCESS_STATM;
    std::vector<std::string> processStatmString;

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

    if (!processStatmString.empty()) {
        const std::string& input = processStatmString.front();

        FastFieldParser parser(input);
        std::vector<uint64_t> memValues;
        memValues.reserve(3);

        auto iter = parser.begin();
        auto end = parser.end();
        for (size_t i = 0; i < 3 && iter != end; ++i, ++iter) {
            uint64_t value;
            memValues.push_back(StringTo(*iter, value) ? value : 0);
        }

        if (memValues.size() >= 3) {
            processMemory.size = memValues[0] * PAGE_SIZE;
            processMemory.resident = memValues[1] * PAGE_SIZE;
            processMemory.share = memValues[2] * PAGE_SIZE;
        } else {
            return false;
        }
    } else {
        return false;
    }

    return true;
}

bool LinuxSystemInterface::GetProcessCredNameOnce(pid_t pid, ProcessCredName& processCredName) {
    auto processStatus = PROCESS_DIR / std::to_string(pid) / PROCESS_STATUS;

    std::ifstream file(static_cast<std::string>(processStatus));

    if (!file.is_open()) {
        LOG_ERROR(sLogger, ("open process status file", "fail")("file", processStatus));
        return false;
    }

    std::string line;
    ProcessCred cred{};
    bool getUID = false;
    bool getGID = false;
    bool getName = false;
    while (std::getline(file, line) && !(getUID && getGID && getName)) {
        FastFieldParser parser(line, '\t');

        auto firstField = parser.GetField(0);
        if (firstField.empty())
            continue;

        if (firstField == "Name:") {
            auto nameField = parser.GetField(1);
            if (!nameField.empty()) {
                processCredName.name = std::string(nameField);
                getName = true;
            }
        } else if (firstField == "Uid:") {
            // 直接解析数值字段，避免中间字符串转换
            auto uid = parser.GetFieldAs<uint64_t>(1, 0);
            auto euid = parser.GetFieldAs<uint64_t>(2, 0);
            if (uid > 0) { // 基本有效性检查
                cred.uid = uid;
                cred.euid = euid;
                getUID = true;
            }
        } else if (firstField == "Gid:") {
            // 直接解析数值字段，避免中间字符串转换
            auto gid = parser.GetFieldAs<uint64_t>(1, 0);
            auto egid = parser.GetFieldAs<uint64_t>(2, 0);
            if (gid > 0) { // 基本有效性检查
                cred.gid = gid;
                cred.egid = egid;
                getGID = true;
            }
        }
    }

    passwd* pw = nullptr;
    passwd pwbuffer;
    char buffer[2048];
    if (getpwuid_r(cred.uid, &pwbuffer, buffer, sizeof(buffer), &pw) != 0 || pw == nullptr || pw->pw_name == nullptr) {
        return true;
    }

    processCredName.user = pw->pw_name;

    group* grp = nullptr;
    group grpbuffer{};
    char groupBuffer[2048];
    if (getgrgid_r(cred.gid, &grpbuffer, groupBuffer, sizeof(groupBuffer), &grp) != 0) {
        return true;
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

    // 检查目录是否存在，进程可能已经被杀死

    if (!CheckExistance(procFdPath)) {
        LOG_ERROR(sLogger, ("file does not exist", procFdPath.string()));
        return false;
    }

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
