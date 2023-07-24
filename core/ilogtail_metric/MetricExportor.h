#pragma once

#include <string>
#include <mutex>
#include <unordered_map>
#include <map>
#include <list>
#include <json/json.h>
#include "profile_sender/ProfileSender.h"
#include "ILogtailMetric.h"
#include "logger/Logger.h"
#include "plugin/LogtailPlugin.h"
#include "ProfileSender.h"
#include "log_pb/sls_logs.pb.h"

namespace logtail {

class MetricExportor {
public:
    static MetricExportor* GetInstance() {
        static MetricExportor* ptr = new MetricExportor();
        return ptr;
    }
    void PushMetrics();

private:
    MetricExportor();
    ~MetricExportor() {}

    ProfileSender mProfileSender;
    int32_t mSendInterval;
    int32_t mLastSendTime;
    int32_t mSnapshotInterval;
    int32_t mLastSnapshotTime;
};
}