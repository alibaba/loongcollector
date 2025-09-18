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

#include "host_monitor/collector/DiskCollector.h"

#include <mntent.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>

#include <string>

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "BaseCollector.h"
#include "MetricValue.h"
#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"

DEFINE_FLAG_INT32(basic_host_monitor_disk_collect_interval, "basic host monitor disk collect interval, seconds", 1);
namespace logtail {

const std::string DiskCollector::sName = "disk";

template <typename T>
T DiffOrZero(const T& a, const T& b) {
    return a > b ? a - b : T{0};
}

bool IsZero(const std::chrono::steady_clock::time_point& t) {
    return t.time_since_epoch().count() == 0;
}

bool IsZero(const std::chrono::system_clock::time_point& t) {
    return t.time_since_epoch().count() == 0;
}

bool DiskCollector::Init(HostMonitorContext& collectContext) {
    if (!BaseCollector::Init(collectContext)) {
        return false;
    }
    mLastTime = std::chrono::steady_clock::time_point{};
    mDeviceMountMapExpireTime = std::chrono::steady_clock::time_point{};
    return true;
}

// 最少一条
template <typename T>
std::string JoinBytesLimit(const T& v, const std::string& splitter, size_t n) {
    static_assert(std::is_base_of<std::vector<std::string>, T>::value
                      || std::is_base_of<std::set<std::string>, T>::value
                      || std::is_base_of<std::list<std::string>, T>::value,
                  "type must be std::vector<std::string> or std::list<std::string> or std::set<std::string>");
    std::string result;
    auto begin = v.begin();
    auto end = v.end();
    if (begin != end) {
        result = *begin++;
        n = (n == 0 ? std::numeric_limits<size_t>::max() : n);
        for (auto it = begin; it != end && result.size() + splitter.size() + it->size() <= n; ++it) {
            result.append(splitter);
            result.append(*it);
        }
    }
    return result;
}

bool DiskCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) {
    std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
    std::map<std::string, DiskCollectStat> diskCollectStatMap;
    if (GetDiskCollectStatMap(collectContext.mCollectTime, diskCollectStatMap) <= 0) {
        LOG_WARNING(sLogger, ("collect disk error", "skip"));
        return false;
    }

    mCurrentDiskCollectStatMap = diskCollectStatMap;
    if (IsZero(mLastTime)) {
        LOG_WARNING(sLogger, ("collect disk first time", "skip"));
        mLastDiskCollectStatMap = mCurrentDiskCollectStatMap;
        mLastTime = currentTime;
        return true;
    }
    if (mLastTime + std::chrono::milliseconds(1) >= currentTime) {
        // 调度间隔不能低于1ms
        LOG_WARNING(sLogger, ("collect disk too frequency", "skip"));
        return false;
    }

    auto interval = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - mLastTime);
    mLastTime = currentTime;

    for (auto& it : mCurrentDiskCollectStatMap) {
        const std::string& devName = it.first;
        DeviceMetric deviceMetric{};
        if (mLastDiskCollectStatMap.find(devName) != mLastDiskCollectStatMap.end()) {
            const DiskCollectStat& currentStat = mCurrentDiskCollectStatMap[devName];
            const DiskCollectStat& lastStat = mLastDiskCollectStatMap[devName];
            DiskMetric diskMetric;

            CalcDiskMetric(currentStat.diskStat, lastStat.diskStat, interval.count(), diskMetric);

            deviceMetric.total = it.second.space.total;
            deviceMetric.free = it.second.space.free;
            deviceMetric.used = it.second.space.used;
            deviceMetric.usePercent = it.second.space.usePercent;
            deviceMetric.avail = it.second.spaceAvail;
            deviceMetric.reads = diskMetric.reads;
            deviceMetric.writes = diskMetric.writes;
            deviceMetric.readBytes = diskMetric.readBytes;
            deviceMetric.writeBytes = diskMetric.writeBytes;
            deviceMetric.avgqu_sz = diskMetric.avgqu_sz;
            deviceMetric.inodePercent = it.second.inode.usePercent;
            // mDeviceCalMap没有这个dev的数据
            if (mDeviceCalMap.find(devName) == mDeviceCalMap.end()) {
                mDeviceCalMap[devName] = MetricCalculate<DeviceMetric>();
            }
            mDeviceCalMap[devName].AddValue(deviceMetric);
        }
    }
    mLastDiskCollectStatMap = mCurrentDiskCollectStatMap;

    // If group is not provided, just collect data without generating metrics
    if (!groupPtr) {
        return true;
    }

    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;

    for (auto& mDeviceCal : mDeviceCalMap) {
        std::string devName = mDeviceCal.first;
        std::string diskName = GetDiskName(devName);
        std::string diskSerialId;
        SerialIdInformation diskSerialIdInfo;

        if (SystemInterface::GetInstance()->GetDiskSerialIdInformation(
                collectContext.GetMetricTime(), diskName, diskSerialIdInfo)) {
            diskSerialId = diskSerialIdInfo.serialId;
        }

        MetricEvent* metricEvent = groupPtr->AddMetricEvent(true);
        DiskCollectStat diskCollectStat = mCurrentDiskCollectStatMap[devName];
        std::string dirName = JoinBytesLimit(diskCollectStat.deviceMountInfo.mountPaths, ",", kMaxDirSize);
        if (!metricEvent) {
            return false;
        }

        metricEvent->SetTimestamp(diskSerialIdInfo.collectTime, 0);
        metricEvent->SetTag(std::string("hostname"), hostname);
        metricEvent->SetTag(std::string("device"), devName);
        metricEvent->SetTag(std::string("id_serial"), diskSerialId);
        metricEvent->SetTag(std::string("diskname"), dirName);
        metricEvent->SetTag(std::string("m"), std::string("system.disk"));

        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();

        DeviceMetric minDeviceMetric, maxDeviceMetric, avgDeviceMetric;
        mDeviceCal.second.Stat(maxDeviceMetric, minDeviceMetric, avgDeviceMetric);
        mDeviceCal.second.Reset();

        struct MetricDef {
            const char* name;
            double* value;
        } metrics[] = {
            {"diskusage_total_avg", &avgDeviceMetric.total},
            {"diskusage_total_min", &minDeviceMetric.total},
            {"diskusage_total_max", &maxDeviceMetric.total},
            {"diskusage_used_avg", &avgDeviceMetric.used},
            {"diskusage_used_min", &minDeviceMetric.used},
            {"diskusage_used_max", &maxDeviceMetric.used},
            {"diskusage_free_avg", &avgDeviceMetric.free},
            {"diskusage_free_min", &minDeviceMetric.free},
            {"diskusage_free_max", &maxDeviceMetric.free},
            {"diskusage_avail_avg", &avgDeviceMetric.avail},
            {"diskusage_avail_min", &minDeviceMetric.avail},
            {"diskusage_avail_max", &maxDeviceMetric.avail},
            {"diskusage_utilization_avg", &avgDeviceMetric.usePercent},
            {"diskusage_utilization_min", &minDeviceMetric.usePercent},
            {"diskusage_utilization_max", &maxDeviceMetric.usePercent},
            {"disk_readiops_avg", &avgDeviceMetric.reads},
            {"disk_readiops_min", &minDeviceMetric.reads},
            {"disk_readiops_max", &maxDeviceMetric.reads},
            {"disk_writeiops_avg", &avgDeviceMetric.writes},
            {"disk_writeiops_min", &minDeviceMetric.writes},
            {"disk_writeiops_max", &maxDeviceMetric.writes},
            {"disk_writebytes_avg", &avgDeviceMetric.writeBytes},
            {"disk_writebytes_min", &minDeviceMetric.writeBytes},
            {"disk_writebytes_max", &maxDeviceMetric.writeBytes},
            {"disk_readbytes_avg", &avgDeviceMetric.readBytes},
            {"disk_readbytes_min", &minDeviceMetric.readBytes},
            {"disk_readbytes_max", &maxDeviceMetric.readBytes},
            {"fs_inodeutilization_avg", &avgDeviceMetric.inodePercent},
            {"fs_inodeutilization_min", &minDeviceMetric.inodePercent},
            {"fs_inodeutilization_max", &maxDeviceMetric.inodePercent},
            {"DiskIOQueueSize_avg", &avgDeviceMetric.avgqu_sz},
            {"DiskIOQueueSize_min", &minDeviceMetric.avgqu_sz},
            {"DiskIOQueueSize_max", &maxDeviceMetric.avgqu_sz},
        };
        for (const auto& def : metrics) {
            multiDoubleValues->SetValue(std::string(def.name),
                                        UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, *def.value});
        }
    }

    return true;
}

