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

#include "MachineInfoUtil.h"

#include <string.h>
#if defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include <algorithm>
#include <list>
#include <map>
#elif defined(_MSC_VER)
#include <WinSock2.h>
#include <Windows.h>
#endif
#include <curl/curl.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <thread>

#include "FileSystemUtil.h"
#include "StringTools.h"
#include "logger/Logger.h"


#if defined(_MSC_VER)
typedef LONG NTSTATUS, *PNTSTATUS;
#define STATUS_SUCCESS (0x00000000)
typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(POSVERSIONINFO);
bool GetRealOSVersion(POSVERSIONINFO osvi) {
    HMODULE hMod = ::GetModuleHandle("ntdll.dll");
    if (!hMod) {
        LOG_ERROR(sLogger, ("GetModuleHandle failed", GetLastError()));
        return false;
    }
    RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
    if (nullptr == fxPtr) {
        LOG_ERROR(sLogger, ("Get RtlGetVersion failed", GetLastError()));
        return false;
    }
    return STATUS_SUCCESS == fxPtr(osvi);
}
#endif

namespace logtail {

std::string GetOsDetail() {
#if defined(__linux__)
    std::string osDetail;
    utsname* buf = new utsname;
    if (-1 == uname(buf))
        LOG_ERROR(sLogger, ("uname failed, errno", errno));
    else {
        osDetail.append(buf->sysname);
        osDetail.append("; ");
        osDetail.append(buf->release);
        osDetail.append("; ");
        osDetail.append(buf->version);
        osDetail.append("; ");
        osDetail.append(buf->machine);
    }
    delete buf;
    return osDetail;
#elif defined(_MSC_VER)
    // Because Latest Windows implement GetVersionEx according to manifests rather than
    // actual runtime OS, we should get OSVERSIONINFOEX from RtlGetVersion.
    // And we need to call GetVersionEx to get extra fields such as wProductType.
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetRealOSVersion((POSVERSIONINFO)&osvi)) {
        OSVERSIONINFOEX extra;
        ZeroMemory(&extra, sizeof(OSVERSIONINFOEX));
        extra.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if (GetVersionEx((POSVERSIONINFO)&extra)) {
            osvi.wProductType = extra.wProductType;
        }
    } else if (!GetVersionEx((LPOSVERSIONINFO)&osvi)) {
        LOG_ERROR(sLogger, ("GetVersionEx failed", GetLastError()));
        return "";
    }
    // According to https://docs.microsoft.com/zh-cn/windows/desktop/api/winnt/ns-winnt-_osversioninfoexa.
    std::map<std::tuple<DWORD, DWORD, bool>, std::string> OS_AFTER_XP = {
        {{10, 0, true}, "Windows 10"},
        {{10, 0, false}, "Windows Server 2016"},
        {{6, 3, true}, "Windows 8.1"},
        {{6, 3, false}, "Windows Server 2012 R2"},
        {{6, 2, true}, "Windows 8"},
        {{6, 2, false}, "Windows Server 2012"},
        {{6, 1, true}, "Windows 7"},
        {{6, 1, false}, "Windows Server 2008 R2"},
        {{6, 0, true}, "Windows Vista"},
        {{6, 0, false}, "Windows Server 2008"},
    };
    auto key = std::tuple<DWORD, DWORD, bool>{
        osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.wProductType == VER_NT_WORKSTATION};
    std::string osDetail;
    if (OS_AFTER_XP.find(key) != OS_AFTER_XP.end())
        osDetail = OS_AFTER_XP[key];
    // More information is included for OS before XP.
    SYSTEM_INFO siSysInfo;
    GetSystemInfo(&siSysInfo);
    if (osDetail.empty()) {
        if (5 == osvi.dwMajorVersion && 2 == osvi.dwMinorVersion) {
            if (GetSystemMetrics(SM_SERVERR2) != 0)
                osDetail = "Windows Server 2003 R2";
            else if (osvi.wSuiteMask & VER_SUITE_WH_SERVER)
                osDetail = "Windows Home Server";
            else if (GetSystemMetrics(SM_SERVERR2) == 0)
                osDetail = "Windows Server 2003";
            else if ((osvi.wProductType == VER_NT_WORKSTATION)
                     && (siSysInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)) {
                osDetail = "Windows XP Professional"; // x64 Edition.
            }
        } else if (5 == osvi.dwMajorVersion && 1 == osvi.dwMinorVersion) {
            osDetail = "Windows XP";
        }
    }
    if (osDetail.empty()) {
        osDetail = "Windows " + std::to_string(osvi.dwMajorVersion) + "." + std::to_string(osvi.dwMinorVersion);
    }
    return osDetail;
#endif
}

