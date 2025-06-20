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

#include <vector>

#include "host_monitor/Constants.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {

extern const uint32_t kHostMonitorMinInterval;
extern const uint32_t kHostMonitorDefaultInterval;

enum SicStatus {
    SIC_FAILED = -1,
    SIC_EXECUTABLE_FAILED = SIC_FAILED,

    SIC_SUCCESS = 0,
    SIC_EXECUTABLE_SUCCESS = SIC_SUCCESS,

    SIC_NOT_IMPLEMENT = 1,
};

enum SicFileSystemType {
    SIC_FILE_SYSTEM_TYPE_UNKNOWN = 0,
    SIC_FILE_SYSTEM_TYPE_NONE,
    SIC_FILE_SYSTEM_TYPE_LOCAL_DISK,
    SIC_FILE_SYSTEM_TYPE_NETWORK,
    SIC_FILE_SYSTEM_TYPE_RAM_DISK,
    SIC_FILE_SYSTEM_TYPE_CDROM,
    SIC_FILE_SYSTEM_TYPE_SWAP,
    SIC_FILE_SYSTEM_TYPE_MAX
};

struct DeviceMetric {
    double total = 0;
    double free = 0;
    double used = 0;
    double usePercent = 0;
    double avail = 0;
    double reads = 0;
    double writes = 0;
    double writeBytes = 0;
    double readBytes = 0;
    double inodePercent = 0;
    double avgqu_sz = 0;
    // Define the field descriptors
    static inline const FieldName<DeviceMetric> DeviceMetricFields[] = {
        FIELD_ENTRY(DeviceMetric, total),
        FIELD_ENTRY(DeviceMetric, free),
        FIELD_ENTRY(DeviceMetric, used),
        FIELD_ENTRY(DeviceMetric, usePercent),
        FIELD_ENTRY(DeviceMetric, avail),
        FIELD_ENTRY(DeviceMetric, reads),
        FIELD_ENTRY(DeviceMetric, writes),
        FIELD_ENTRY(DeviceMetric, writeBytes),
        FIELD_ENTRY(DeviceMetric, readBytes),
        FIELD_ENTRY(DeviceMetric, inodePercent),
        FIELD_ENTRY(DeviceMetric, avgqu_sz),
    };

    // Define the enumerate function for your metric type
    static void enumerate(const std::function<void(const FieldName<DeviceMetric, double>&)>& callback) {
        for (const auto& field : DeviceMetricFields) {
            callback(field);
        }
    }
};

struct DiskMetric {
    double reads = 0;
    double writes = 0;
    double writeBytes = 0;
    double readBytes = 0;
    double avgqu_sz = 0;
    double svctm = 0;
    double await = 0;
    double r_await = 0;
    double w_await = 0;
    double avgrq_sz = 0;
    double util = 0;
};

struct DeviceMountInfo {
    std::string devName;
    std::vector<std::string> mountPaths;
    std::string type;
};

struct DiskStat {
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t writeBytes = 0;
    uint64_t readBytes = 0;
    uint64_t rtime = 0;
    uint64_t wtime = 0;
    uint64_t qtime = 0;
    uint64_t time = 0;
    double service_time = 0;
    double queue = 0;
};

struct PartitionStat {
    double total = 0;
    double free = 0;
    double used = 0;
    double usePercent = 0;

    void setValueMap(std::map<std::string, double>& valueMap) const;
};

struct DiskCollectStat {
    PartitionStat space;
    PartitionStat inode;
    double spaceAvail = 0;
    DiskStat diskStat;
    DeviceMountInfo deviceMountInfo;
};
struct FileSystemInfo {
    std::string dirName;
    std::string devName;
    std::string type;
};

struct SicFileSystem {
    std::string dirName;
    std::string devName;
    std::string typeName;
    std::string sysTypeName;
    std::string options;
    SicFileSystemType type = SIC_FILE_SYSTEM_TYPE_UNKNOWN;
    unsigned long flags = 0;
};

struct SicDiskUsage {
    std::string dirName;
    std::string devName;

    uint64_t time = 0;
    uint64_t rTime = 0;
    uint64_t wTime = 0;
    uint64_t qTime = 0;
    uint64_t reads = 0;
    uint64_t writes = 0;
    uint64_t writeBytes = 0;
    uint64_t readBytes = 0;
    double snapTime = 0;
    double serviceTime = 0.0;
    double queue = 0.0;