template <typename T>
constexpr bool IsNumeric() {
    return std::is_arithmetic<T>::value;
}
template <typename T1, typename T2, typename... TOthers>
constexpr bool IsNumeric() {
    return IsNumeric<T1>() && IsNumeric<T2, TOthers...>();
}
template <typename T>
double GetRatio(const T& curr, const T& prev, double interval) {
    auto delta = static_cast<double>(curr > prev ? curr - prev : 0);
    return interval == 0 ? 0.0 : (delta / interval);
}

// T 是否无符号整数
template <typename T>
constexpr bool is_uint() {
    return std::is_integral<T>::value && std::is_unsigned<T>::value;
}
// 无符号整数，支持溢出情况下的循环计算
template <typename T, typename std::enable_if<is_uint<T>(), int>::type = 0>
T Delta(const T& a, const T& b) {
    if (a < b) {
        // 溢出了
        return std::numeric_limits<T>::max() - b + a;
    } else {
        return a - b;
    }
}

// T1, T2不是相同数字类型，或不是无符号整数，不支持溢出情况下的循环计算
template <typename T1,
          typename T2,
          typename std::enable_if<IsNumeric<T1, T2>() && (!std::is_same<T1, T2>::value || !is_uint<T1>()), int>::type
          = 0>
auto Delta(const T1& a, const T2& b) -> decltype(a - b) {
    return a > b ? a - b : 0;
}
void DiskCollector::CalcDiskMetric(const DiskStat& current,
                                   const DiskStat& last,
                                   double interval,
                                   DiskMetric& diskMetric) {
    diskMetric.reads = GetRatio(current.reads, last.reads, interval);
    diskMetric.writes = GetRatio(current.writes, last.writes, interval);
    diskMetric.writeBytes = GetRatio(current.writeBytes, last.writeBytes, interval);
    diskMetric.readBytes = GetRatio(current.readBytes, last.readBytes, interval);
    diskMetric.avgqu_sz = current.queue;
    diskMetric.svctm = current.service_time;
    uint64_t rd_t = Delta(current.rtime, last.rtime);
    uint64_t wr_t = Delta(current.wtime, last.wtime);
    uint64_t rd_ios = Delta(current.reads, last.reads);
    uint64_t wr_ios = Delta(current.writes, last.writes);
    uint64_t rd_sec = Delta(current.readBytes, last.readBytes) / 512;
    uint64_t wr_sec = Delta(current.writeBytes, last.writeBytes) / 512;
    uint64_t tick = Delta(current.time, last.time);
    diskMetric.w_await = wr_ios > 0 ? wr_t / wr_ios : 0.0;
    diskMetric.r_await = rd_ios > 0 ? rd_t / rd_ios : 0.0;
    diskMetric.await = (rd_ios + wr_ios) > 0 ? (wr_t + rd_t) / (rd_ios + wr_ios) : 0.0;
    diskMetric.avgrq_sz = (rd_ios + wr_ios) > 0 ? (rd_sec + wr_sec) / (rd_ios + wr_ios) : 0.0;
    diskMetric.util = tick / (10.0 * interval);
}