std::string GetUsername() {
#if defined(__linux__)
    passwd* pw = getpwuid(getuid());
    if (pw)
        return std::string(pw->pw_name);
    else
        return "";
#elif defined(_MSC_VER)
    const int INFO_BUFFER_SIZE = 100;
    TCHAR infoBuf[INFO_BUFFER_SIZE];
    DWORD bufCharCount = INFO_BUFFER_SIZE;
    if (!GetUserName(infoBuf, &bufCharCount)) {
        LOG_ERROR(sLogger, ("GetUserName failed", GetLastError()));
        return "";
    }
    return std::string(infoBuf);
#endif
}

std::string GetHostName() {
    char hostname[1024];
    gethostname(hostname, 1024);
    return std::string(hostname);
}

std::unordered_set<std::string> GetNicIpv4IPSet() {
    struct ifaddrs* ifAddrStruct = NULL;
    void* tmpAddrPtr = NULL;
    std::unordered_set<std::string> ipSet;
    getifaddrs(&ifAddrStruct);
    for (struct ifaddrs* ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN] = "";
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string ip(addressBuffer);
            // The loopback on most Linux distributions is lo, however it is not portable. For example loopback in OSX
            // is lo0.
            if (0 == strcmp("lo", ifa->ifa_name) || ip.empty() || StartWith(ip, "127.")) {
                continue;
            }
            ipSet.insert(std::move(ip));
        }
    }
    freeifaddrs(ifAddrStruct);
    return ipSet;
}

std::string GetHostIpByHostName() {
    std::string hostname = GetHostName();

    // if hostname is invalid, other methods should be used to get correct ip.
    if (!IsDigitsDotsHostname(hostname.c_str())) {
        LOG_INFO(sLogger, ("invalid hostname", "will use other methods to obtain ip")("hostname", hostname));
        return "";
    }

    struct addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    int status = getaddrinfo(hostname.c_str(), NULL, &hints, &res);
    if (status != 0 || res == nullptr) {
        LOG_WARNING(sLogger,
                    ("failed get address info", "will use other methods to obtain ip")("errMsg", gai_strerror(status)));
        return "";
    }

    std::vector<sockaddr_in> addrs;
    for (auto p = res; p != nullptr; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            addrs.emplace_back(*(struct sockaddr_in*)p->ai_addr);
        }
    }
    freeaddrinfo(res);

    std::string firstIp;
    char ipStr[INET_ADDRSTRLEN + 1] = "";
#if defined(__linux__)
    auto ipSet = GetNicIpv4IPSet();
    for (size_t i = 0; i < addrs.size(); ++i) {
        auto p = inet_ntop(AF_INET, &addrs[i].sin_addr, ipStr, INET_ADDRSTRLEN);
        if (p == nullptr) {
            continue;
        }
        auto tmp = std::string(ipStr);
        if (ipSet.find(tmp) != ipSet.end()) {
            return tmp;
        }
        if (i == 0) {
            firstIp = tmp;
            if (ipSet.empty()) {
                LOG_INFO(sLogger, ("no entry from getifaddrs", "use first entry from getaddrinfo"));
                return firstIp;
            }
        }
    }
#elif defined(_MSC_VER)
    for (size_t i = 0; i < addrs.size(); ++i) {
        auto p = inet_ntop(AF_INET, &addrs[i].sin_addr, ipStr, INET_ADDRSTRLEN);
        if (p == nullptr) {
            continue;
        }
        auto tmp = std::string(ipStr);
        // According to RFC 1918 (http://www.faqs.org/rfcs/rfc1918.html), private IP ranges are as bellow:
        //   10.0.0.0        -   10.255.255.255  (10/8 prefix)
        //   172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
        //   192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
        //   100.*.*.* , 30.*.*.* - alibaba office network
        if (tmp.find("10.") == 0 || tmp.find("100.") == 0 || tmp.find("30.") == 0 || tmp.find("192.168.") == 0
            || tmp.find("172.16.") == 0 || tmp.find("172.17.") == 0 || tmp.find("172.18.") == 0
            || tmp.find("172.19.") == 0 || tmp.find("172.20.") == 0 || tmp.find("172.21.") == 0
            || tmp.find("172.22.") == 0 || tmp.find("172.23.") == 0 || tmp.find("172.24.") == 0
            || tmp.find("172.25.") == 0 || tmp.find("172.26.") == 0 || tmp.find("172.27.") == 0
            || tmp.find("172.28.") == 0 || tmp.find("172.29.") == 0 || tmp.find("172.30.") == 0
            || tmp.find("172.31.") == 0) {
            return tmp;
        }
        if (i == 0) {
            firstIp = tmp;
        }
    }
