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

#include <string>

// #include <boost/lexical_cast.hpp>
#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/split.hpp"

#include "MetricValue.h"
#include "common/FileSystemUtil.h"
#include "common/StringTools.h"
#include "host_monitor/Constants.h"
#include "host_monitor/SystemInformationTools.h"
#include "logger/Logger.h"
#include "monitor/Monitor.h"

namespace logtail {

const std::string DiskCollector::sName = "disk";

template <typename T>
T Diff(const T& a, const T& b) {
    return a > b ? a - b : T{0};
}

bool IsZero(const std::chrono::steady_clock::time_point& t) {
    return t.time_since_epoch().count() == 0;
}
bool IsZero(const std::chrono::system_clock::time_point& t) {
    return t.time_since_epoch().count() == 0;
}
DiskCollector::DiskCollector() {
    Init();
}
int DiskCollector::Init(int totalCount) {
    mCountPerReport = totalCount;
    mCount = 0;
    mLastTime = std::chrono::steady_clock::time_point{};
    mDeviceMountMapExpireTime = std::chrono::steady_clock::time_point{};
    return 0;
}

const struct {
    SicFileSystemType fs;
    const char* name;
} fsTypeNames[] = {
    {SIC_FILE_SYSTEM_TYPE_UNKNOWN, "unknown"},
    {SIC_FILE_SYSTEM_TYPE_NONE, "none"},
    {SIC_FILE_SYSTEM_TYPE_LOCAL_DISK, "local"},
    {SIC_FILE_SYSTEM_TYPE_NETWORK, "remote"},
    {SIC_FILE_SYSTEM_TYPE_RAM_DISK, "ram"},
    {SIC_FILE_SYSTEM_TYPE_CDROM, "cdrom"},
    {SIC_FILE_SYSTEM_TYPE_SWAP, "swap"},
};
constexpr size_t fsTypeNamesCount = sizeof(fsTypeNames) / sizeof(fsTypeNames[0]);
static_assert(SIC_FILE_SYSTEM_TYPE_MAX == fsTypeNamesCount, "fsTypeNames size not matched");

std::string GetName(SicFileSystemType fs) {
    int idx = static_cast<int>(fs);
    if (0 <= idx && (size_t)idx < fsTypeNamesCount && fsTypeNames[idx].fs == fs) {
        return fsTypeNames[idx].name;
    }
    return "";
}

// 最少一条
template <typename T>
std::string join_n(const T& v, const std::string& splitter, size_t n) {
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
    // 去掉最后一个分隔符
    return result;
}

bool DiskCollector::Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) {
    if (group == nullptr) {
        return false;
    }

    std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
    std::map<std::string, DiskCollectStat> diskCollectStatMap;
    if (GetDiskCollectStatMap(diskCollectStatMap) <= 0) {
        LOG_WARNING(sLogger, ("collect disk error", "skip")("time", ""));
        return false;
    }
    // std::cout << "mLastTime: "
    //           << std::chrono::duration_cast<std::chrono::milliseconds>(mLastTime.time_since_epoch()).count()
    //           << std::endl;
    // std::cout << "currentTime: "
    //           << std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()).count()
    //           << std::endl;
    mCurrentDiskCollectStatMap = diskCollectStatMap;
    if (IsZero(mLastTime)) {
        LOG_WARNING(sLogger, ("collect disk first time", "skip")("time", ""));
        mLastDiskCollectStatMap = mCurrentDiskCollectStatMap;
        mLastTime = currentTime;
        return true;
    }
    if (mLastTime + std::chrono::milliseconds(1) >= currentTime) {
        // 调度间隔不能低于1ms
        LOG_WARNING(sLogger, ("collect disk too frequency", "skip")("time", ""));
        return false;
    }
    auto interval = std::chrono::duration_cast<std::chrono::duration<double>>(currentTime - mLastTime);
    mLastTime = currentTime;
    mCount++;

    // DeviceMetric deviceMetric{};

    for (auto& it : mCurrentDiskCollectStatMap) {
        const std::string& devName = it.first;
        DeviceMetric deviceMetric{};
        if (mLastDiskCollectStatMap.find(devName) != mLastDiskCollectStatMap.end()) {
            const DiskCollectStat& currentStat = mCurrentDiskCollectStatMap[devName];
            const DiskCollectStat& lastStat = mLastDiskCollectStatMap[devName];
            DiskMetric diskMetric;
            // std::cout << "\ndevName:" << std::string(devName) << std::endl;
            // std::cout << "total:" << it.second.space.total / 1024 / 1024 << std::endl;
            // std::cout << "free:" << it.second.space.free / 1024 / 1024 << std::endl;
            // std::cout << "used:" << it.second.space.used / 1024 / 1024 << std::endl;
            // std::cout << "usePercent:" << it.second.space.usePercent << std::endl;
            // std::cout << "spaceAvail:" << it.second.spaceAvail / 1024 / 1024 << std::endl;
            CalcDiskMetric(currentStat.diskStat, lastStat.diskStat, interval.count(), diskMetric);
            // std::cout << "reads:" << diskMetric.reads << std::endl;
            // std::cout << "writes:" << diskMetric.writes << std::endl;
            // std::cout << "readBytes:" << diskMetric.readBytes << std::endl;
            // std::cout << "writeBytes:" << diskMetric.writeBytes << std::endl;
            // std::cout << "inode.usePercent:" << it.second.inode.usePercent << std::endl;
            // std::cout << "avgqu_sz:" << diskMetric.avgqu_sz << std::endl;
            //  mMetricCalculateMap[devName].AddValue(diskMetric);
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

            // mDeviceCalMap[devName].AddValue(deviceMetric);
        }
    }
    mLastDiskCollectStatMap = mCurrentDiskCollectStatMap;

    if (mCount < mCountPerReport) {
        return true;
    }

    // std::diskName = GetDiskName(devName);
    // std::string diskSerialId = "";
    // SicGetDiskSerialId(diskName,serialId);
    // std::cout << "\ndiskName: " << std::string(diskName) << std::endl;
    // std::cout << "\nserialId: " << std::string(serialId) << std::endl;
    const time_t now = time(nullptr);
    auto hostname = LoongCollectorMonitor::GetInstance()->mHostname;

    for (auto& mDeviceCal : mDeviceCalMap) {
        std::string devName = mDeviceCal.first;
        std::string diskName = GetDiskName(devName);
        std::string diskSerialId = "";
        SicGetDiskSerialId(diskName, diskSerialId);
        MetricEvent* metricEvent = group->AddMetricEvent(true);
        DiskCollectStat diskCollectStat = mCurrentDiskCollectStatMap[devName];
        std::string dir_name = join_n(diskCollectStat.deviceMountInfo.mountPaths, ",", maxDirSize);
        if (!metricEvent) {
            return false;
        }

        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetTag(std::string("hostname"), hostname);
        metricEvent->SetTag(std::string("device"), devName);
        metricEvent->SetTag(std::string("id_serial"), diskSerialId);
        metricEvent->SetTag(std::string("diskname"), dir_name);
        // std::cout << "\ndiskName: " << std::string(diskName) << std::endl;
        // std::cout << "\nserialId: " << std::string(diskSerialId) << std::endl;
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
        // ResNetPackRate minPackRate, maxPackRate, avgPackRate;
        // packRateCal.second.Stat(maxPackRate, minPackRate, avgPackRate);
        // packRateCal.second.Reset();

        // LogDebug("collect disk spend {:.3f}ms", tp.cost<fraction_millis>().count());
        // if (mCount < mTotalCount) {
        //     return true;
        // }
        // mCount = 0;
    }

    return true;
}