int DiskCollector::GetDiskCollectStatMap(const CollectTime& collectTime,
                                         std::map<std::string, DiskCollectStat>& diskCollectStatMap) {
    std::map<std::string, DeviceMountInfo> deviceMountMap;
    int num = GetDeviceMountMap(collectTime, deviceMountMap);
    if (num <= 0) {
        return num;
    }

    for (auto& it : deviceMountMap) {
        std::string dirName = it.second.mountPaths[0];
        FileSystemUsage fileSystemStat;
        // 只有在获取文件系统信息成功之后才进行磁盘信息的获取
        if (GetFileSystemStat(collectTime, dirName, fileSystemStat) != 0) {
            continue;
        }

        DiskCollectStat diskCollectStat;
        diskCollectStat.deviceMountInfo = it.second;
#define CastUint64(Expr) static_cast<uint64_t>(Expr)
#define CastDouble(Expr) static_cast<double>(Expr)
        diskCollectStat.space.total = CastDouble(fileSystemStat.total) * 1024;
        diskCollectStat.space.free = CastDouble(fileSystemStat.free) * 1024;
        diskCollectStat.space.used = CastDouble(fileSystemStat.used) * 1024;
        diskCollectStat.space.usePercent = fileSystemStat.use_percent * 100.0;
        diskCollectStat.spaceAvail = CastDouble(fileSystemStat.avail) * 1024;

        diskCollectStat.inode.total = CastDouble(fileSystemStat.files);
        diskCollectStat.inode.free = CastDouble(fileSystemStat.freeFiles);
        diskCollectStat.inode.used = fileSystemStat.files > fileSystemStat.freeFiles
            ? CastDouble(fileSystemStat.files - fileSystemStat.freeFiles)
            : 0.0;
        if (fileSystemStat.files != 0) {
            diskCollectStat.inode.usePercent
                = (diskCollectStat.inode.used * 100.0) / (diskCollectStat.inode.total * 1.0);
        }

        diskCollectStat.diskStat.reads = CastUint64(fileSystemStat.disk.reads);
        diskCollectStat.diskStat.writes = CastUint64(fileSystemStat.disk.writes);
        diskCollectStat.diskStat.writeBytes = CastUint64(fileSystemStat.disk.writeBytes);
        diskCollectStat.diskStat.readBytes = CastUint64(fileSystemStat.disk.readBytes);
        diskCollectStat.diskStat.rtime = CastUint64(fileSystemStat.disk.rTime);
        diskCollectStat.diskStat.wtime = CastUint64(fileSystemStat.disk.wTime);
        diskCollectStat.diskStat.qtime = CastUint64(fileSystemStat.disk.qTime);
        diskCollectStat.diskStat.time = CastUint64(fileSystemStat.disk.time);
        diskCollectStat.diskStat.service_time
            = CastDouble(fileSystemStat.disk.serviceTime >= 0 ? fileSystemStat.disk.serviceTime : 0.0);
        diskCollectStat.diskStat.queue = fileSystemStat.disk.queue >= 0 ? fileSystemStat.disk.queue : 0.0;
        diskCollectStatMap[it.first] = diskCollectStat;
#undef CastDouble
#undef CastUint64
    }
    return static_cast<int>(diskCollectStatMap.size());
}