#endif
    LOG_INFO(sLogger, ("no entry from getaddrinfo matches entry from getifaddrs", "use first entry from getaddrinfo"));
    return firstIp;
}

std::string GetHostIpByInterface(const std::string& intf) {
#if defined(__linux__)
    int sock;
    struct sockaddr_in sin;
    struct ifreq ifr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return "";
    }
    // use eth0 as the default ETH name
    strncpy(ifr.ifr_name, intf.size() > 0 ? intf.c_str() : "eth0", IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;

    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return "";
    }

    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));

    char* ipaddr = inet_ntoa(sin.sin_addr);
    close(sock);
    if (ipaddr == NULL) {
        return "";
    }
    return std::string(ipaddr);
#elif defined(_MSC_VER)
    // TODO: For Windows, interface should be replace to adaptor name.
    // Delay implementation, assume that GetIpByHostName will succeed.
    return "";
#endif
}

std::string GetHostIp(const std::string& intf) {
    std::string ip = GetHostIpByHostName();
#if defined(__linux__)
    if (ip.empty() || ip.find("127.") == 0) {
        if (intf.empty()) {
            ip = GetHostIpByInterface("eth0");
            if (ip.empty() || ip.find("127.") == 0) {
                ip = GetHostIpByInterface("bond0");
            }
            return ip;
        }
        return GetHostIpByInterface(intf);
    }
#endif
    return ip;
}

std::string GetAnyAvailableIP() {
#if defined(__linux__)
    std::string retIP;
    char host[NI_MAXHOST];
    auto ipSet = GetNicIpv4IPSet();
    if (!ipSet.empty()) {
        for (auto& ip : ipSet) {
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_port = 0;
            int result = inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);
            if (result != 1) {
                continue;
            }
            int s = getnameinfo(
                (struct sockaddr*)&sa, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                continue;
            }
            retIP = ip;
            break;
        }
    }
    return retIP;

#elif defined(_MSC_VER)
    // TODO:
    return "192.168.1.1";
#endif
}

bool GetKernelInfo(std::string& kernelRelease, int64_t& kernelVersion) {
#if defined(__linux__)
    struct utsname buf;
    if (-1 == uname(&buf)) {
        LOG_ERROR(sLogger, ("uname failed, errno", errno));
        return false;
    }
    kernelRelease.assign(buf.release);
    std::vector<int64_t> versions;
    char* p = buf.release;
    char* maxP = buf.release + sizeof(buf.release);
    while (*p && p < maxP) {
        if (isdigit(*p)) {
            versions.push_back(strtol(p, &p, 10));
        } else {
            p++;
        }
    }
    kernelVersion = 0;
    for (size_t i = 0; i < versions.size() && i < (size_t)4; ++i) {
        kernelVersion = kernelVersion * 1000 + versions[i];
    }
    return true;
#else
    return false;
#endif
}

uint32_t GetHostIpValueByInterface(const std::string& intf) {
#if defined(__linux__)
    int sock;
    struct sockaddr_in sin;
    struct ifreq ifr;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) {
        return 0;
    }
    // use eth0 as the default ETH name
    strncpy(ifr.ifr_name, intf.size() > 0 ? intf.c_str() : "eth0", IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = 0;
    if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
        close(sock);
        return 0;
    }
    memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
    close(sock);
    return sin.sin_addr.s_addr;
#elif defined(_MSC_VER)
    // TODO: For Windows, interface should be replace to adaptor name.
    // Delay implementation, assume that GetIpByHostName will succeed.
    return 0;
#endif
}