template <typename T>
constexpr bool is_numeric() {
    return std::is_arithmetic<T>::value;
}
template <typename T1, typename T2, typename... TOthers>
constexpr bool is_numeric() {
    return is_numeric<T1>() && is_numeric<T2, TOthers...>();
}
template <typename T>
double GetRatio(const T& prev, const T& curr, double interval) {
    auto delta = static_cast<double>(prev > curr ? prev - curr : 0);
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
          typename std::enable_if<is_numeric<T1, T2>() && (!std::is_same<T1, T2>::value || !is_uint<T1>()), int>::type
          = 0>
auto Delta(const T1& a, const T2& b) -> decltype(a - b) {
    return a > b ? a - b : 0;
}
void DiskCollector::CalcDiskMetric(const DiskStat& current,
                                   const DiskStat& last,
                                   double interval,
                                   DiskMetric& diskMetric) {
    // GetRatios((uint64_t *) &current, (uint64_t *) &last, interval, 4, (double *) &diskMetric);
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
void DiskCollector::GetDiskMetricData(const std::string& metricName,
                                      const std::string& devName,
                                      const std::string& diskSerialId,
                                      double value,
                                      const std::string& ns,
                                      MetricData& metricData) {
    metricData.tagMap["metricName"] = metricName;
    metricData.tagMap["diskname"] = devName;
    metricData.valueMap["metricValue"] = value;
    metricData.tagMap["ns"] = ns;
    if (!diskSerialId.empty()) {
        metricData.tagMap["id_serial"] = diskSerialId;
    }
}
int DiskCollector::GetDiskCollectStatMap(std::map<std::string, DiskCollectStat>& diskCollectStatMap) {
    std::map<std::string, DeviceMountInfo> deviceMountMap;
    int num = GetDeviceMountMap(deviceMountMap);
    if (num <= 0) {
        return num;
    }

    for (auto& it : deviceMountMap) {
        std::string dirName = it.second.mountPaths[0];
        // string devName = it.second.devName;
        SicFileSystemUsage fileSystemStat;
        // 只有在获取文件系统信息成功之后才进行磁盘信息的获取
        if (GetFileSystemStat(dirName, fileSystemStat) != 0) { // || GetDiskStat(devName, fileSystemStat) != 0) {
            continue;
        }
        // if (std::isinf(fileSystemStat.disk.queue) && !this->SicPtr()->errorMessage.empty()) {
        //     LogInfo("{}", SicPtr()->errorMessage);
        // }
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

int DiskCollector::GetFileSystemStat(const std::string& dirName, SicFileSystemUsage& fileSystemUsage) {
    struct statvfs buffer {};
    int status = statvfs(dirName.c_str(), &buffer);
    if (status != 0) {
        return status;
    }

    // 单位是: KB
    uint64_t bsize = buffer.f_frsize / 512;
    fileSystemUsage.total = ((buffer.f_blocks * bsize) >> 1);
    fileSystemUsage.free = ((buffer.f_bfree * bsize) >> 1);
    fileSystemUsage.avail = ((buffer.f_bavail * bsize) >> 1); // 非超级用户最大可使用的磁盘量
    fileSystemUsage.used = Diff(fileSystemUsage.total, fileSystemUsage.free);
    fileSystemUsage.files = buffer.f_files;
    fileSystemUsage.freeFiles = buffer.f_ffree;

    // 此处为用户可使用的磁盘量，可能会与fileSystemUsage.total有差异。也就是说:
    // 当total < fileSystemUsage.total时，表明即使磁盘仍有空间，用户也申请不到了
    // 毕竟OS维护磁盘，会占掉一部分，比如文件分配表，目录文件等。
    uint64_t total = fileSystemUsage.used + fileSystemUsage.avail;
    uint64_t used = fileSystemUsage.used;
    double percent = 0;
    if (total != 0) {
        // 磁盘占用率，使用的是用户最大可用磁盘总量来的，而非物理磁盘总量
        percent = (double)used / (double)total;
    }
    fileSystemUsage.use_percent = percent;

    SicGetDiskUsage(fileSystemUsage.disk, dirName);

    return 0;
}

enum class EnumDiskStats {
    major,
    minor,
    devName,

    reads,
    readsMerged,
    readSectors,
    rMillis,

    writes,
    writesMerged,
    writeSectors,
    wMillis,

    ioCount,
    rwMillis, // 输入输出花费的毫秒数
    qMillis, // 输入/输出操作花费的加权毫秒数

    count, // 这个用于收尾，不是实际的列号。
};
static_assert((int)EnumDiskStats::count == 14, "EnumDiskStats::count unexpected");
int DiskCollector::GetDiskStat(dev_t rDev, const std::string& dirName, SicDiskUsage& disk, SicDiskUsage& deviceUsage) {
    std::vector<std::string> diskLines = {};
    std::string errorMessage;

    if (!GetHostSystemStatWithPath(diskLines, errorMessage, PROCESS_DISKSTATS)) {
        LOG_WARNING(sLogger, ("failed to get diskLines", "invalid disk collector")("error msg", errorMessage));

        return SIC_EXECUTABLE_FAILED;
    }
    int ret = SIC_EXECUTABLE_SUCCESS;

    if (ret == SIC_EXECUTABLE_SUCCESS) {
        for (auto const& diskLine : diskLines) {
            std::vector<std::string> diskMetric;
            boost::split(diskMetric,
                         boost::algorithm::trim_left_copy(diskLine),
                         boost::is_any_of(" "),
                         boost::token_compress_on);
            if (diskMetric.size() < (size_t)EnumDiskStats::count) {
                continue;
            }
            try {
                auto get_int = [&](EnumDiskStats key) -> uint64_t {
                    const std::string& s = diskMetric[(int)key];
                    return static_cast<uint64_t>(std::stoull(s));
                };
                // int currentIndex = 0;
                // 1  major number
                // auto devMajor = convert<decltype(major(rDev))>(diskMetric[(int)EnumDiskStats::major]);
                uint64_t majorVal = get_int(EnumDiskStats::major);
                uint64_t minorVal = get_int(EnumDiskStats::minor);
                unsigned int devMajor = static_cast<unsigned int>(majorVal);
                unsigned int devMinor = static_cast<unsigned int>(minorVal);

                // unsigned int devMajor = static_cast<unsigned int>(std::stoul(diskMetric[(int)EnumDiskStats::major]));
                //  2  minor number
                //  auto devMinor = convert<decltype(minor(rDev))>(diskMetric[(int)EnumDiskStats::minor]);
                // unsigned int devMinor = static_cast<unsigned int>(std::stoul(diskMetric[(int)EnumDiskStats::minor]));
                if (devMajor == major(rDev) && (0 == devMinor || devMinor == minor(rDev))) {
                    // 3  device name
                    // ++currentIndex;
                    // 4  reads completed successfully
                    disk.reads = get_int(EnumDiskStats::reads);
                    // 5  reads merged
                    // ++currentIndex;
                    //	6  sectors read
                    disk.readBytes = get_int(EnumDiskStats::readSectors) * 512;
                    // 7  time spent reading (ms)
                    disk.rTime = get_int(EnumDiskStats::rMillis);
                    // 8  writes completed
                    disk.writes = get_int(EnumDiskStats::writes);
                    // 9  writes merged
                    // ++currentIndex;
                    // 10  sectors written
                    disk.writeBytes = get_int(EnumDiskStats::writeSectors) * 512;
                    // 11  time spent writing (ms)
                    disk.wTime = get_int(EnumDiskStats::wMillis);
                    // 12  I/Os currently in progress
                    // ++currentIndex;
                    // 13  time spent doing I/Os (ms)
                    disk.time = get_int(EnumDiskStats::rwMillis);
                    // 14  weighted time spent doing I/Os (ms)
                    disk.qTime = get_int(EnumDiskStats::qMillis);
                    if (devMinor == 0) {
                        deviceUsage = disk;
                    }
                    if (devMinor == minor(rDev)) {
                        return SIC_EXECUTABLE_SUCCESS;
                    }
                }
            } catch (...) {
                LOG_ERROR(sLogger, ("failed to parse number in diskstats", diskLine));
                return SIC_EXECUTABLE_FAILED; // 默认值
            }
        }
        ret = SIC_EXECUTABLE_FAILED;
    }

    return ret;
}

/*
> cat /proc/uptime
183857.30 1969716.84
第一列: 系统启动到现在的时间（以秒为单位）；
第二列: 系统空闲的时间（以秒为单位）。
*/
int DiskCollector::SicGetUpTime(double& uptime) {
    std::vector<std::string> uptimeLines;
    std::string errorMessage;
    int ret = GetFileLines(PROCESS_UPTIME, uptimeLines, true, &errorMessage);
    if (ret == SIC_EXECUTABLE_SUCCESS) {
        std::vector<std::string> uptimeMetric;
        boost::split(uptimeMetric,
                     (uptimeLines.empty() ? "" : uptimeLines.front()),
                     boost::is_any_of(" "),
                     boost::token_compress_on);
        //  uptime = convert<double>(uptimeMetric.front());
        uptime = std::stod(uptimeMetric.front());
    }
    return ret;
}

int DiskCollector::CalDiskUsage(SicIODev& ioDev, SicDiskUsage& diskUsage) {
    double uptime;
    int status = SicGetUpTime(uptime);
    if (status == SIC_EXECUTABLE_SUCCESS) {
        diskUsage.snapTime = uptime;

        double interval = diskUsage.snapTime - ioDev.diskUsage.snapTime;

        diskUsage.serviceTime = -1;
        if (diskUsage.time != std::numeric_limits<uint64_t>::max()) {
            uint64_t ios
                = Diff(diskUsage.reads, ioDev.diskUsage.reads) + Diff(diskUsage.writes, ioDev.diskUsage.writes);
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
               << "                       uptime: " << uptime << " s" << std::endl
               << "                     interval: " << interval << " s" << std::endl
               << "              diskUsage.qTime: " << diskUsage.qTime << std::endl
               << "        ioDev.diskUsage.qTime: " << ioDev.diskUsage.qTime << std::endl;
            // std::cout << ss.str();
        }

        ioDev.diskUsage = diskUsage;
    }
    return status;
}
int DiskCollector::SicGetDiskUsage(SicDiskUsage& diskUsage, std::string dirName) {
    std::shared_ptr<SicIODev> ioDev;
    SicDiskUsage deviceUsage{};
    int status = SicGetIOstat(dirName, diskUsage, ioDev, deviceUsage);

    if (status == SIC_EXECUTABLE_SUCCESS && ioDev) {
        // if (ioDev->isPartition) {
        //     /* 2.6 kernels do not have per-partition times */
        //     diskUsage = deviceUsage;
        // }
        diskUsage.devName = ioDev->name;
        diskUsage.dirName = dirName;
        status = CalDiskUsage(*ioDev, (ioDev->isPartition ? deviceUsage : diskUsage));
        if (status == SIC_EXECUTABLE_SUCCESS && ioDev->isPartition) {
            diskUsage.serviceTime = deviceUsage.serviceTime;
            diskUsage.queue = deviceUsage.queue;
        }
    }

    return status;
}

// dirName可以是devName，也可以是dirName
int DiskCollector::SicGetIOstat(std::string& dirName,
                                SicDiskUsage& disk,
                                std::shared_ptr<SicIODev>& ioDev,
                                SicDiskUsage& deviceUsage) {
    // 本函数的思路dirName -> devName -> str_rdev(设备号)
    // 1. 通过dirName找到devName
    ioDev = SicGetIODev(dirName);
    if (!ioDev) {
        return SIC_EXECUTABLE_FAILED;
    }

    struct stat ioStat {};
    // 此处使用设备名，以获取 更多stat信息，如st_rdev(驱动号、设备号)
    // 其实主要目的就是为了获取st_rdev
    if (stat(ioDev->name.c_str(), &ioStat) < 0) {
        return SIC_EXECUTABLE_FAILED;
    }
    // print(ioDev->name, ioStat);

    // 2. 统计dev的磁盘使用情况
    return GetDiskStat(ioStat.st_rdev, dirName, disk, deviceUsage);
}

bool IsDev(const std::string& dirName) {
    return StartWith(dirName, "/dev/");
}

static uint64_t cacheId(const struct stat& ioStat) {
    return S_ISBLK(ioStat.st_mode) ? ioStat.st_rdev : (ioStat.st_ino + ioStat.st_dev);
}
std::shared_ptr<SicIODev> DiskCollector::SicGetIODev(std::string& dirName) {
    if (!StartWith(dirName, "/")) {
        dirName = "/dev/" + dirName;
    }

    struct stat ioStat {};
    if (stat(dirName.c_str(), &ioStat) < 0) {
        // SicPtr()->errorMessage = (sout{} << "stat(" << dirName << ") error: " << strerror(errno)).str();
        return std::shared_ptr<SicIODev>{};
    }
    // print(dirName, ioStat);

    uint64_t targetId = cacheId(ioStat);
    if (fileSystemCache.find(targetId) != fileSystemCache.end()) {
        return fileSystemCache[targetId];
    }

    if (IsDev(dirName)) {
        // 如果确定是设备文件，则直接缓存，无需再枚举设备列表
        auto ioDev = std::make_shared<SicIODev>();
        ioDev->name = dirName;
        fileSystemCache[targetId] = ioDev;
        return ioDev;
    }

    RefreshLocalDisk();

    auto targetIt = fileSystemCache.find(targetId);
    if (targetIt != fileSystemCache.end() && !targetIt->second->name.empty()) {
        return targetIt->second;
    }
    // SicPtr()->errorMessage = (sout{} << "<" << dirName << "> not a valid disk folder").str();
    return std::shared_ptr<SicIODev>{};
}

void DiskCollector::RefreshLocalDisk() {
    // std::cout << "in RefreshLocalDisk" << std::endl;
    //  auto &cache = fileSystemCache;
    std::vector<SicFileSystem> fileSystemList;
    // int ret = GetFileSystemListInformation(fileSystemList);
    if (GetFileSystemListInformation(fileSystemList)) {
        for (auto const& fileSystem : fileSystemList) {
            if (fileSystem.type == SIC_FILE_SYSTEM_TYPE_LOCAL_DISK && IsDev(fileSystem.devName)) {
                struct stat ioStat {};
                if (stat(fileSystem.dirName.c_str(), &ioStat) < 0) {
                    continue;
                }
                uint64_t id = cacheId(ioStat);
                if (fileSystemCache.find(id) == fileSystemCache.end()) {
                    auto ioDev = std::make_shared<SicIODev>();
                    ioDev->isPartition = true;
                    ioDev->name = fileSystem.devName;
                    fileSystemCache[id] = ioDev;
                }
            }
        }
    }
}
int DiskCollector::GetDeviceMountMap(std::map<std::string, DeviceMountInfo>& deviceMountMap) {
    auto now = std::chrono::steady_clock::now(); // NowSeconds();
    if (now < mDeviceMountMapExpireTime) {
        deviceMountMap = mDeviceMountMap;
        return static_cast<int>(deviceMountMap.size());
    }
    mDeviceMountMapExpireTime = now + std::chrono::seconds(60); // CloudMonitorConst::kDefaultMountInfoInterval;
    deviceMountMap.clear();

    std::vector<FileSystemInfo> fileSystemInfos;
    if (GetFileSystemInfos(fileSystemInfos) != 0) {
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

int DiskCollector::GetFileSystemInfos(std::vector<FileSystemInfo>& fileSystemInfos) {
    std::vector<SicFileSystem> sicFileSystemList;
    // int F(SicGetFileSystemListInformation, sicFileSystemList)
    if (!GetFileSystemListInformation(sicFileSystemList)) {
        return -1;
    }

    for (auto& sicFileSystem : sicFileSystemList) {
        if (sicFileSystem.type != SIC_FILE_SYSTEM_TYPE_LOCAL_DISK) {
            continue;
        }
        FileSystemInfo fileSystemInfo;
        fileSystemInfo.dirName = sicFileSystem.dirName;
        fileSystemInfo.devName = sicFileSystem.devName;
        fileSystemInfo.type = sicFileSystem.sysTypeName;
        fileSystemInfos.push_back(fileSystemInfo);
    }
    return 0;
}

#ifndef MOUNTED
#define MOUNTED "/etc/mtab"
#endif

bool DiskCollector::GetFileSystemListInformation(std::vector<SicFileSystem>& informations) {
    FILE* fp;

    // MOUNTED: /etc/mtab, defined in /usr/include/paths.h
    if (!(fp = setmntent(MOUNTED, "r"))) {
        return false;
    }
    // defer(endmntent(fp));

    mntent ent{};
    std::vector<char> buffer((size_t)4096);
    while (getmntent_r(fp, &ent, &buffer[0], buffer.size())) {
        SicFileSystem fileSystem;
        fileSystem.type = SIC_FILE_SYSTEM_TYPE_UNKNOWN;
        fileSystem.dirName = ent.mnt_dir;
        fileSystem.devName = ent.mnt_fsname;
        fileSystem.sysTypeName = ent.mnt_type;
        fileSystem.options = ent.mnt_opts;

        SicGetFileSystemType(fileSystem.sysTypeName, fileSystem.type, fileSystem.typeName);
        informations.push_back(fileSystem);
    }

    return true;
}

// 已知文件系统
const auto& knownFileSystem = *new std::unordered_map<std::string, SicFileSystemType>{
    {"adfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"affs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"anon-inode FS", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"befs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"bfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"btrfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"ecryptfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"efs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"futexfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"gpfs", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"hpfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"hfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"isofs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"k-afs", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"lustre", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"nilfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"openprom", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"reiserfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vzfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"xfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"xiafs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},

    // CommonFileSystem
    {"ntfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"smbfs", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"smb", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"swap", SIC_FILE_SYSTEM_TYPE_SWAP},
    {"afs", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"iso9660", SIC_FILE_SYSTEM_TYPE_CDROM},
    {"cvfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"cifs", SIC_FILE_SYSTEM_TYPE_NETWORK},
    {"msdos", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"minix", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vxfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"vfat", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
    {"zfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
};
const struct {
    const char* prefix;
    const SicFileSystemType fsType;
} knownFileSystemPrefix[] = {{"ext", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"gfs", SIC_FILE_SYSTEM_TYPE_NETWORK},
                             {"jffs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"jfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"minix", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},
                             {"ocfs", SIC_FILE_SYSTEM_TYPE_NETWORK},
                             {"psfs", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK},

                             {"nfs", SIC_FILE_SYSTEM_TYPE_NETWORK},
                             {"fat", SIC_FILE_SYSTEM_TYPE_LOCAL_DISK}};

bool DiskCollector::SicGetFileSystemType(const std::string& fsTypeName,
                                         SicFileSystemType& fsType,
                                         std::string& fsTypeDisplayName) {
    bool found = fsType != SIC_FILE_SYSTEM_TYPE_UNKNOWN;
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

    if (!found || fsType >= SIC_FILE_SYSTEM_TYPE_MAX) {
        fsType = SIC_FILE_SYSTEM_TYPE_NONE;
    }
    fsTypeDisplayName = GetName(fsType);
    // fsTypeDisplayName = fsType;

    return found;
}
std::string DiskCollector::FormatDir(const std::string& dir) {
    std::string newDir = dir;
    const char sep = '/';
    if (!newDir.empty() && *newDir.rbegin() != sep) {
        newDir += sep;
    }
    return newDir;
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

void DiskCollector::SicGetDiskSerialId(const std::string& diskName, std::string& serialId) {
    std::vector<std::string> serialIdLines = {};
    std::string errorMessage;
    std::string PROCESS_SERIALID = "/sys/class/block/" + diskName + "/serial";
    // std::cout << "\nproc: " << std::string(PROCESS_SERIALID) << std::endl;
    if (!GetHostSystemStatWithPath(serialIdLines, errorMessage, PROCESS_SERIALID)) {
        LOG_WARNING(sLogger, ("failed to get serialId, proc:  ", PROCESS_SERIALID)("error msg", errorMessage));
        // return SIC_EXECUTABLE_FAILED;
    } else {
        serialId = serialIdLines[0];
        // std::cout << "\nserialId: " << std::string(serialId) << std::endl;
        // for (auto const& diskLine : serialIdLines) {
        //	std::cout << "\nserialLine: " << std::string(diskLine) << std::endl;
        // }
    }
    // return SIC_EXECUTABLE_FAILED;
}

} // namespace logtail