int DiskCollector::GetFileSystemStat(const CollectTime& collectTime,
                                     const std::string& dirName,
                                     FileSystemUsage& fileSystemUsage) {
    FileSystemInformation fileSystemInfo;

    if (!SystemInterface::GetInstance()->GetFileSystemInformation(collectTime.mMetricTime, dirName, fileSystemInfo)) {
        return -1;
    }

    fileSystemUsage.total = fileSystemInfo.fileSystemState.total;
    fileSystemUsage.free = fileSystemInfo.fileSystemState.free;
    fileSystemUsage.avail = fileSystemInfo.fileSystemState.avail;
    fileSystemUsage.used = fileSystemInfo.fileSystemState.used;
    fileSystemUsage.files = fileSystemInfo.fileSystemState.files;
    fileSystemUsage.freeFiles = fileSystemInfo.fileSystemState.freeFiles;
    fileSystemUsage.use_percent = fileSystemInfo.fileSystemState.use_percent;

    GetDiskUsage(collectTime, fileSystemUsage.disk, dirName);

    return 0;
}

int DiskCollector::GetDiskStat(const CollectTime& collectTime, dev_t rDev, DiskUsage& disk, DiskUsage& deviceUsage) {
    std::vector<std::string> diskLines = {};
    std::string errorMessage;

    DiskStateInformation diskStateInfo;
    if (!SystemInterface::GetInstance()->GetDiskStateInformation(collectTime.mMetricTime, diskStateInfo)) {
        return -1;
    }
    for (auto const& diskState : diskStateInfo.diskStats) {
        if (diskState.major == major(rDev) && (0 == diskState.minor || diskState.minor == minor(rDev))) {
            disk.reads = diskState.reads;
            disk.readBytes = diskState.readBytes;
            disk.rTime = diskState.rTime;
            disk.writes = diskState.writes;
            disk.writeBytes = diskState.writeBytes;
            disk.wTime = diskState.wTime;
            disk.time = diskState.time;
            disk.qTime = diskState.qTime;
            deviceUsage = disk;

            if (diskState.minor == minor(rDev)) {
                return 0;
            }
        }
    }

    return -1;
}