void GetAllPids(std::unordered_set<int32_t>& pids) {
#if defined(__linux__)
    pids.clear();
    fsutil::Dir procDir("/proc/");
    if (!procDir.Open()) {
        return;
    }
    fsutil::Entry entry;
    while (true) {
        entry = procDir.ReadNext();
        if (!entry) {
            return;
        }
        if (!entry.IsDir()) {
            continue;
        }
        std::string name = entry.Name();
        int32_t pid = strtol(name.data(), NULL, 10);
        if (pid <= 0) {
            continue;
        }
        pids.insert(pid);
    }
#endif
}
bool GetRedHatReleaseInfo(std::string& os, int64_t& osVersion, std::string bashPath) {
    static const boost::regex sReg(R"(^(\S+)\s+\S+\s+\S+\s+(\d+)\.(\d+).*$)");
    bashPath.append("/etc/redhat-release");
    os.clear();
    std::string content, exception;
    if (!ReadFileContent(bashPath, content)) {
        return false;
    }
    boost::match_results<const char*> what;
    if (BoostRegexSearch(content.c_str(), sReg, exception, what) && what.size() > 1) {
        os = what[1].str();
        osVersion = strtol(what[2].begin(), nullptr, 10) * 1000;
        osVersion += strtol(what[3].begin(), nullptr, 10);
    }
    LOG_DEBUG(sLogger, ("read /etc/redhat-release content", content));
    return !os.empty() && osVersion != 0;
}

// For hostnames in the following format, gethostbyname will fake an IP (and thus return wrong IP):
// 1. a, where a is a number between 0 and 2^32-1;
// 2. a.b, where a & b are numbers, and a is between 0 and 2^8-1, b is between 0 and 2^24-1;
// 3. a.b.c, where a, b & c are numbers, and a & b are between 0 and 2^8-1, c is between 0 and 2^16-1;
// 4. a.b.c.d, where a, b, c & d are numbers between 0 and 2^8-1.
// All numbers mentioned here can be both in base 8 or 10.
//
// see https://codebrowser.dev/glibc/glibc/nss/digits_dots.c.html#__nss_hostname_digits_dots_context for more details.
bool IsDigitsDotsHostname(const char* hostname) {
    if (hostname && *hostname != '\0') {
        const char* cp = hostname;
        int16_t digits = 32;
        while (*cp != '\0' && digits > 0) {
            char* endp;
            uint64_t sum = strtoul(cp, &endp, 0);
            if ((sum == ULONG_MAX && errno == ERANGE) || sum >= (1UL << digits)) {
                break;
            }
            cp = endp;
            if (*cp == '.' && sum <= 255) {
                digits -= 8;
                cp++;
            } else {
                break;
            }
        }
        if (*cp == '\0' && *(cp - 1) != '.') {
            return false;
        }
        return true;
    }
    return false;
}


size_t FetchECSMetaCallback(char* buffer, size_t size, size_t nmemb, std::string* res) {
    if (NULL == buffer) {
        return 0;
    }

    size_t sizes = size * nmemb;
    res->append(buffer, sizes);
    return sizes;
}

ECSMeta FetchECSMeta() {
    CURL* curl;
    for (size_t retryTimes = 1; retryTimes <= 5; retryTimes++) {
        curl = curl_easy_init();
        if (curl) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    ECSMeta metaObj;
    metaObj.instanceID = "";
    metaObj.userID = "";
    metaObj.regionID = "";
    if (curl) {
        std::string meta;
        curl_easy_setopt(curl, CURLOPT_URL, "http://100.100.100.200/latest/dynamic/instance-identity/document");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &meta);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, FetchECSMetaCallback);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG_DEBUG(sLogger, ("fetch ecs meta fail", curl_easy_strerror(res)));
        } else {
            rapidjson::Document doc;
            doc.Parse(meta.c_str());
            if (doc.HasParseError() || !doc.IsObject()) {
                LOG_DEBUG(sLogger, ("fetch ecs meta fail", meta));
            } else {
                rapidjson::Value::ConstMemberIterator instanceItr = doc.FindMember("instance-id");
                if (instanceItr != doc.MemberEnd() && (instanceItr->value.IsString())) {
                    metaObj.instanceID = instanceItr->value.GetString();
                }

                rapidjson::Value::ConstMemberIterator userItr = doc.FindMember("owner-account-id");
                if (userItr != doc.MemberEnd() && userItr->value.IsString()) {
                    metaObj.userID = userItr->value.GetString();
                }

                rapidjson::Value::ConstMemberIterator regionItr = doc.FindMember("region-id");
                if (regionItr != doc.MemberEnd() && regionItr->value.IsString()) {
                    metaObj.regionID = regionItr->value.GetString();
                }
            }
        }
        curl_easy_cleanup(curl);
        return metaObj;
    }
    LOG_WARNING(
        sLogger,
        ("curl handler cannot be initialized during user environment identification", "ecs meta may be mislabeled"));
    return metaObj;
}

} // namespace logtail