    std::string string() const;
};

struct SicFileSystemUsage {
    SicDiskUsage disk;
    double use_percent = 0;
    // usage in KB
    uint64_t total = 0;
    uint64_t free = 0;
    uint64_t used = 0;
    uint64_t avail = 0;
    uint64_t files = 0;
    uint64_t freeFiles = 0;
};

struct SicIODev {
    std::string name; // devName
    bool isPartition = false;
    SicDiskUsage diskUsage{};
};

struct MetricData {
    std::map<std::string, double> valueMap;
    std::map<std::string, std::string> tagMap;

    // bool check(int index = -1) const;
    std::string metricName() const;

    // boost::json::object toJson() const;
};

class DiskCollector : public BaseCollector {
public:
    DiskCollector();
    int Init(int totalCount = kHostMonitorDefaultInterval / kHostMonitorMinInterval);
    ~DiskCollector() override = default;

    bool Collect(const HostMonitorTimerEvent::CollectConfig& collectConfig, PipelineEventGroup* group) override;

    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    static std::string FormatDir(const std::string& dir);

    // static void CalcDiskMetric(const DiskStat &current, const DiskStat &last, double interval, DiskMetric
    // &diskMetric);

    int GetDeviceMountMap(std::map<std::string, DeviceMountInfo>& mountMap);

    int GetDiskCollectStatMap(std::map<std::string, DiskCollectStat>& diskCollectStatMap);
    int GetFileSystemInfos(std::vector<FileSystemInfo>& fileSystemInfos);
    bool GetFileSystemListInformation(std::vector<SicFileSystem>& informations);
    bool SicGetFileSystemType(const std::string& fsTypeName, SicFileSystemType& fsType, std::string& fsTypeDisplayName);
    int GetFileSystemStat(const std::string& dirName, SicFileSystemUsage& sicFileSystemUsage);
    void GetDiskMetricData(const std::string& metricName,
                           const std::string& devName,
                           const std::string& diskSerialId,
                           double value,
                           const std::string& ns,
                           MetricData& metricData);
    std::string GetDiskName(const std::string& dev);
    int GetDiskStat(dev_t rDev, const std::string& dirName, SicDiskUsage& disk, SicDiskUsage& deviceUsage);
    int SicGetUpTime(double& uptime);
    int CalDiskUsage(SicIODev& ioDev, SicDiskUsage& diskUsage);
    int SicGetDiskUsage(SicDiskUsage& diskUsage, std::string dirName);
    int
    SicGetIOstat(std::string& dirName, SicDiskUsage& disk, std::shared_ptr<SicIODev>& ioDev, SicDiskUsage& deviceUsage);
    std::shared_ptr<SicIODev> SicGetIODev(std::string& dirName);
    void RefreshLocalDisk();
    void CalcDiskMetric(const DiskStat& current, const DiskStat& last, double interval, DiskMetric& diskMetric);
    void SicGetDiskSerialId(const std::string& devName, std::string& serialId);

private:
    std::map<std::string, DeviceMountInfo> mDeviceMountMap;
    std::chrono::steady_clock::time_point mDeviceMountMapExpireTime;
    std::map<std::string, DiskCollectStat> mCurrentDiskCollectStatMap;
    std::map<std::string, DiskCollectStat> mLastDiskCollectStatMap;
    // std::map<std::string, bool> mExcludeMountPathMap;
    // std::map<std::string, DiskMetric> mMetricCalculateMap;
    const std::string mModuleName;
    int mCountPerReport = 0;
    int mCount = 0;
    size_t maxDirSize = 1024;
    std::chrono::steady_clock::time_point mLastTime; // 上次获取磁盘信息的时间
    std::unordered_map<uint64_t, std::shared_ptr<SicIODev>> fileSystemCache;
    // const char* const  PROCESS_DIR = "/proc/";
    const char* const PROCESS_DISKSTATS = "/proc/diskstats";
    const char* const PROCESS_UPTIME = "/proc/uptime";
    std::map<std::string, MetricCalculate<DeviceMetric>> mDeviceCalMap;
    // MetricCalculate<DeviceMetric> mCalculate;
};

} // namespace logtail