int DiskCollector::CalDiskUsage(const CollectTime& collectTime, IODev& ioDev, DiskUsage& diskUsage) {
    SystemUptimeInformation uptimeInfo;
    if (!SystemInterface::GetInstance()->GetSystemUptimeInformation(collectTime.mMetricTime, uptimeInfo)) {
        return -1;
    }

    diskUsage.snapTime = uptimeInfo.uptime;
    double interval = diskUsage.snapTime - ioDev.diskUsage.snapTime;

    diskUsage.serviceTime = -1;
    if (diskUsage.time != std::numeric_limits<uint64_t>::max()) {
        uint64_t ios
            = DiffOrZero(diskUsage.reads, ioDev.diskUsage.reads) + DiffOrZero(diskUsage.writes, ioDev.diskUsage.writes);
        double tmp = ((double)ios) * HZ / interval;
        double util = ((double)(diskUsage.time - ioDev.diskUsage.time)) / interval * HZ;

        diskUsage.serviceTime = (tmp != 0 ? util / tmp : 0);
    }

    diskUsage.queue = -1;
    if (diskUsage.qTime != std::numeric_limits<uint64_t>::max()) {
        // 浮点运算：0.0/0.0 => nan, 1.0/0.0 => inf
        double util = ((double)(diskUsage.qTime - ioDev.diskUsage.qTime)) / interval;
        diskUsage.queue = util / 1000.0;
    }

    if (!std::isfinite(diskUsage.queue)) {
        std::stringstream ss;
        ss << "diskUsage.queue is not finite: " << diskUsage.queue << std::endl
           << "                       uptime: " << uptimeInfo.uptime << " s" << std::endl
           << "                     interval: " << interval << " s" << std::endl
           << "              diskUsage.qTime: " << diskUsage.qTime << std::endl
           << "        ioDev.diskUsage.qTime: " << ioDev.diskUsage.qTime << std::endl;
        LOG_ERROR(sLogger, ("diskUsage.queue calculated failed", ss.str()));
    }

    ioDev.diskUsage = diskUsage;

    return 0;
}
int DiskCollector::GetDiskUsage(const CollectTime& collectTime, DiskUsage& diskUsage, std::string dirName) {
    std::shared_ptr<IODev> ioDev;
    DiskUsage deviceUsage{};
    int status = GetIOstat(collectTime, dirName, diskUsage, ioDev, deviceUsage);

    if (status == 0 && ioDev) {
        // if (ioDev->isPartition) {
        //     /* 2.6 kernels do not have per-partition times */
        //     diskUsage = deviceUsage;
        // }
        diskUsage.devName = ioDev->name;
        diskUsage.dirName = dirName;
        status = CalDiskUsage(collectTime, *ioDev, (ioDev->isPartition ? deviceUsage : diskUsage));
        if (status == 0 && ioDev->isPartition) {
            diskUsage.serviceTime = deviceUsage.serviceTime;
            diskUsage.queue = deviceUsage.queue;
        }
    }

    return status;
}

// dirName可以是devName，也可以是dirName
int DiskCollector::GetIOstat(const CollectTime& collectTime,
                             std::string& dirName,
                             DiskUsage& disk,
                             std::shared_ptr<IODev>& ioDev,
                             DiskUsage& deviceUsage) {
    // 本函数的思路dirName -> devName -> str_rdev(设备号)
    // 1. 通过dirName找到devName
    ioDev = GetIODev(collectTime, dirName);
    if (!ioDev) {
        return -1;
    }

    struct stat ioStat {};
    // 此处使用设备名，以获取 更多stat信息，如st_rdev(驱动号、设备号)
    // 其实主要目的就是为了获取st_rdev
    if (stat(ioDev->name.c_str(), &ioStat) < 0) {
        return -1;
    }

    // 2. 统计dev的磁盘使用情况
    return GetDiskStat(collectTime, ioStat.st_rdev, disk, deviceUsage);
}

bool IsDev(const std::string& dirName) {
    return StartWith(dirName, "/dev/");
}

static uint64_t cacheId(const struct stat& ioStat) {
    return S_ISBLK(ioStat.st_mode) ? ioStat.st_rdev : (ioStat.st_ino + ioStat.st_dev);
}
std::shared_ptr<IODev> DiskCollector::GetIODev(const CollectTime& collectTime, std::string& dirName) {
    if (!StartWith(dirName, "/")) {
        dirName = "/dev/" + dirName;
    }

    struct stat ioStat {};
    if (stat(dirName.c_str(), &ioStat) < 0) {
        return std::shared_ptr<IODev>{};
    }

    uint64_t targetId = cacheId(ioStat);

    if (fileSystemCache.find(targetId) != fileSystemCache.end()) {
        return fileSystemCache[targetId];
    }

    if (IsDev(dirName)) {
        // 如果确定是设备文件，则直接缓存，无需再枚举设备列表
        auto ioDev = std::make_shared<IODev>();
        ioDev->name = dirName;
        fileSystemCache[targetId] = ioDev;
        return ioDev;
    }

    RefreshLocalDisk(collectTime);

    auto targetIt = fileSystemCache.find(targetId);
    if (targetIt != fileSystemCache.end() && !targetIt->second->name.empty()) {
        return targetIt->second;
    }
    return std::shared_ptr<IODev>{};
}

