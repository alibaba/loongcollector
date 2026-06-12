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

#include "host_monitor/collector/FsStatCollector.h"

#include <mntent.h>
#include <sys/statvfs.h>

#include <filesystem>
#include <set>

#include "MetricValue.h"
#include "common/Flags.h"
#include "host_monitor/Constants.h"
#include "logger/Logger.h"

DEFINE_FLAG_INT32(basic_host_monitor_fsstat_collect_interval, "basic host monitor fsstat collect interval, seconds", 5);

namespace logtail {

const std::string FsStatCollector::sName = "fsstat";

bool FsStatCollector::Init(HostMonitorContext& collectContext) {
    return BaseCollector::Init(collectContext);
}

const std::chrono::seconds FsStatCollector::GetCollectInterval() const {
    return std::chrono::seconds(INT32_FLAG(basic_host_monitor_fsstat_collect_interval));
}

bool FsStatCollector::ReadMountPoints(std::vector<FsStatInfo>& fsStats) {
    // /proc/mounts or /etc/mtab
    auto mountedDir = ETC_DIR / ETC_MTAB;
    FILE* fp = setmntent(mountedDir.c_str(), "r");
    if (!fp) {
        LOG_ERROR(sLogger, ("fsstat collector", "failed to open mount file")("path", mountedDir.string()));
        return false;
    }

    std::set<std::string> seenDevices;
    bool hasRoot = false;

    mntent ent{};
    std::vector<char> buffer(4096);
    while (getmntent_r(fp, &ent, buffer.data(), buffer.size())) {
        // Only consider /dev/ block devices
        std::string fsName(ent.mnt_fsname);
        if (fsName.size() < 5 || fsName.substr(0, 5) != "/dev/") {
            continue;
        }

        // Deduplicate: only keep the first mount point for each device
        if (seenDevices.count(fsName)) {
            continue;
        }
        seenDevices.insert(fsName);

        std::string mountDir(ent.mnt_dir);
        if (mountDir == "/") {
            hasRoot = true;
        }

        FsStatInfo info;
        info.device = fsName;
        info.mountPoint = mountDir;

        struct statvfs vfsBuf {};
        if (statvfs(mountDir.c_str(), &vfsBuf) != 0) {
            LOG_WARNING(sLogger, ("fsstat collector", "statvfs failed")("mount", mountDir));
            continue;
        }

        info.bsize = vfsBuf.f_bsize;
        info.blocks = vfsBuf.f_blocks;
        info.bfree = vfsBuf.f_bfree;
        info.bavail = vfsBuf.f_bavail;
        info.files = vfsBuf.f_files;
        info.ffree = vfsBuf.f_ffree;

        fsStats.push_back(std::move(info));
    }

    endmntent(fp);

    // If / is not in the list (e.g. overlay root in container), force add it
    if (!hasRoot) {
        struct statvfs vfsBuf {};
        if (statvfs("/", &vfsBuf) == 0) {
            FsStatInfo info;
            info.device = "root";
            info.mountPoint = "/";
            info.bsize = vfsBuf.f_bsize;
            info.blocks = vfsBuf.f_blocks;
            info.bfree = vfsBuf.f_bfree;
            info.bavail = vfsBuf.f_bavail;
            info.files = vfsBuf.f_files;
            info.ffree = vfsBuf.f_ffree;
            fsStats.push_back(std::move(info));
        } else {
            LOG_WARNING(sLogger, ("fsstat collector", "statvfs on / failed, skipping root"));
        }
    }

    return !fsStats.empty();
}

bool FsStatCollector::Collect(HostMonitorContext& collectContext, PipelineEventGroup* group) {
    if (group == nullptr) {
        return true;
    }

    std::vector<FsStatInfo> fsStats;
    if (!ReadMountPoints(fsStats)) {
        LOG_ERROR(sLogger, ("fsstat collector", "ReadMountPoints failed"));
        return false;
    }

    const time_t now = time(nullptr);

    for (const auto& info : fsStats) {
        MetricEvent* metricEvent = group->AddMetricEvent(true);
        if (!metricEvent) {
            LOG_ERROR(sLogger, ("fsstat collector", "metricEvent is nullptr"));
            return false;
        }
        metricEvent->SetTimestamp(now, 0);
        metricEvent->SetValue<UntypedMultiDoubleValues>(metricEvent);
        auto* multiDoubleValues = metricEvent->MutableValue<UntypedMultiDoubleValues>();

        multiDoubleValues->SetValue(
            std::string("f_bsize"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.bsize)});
        multiDoubleValues->SetValue(
            std::string("f_blocks"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.blocks)});
        multiDoubleValues->SetValue(
            std::string("f_bfree"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.bfree)});
        multiDoubleValues->SetValue(
            std::string("f_bavail"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.bavail)});
        multiDoubleValues->SetValue(
            std::string("f_files"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.files)});
        multiDoubleValues->SetValue(
            std::string("f_ffree"),
            UntypedMultiDoubleValue{UntypedValueMetricType::MetricTypeGauge, static_cast<double>(info.ffree)});

        metricEvent->SetTag(std::string("device"), info.device);
        metricEvent->SetTag(std::string("m"), std::string("system.fsstat"));
    }

    return true;
}

} // namespace logtail
