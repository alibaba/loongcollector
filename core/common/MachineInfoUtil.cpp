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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <netdb.h>
#elif defined(_MSC_VER)
#include <WinSock2.h>
#include <Windows.h>
#endif
#include "logger/Logger.h"
#include "StringTools.h"


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

std::string GetHostIpByHostName() {
    struct hostent* entry = gethostbyname(GetHostName().c_str());
    if (entry == NULL) {
        return "";
    }
#if defined(__linux__)
    struct in_addr* addr = (struct in_addr*)entry->h_addr_list[0];
    if (addr == NULL) {
        return "";
    }
    char* ipaddr = inet_ntoa(*addr);
    if (ipaddr == NULL) {
        return "";
    }
    return std::string(ipaddr);
#elif defined(_MSC_VER)
    std::string ip;
    while (*(entry->h_addr_list) != NULL) {
        if (AF_INET == entry->h_addrtype) {
            // According to RFC 1918 (http://www.faqs.org/rfcs/rfc1918.html), private IP ranges are as bellow:
            //   10.0.0.0        -   10.255.255.255  (10/8 prefix)
            //   172.16.0.0      -   172.31.255.255  (172.16/12 prefix)
            //   192.168.0.0     -   192.168.255.255 (192.168/16 prefix)
            //   100.*.*.* , 30.*.*.* - alibaba office network
            std::string curIP = inet_ntoa(*(struct in_addr*)*entry->h_addr_list);
            ip = curIP;
            if (curIP.find("10.") == 0 || curIP.find("100.") == 0 || curIP.find("30.") == 0
                || curIP.find("192.168.") == 0 || curIP.find("172.16.") == 0 || curIP.find("172.17.") == 0
                || curIP.find("172.18.") == 0 || curIP.find("172.19.") == 0 || curIP.find("172.20.") == 0
                || curIP.find("172.21.") == 0 || curIP.find("172.22.") == 0 || curIP.find("172.23.") == 0
                || curIP.find("172.24.") == 0 || curIP.find("172.25.") == 0 || curIP.find("172.26.") == 0
                || curIP.find("172.27.") == 0 || curIP.find("172.28.") == 0 || curIP.find("172.29.") == 0
                || curIP.find("172.30.") == 0 || curIP.find("172.31.") == 0) {
                return curIP;
            }
        }
        entry->h_addr_list++;
    }
    return ip;
#endif
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
    struct ifaddrs* ifaddr = NULL;
    if (getifaddrs(&ifaddr) == -1) {
        APSARA_LOG_ERROR(sLogger, ("get any available IP error", errno));
        return "";
    }

    std::string retIP;
    char host[NI_MAXHOST];
    for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        const int family = ifa->ifa_addr->sa_family;
        const char* familyName = (family == AF_PACKET)
            ? "AF_PACKET"
            : (family == AF_INET) ? "AF_INET" : (family == AF_INET6) ? "AF_INET6" : "???";
        APSARA_LOG_DEBUG(sLogger, ("interface name", ifa->ifa_name)("family", family)("family name", familyName));
        if (family == AF_INET) {
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                APSARA_LOG_WARNING(sLogger, ("getnameinfo error", gai_strerror(s))("interface name", ifa->ifa_name));
                continue;
            }

            std::string ip(host);
            APSARA_LOG_DEBUG(sLogger, ("interface name", ifa->ifa_name)("IP", ip));
            if (!ip.empty() && !StartWith(ip, "127.")) {
                retIP = ip;
                break;
            }
            if (retIP.empty()) {
                retIP = ip;
            }
        }
    }

    freeifaddrs(ifaddr);
    return retIP;

#elif defined(_MSC_VER)
    // TODO:
    return "192.168.1.1";
#endif
}

} // namespace logtail