void DiskCollector::RefreshLocalDisk(const CollectTime& collectTime) {
    FileSystemListInformation informations;
    if (SystemInterface::GetInstance()->GetFileSystemListInformation(collectTime.mMetricTime, informations)) {
        for (auto const& fileSystem : informations.fileSystemList) {
            if (fileSystem.type == FILE_SYSTEM_TYPE_LOCAL_DISK && IsDev(fileSystem.devName)) {
                struct stat ioStat {};
                if (stat(fileSystem.dirName.c_str(), &ioStat) < 0) {
                    continue;
                }
                uint64_t id = cacheId(ioStat);
                if (fileSystemCache.find(id) == fileSystemCache.end()) {
                    auto ioDev = std::make_shared<IODev>();
                    ioDev->isPartition = true;
                    ioDev->name = fileSystem.devName;
                    fileSystemCache[id] = ioDev;
                }
            }
        }
    }
}

int DiskCollector::GetDeviceMountMap(const CollectTime& collectTime,
                                     std::map<std::string, DeviceMountInfo>& deviceMountMap) {
    if (collectTime.mScheduleTime < mDeviceMountMapExpireTime) {
        deviceMountMap = mDeviceMountMap;
        return static_cast<int>(deviceMountMap.size());
    }
    mDeviceMountMapExpireTime = collectTime.mScheduleTime + std::chrono::seconds(60);
    deviceMountMap.clear();

    std::vector<FileSystemInfo> fileSystemInfos;
    if (GetFileSystemInfos(collectTime, fileSystemInfos) != 0) {
        // 走到这里时，就意味着mDeviceMountMapExpire又续了一条命
        return -1;
    }

    std::map<std::string, FileSystemInfo> mountMap;
    for (auto& fileSystemInfo : fileSystemInfos) {
        mountMap[fileSystemInfo.dirName] = fileSystemInfo;
    }

    for (auto& it : mountMap) {
        std::string devName = it.second.devName;
        if (deviceMountMap.find(devName) == deviceMountMap.end()) {
            DeviceMountInfo deviceMountInfo;
            deviceMountInfo.devName = devName;
            deviceMountInfo.type = it.second.type;
            deviceMountMap[devName] = deviceMountInfo;
        }
        deviceMountMap[devName].mountPaths.push_back(it.second.dirName);
    }
    // sort the dirName;

    for (auto& itD : deviceMountMap) {
        sort(itD.second.mountPaths.begin(), itD.second.mountPaths.end());
    }
    mDeviceMountMap = deviceMountMap;
    return static_cast<int>(deviceMountMap.size());
}

int DiskCollector::GetFileSystemInfos(const CollectTime& collectTime, std::vector<FileSystemInfo>& fileSystemInfos) {
    FileSystemListInformation informations;
    if (!SystemInterface::GetInstance()->GetFileSystemListInformation(collectTime.mMetricTime, informations)) {
        return -1;
    }

    for (auto& fileSystem : informations.fileSystemList) {
        if (fileSystem.type != FILE_SYSTEM_TYPE_LOCAL_DISK) {
            continue;
        }
        FileSystemInfo fileSystemInfo;
        fileSystemInfo.dirName = fileSystem.dirName;
        fileSystemInfo.devName = fileSystem.devName;
        fileSystemInfo.type = fileSystem.sysTypeName;
        fileSystemInfos.push_back(fileSystemInfo);
    }
    return 0;
}

// 获取设备的名称
// input:/dev/sda1, output:sda
// input:/dev/sda10,output:sda
std::string DiskCollector::GetDiskName(const std::string& dev) {
    std::string device = dev;
    size_t index = device.find("/dev/");
    if (index != std::string::npos) {
        device = device.substr(5);
    }
    for (int i = static_cast<int>(device.size()) - 1; i >= 0; i--) {
        if (device[i] < '0' || device[i] > '9') {
            return device.substr(0, i + 1);
        }
    }
    return device;
}

const std::chrono::seconds DiskCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_disk_collect_interval));
}

} // namespace logtail
