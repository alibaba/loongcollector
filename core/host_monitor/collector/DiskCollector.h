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

#include "HostMonitorTimerEvent.h"
#include "host_monitor/Constants.h"
#include "host_monitor/HostMonitorContext.h"
#include "host_monitor/SystemInterface.h"
#include "host_monitor/collector/BaseCollector.h"
#include "host_monitor/collector/MetricCalculate.h"
#include "plugin/input/InputHostMonitor.h"

namespace logtail {
#ifndef HZ
#define HZ 100
#endif

extern const uint32_t kHostMonitorMinInterval;
extern const uint32_t kHostMonitorDefaultInterval;

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

struct DiskUsage {
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

struct FileSystemUsage {
    DiskUsage disk;
    double use_percent = 0;
    // usage in KB
    uint64_t total = 0;
    uint64_t free = 0;
    uint64_t used = 0;
    uint64_t avail = 0;
    uint64_t files = 0;
    uint64_t freeFiles = 0;
};

struct IODev {
    std::string name; // devName
    bool isPartition = false;
    DiskUsage diskUsage{};
};

struct MetricData {
    std::map<std::string, double> valueMap;
    std::map<std::string, std::string> tagMap;
    std::string metricName() const;
};

class DiskCollector : public BaseCollector {
public:
    DiskCollector() = default;
    ~DiskCollector() override = default;

    bool Init(HostMonitorContext& collectContext) override;
    bool Collect(HostMonitorContext& collectContext, PipelineEventGroup* groupPtr) override;
    [[nodiscard]] const std::chrono::seconds GetCollectInterval() const override;
    static const std::string sName;
    const std::string& Name() const override { return sName; }

private:
    int GetDeviceMountMap(const CollectTime& collectTime, std::map<std::string, DeviceMountInfo>& mountMap);
    int GetDiskCollectStatMap(const CollectTime& collectTime,
                              std::map<std::string, DiskCollectStat>& diskCollectStatMap);
    int GetFileSystemInfos(const CollectTime& collectTime, std::vector<FileSystemInfo>& fileSystemInfos);
    int GetFileSystemStat(const CollectTime& collectTime, const std::string& dirName, FileSystemUsage& fileSystemUsage);
    std::string GetDiskName(const std::string& dev);
    int GetDiskStat(const CollectTime& collectTime, dev_t rDev, DiskUsage& disk, DiskUsage& deviceUsage);
    int CalDiskUsage(const CollectTime& collectTime, IODev& ioDev, DiskUsage& diskUsage);
    int GetDiskUsage(const CollectTime& collectTime, DiskUsage& diskUsage, std::string dirName);
    int GetIOstat(const CollectTime& collectTime,
                  std::string& dirName,
                  DiskUsage& disk,
                  std::shared_ptr<IODev>& ioDev,
                  DiskUsage& deviceUsage);
    std::shared_ptr<IODev> GetIODev(const CollectTime& collectTime, std::string& dirName);
    void RefreshLocalDisk(const CollectTime& collectTime);
    void CalcDiskMetric(const DiskStat& current, const DiskStat& last, double interval, DiskMetric& diskMetric);

private:
    std::map<std::string, DeviceMountInfo> mDeviceMountMap;
    std::chrono::steady_clock::time_point mDeviceMountMapExpireTime;
    std::map<std::string, DiskCollectStat> mCurrentDiskCollectStatMap;
    std::map<std::string, DiskCollectStat> mLastDiskCollectStatMap;
    const std::string mModuleName;
    static const size_t kMaxDirSize = 1024;
    std::chrono::steady_clock::time_point mLastTime; // 上次获取磁盘信息的时间
    std::unordered_map<uint64_t, std::shared_ptr<IODev>> fileSystemCache;
    std::map<std::string, MetricCalculate<DeviceMetric>> mDeviceCalMap;
};

} // namespace logtail
