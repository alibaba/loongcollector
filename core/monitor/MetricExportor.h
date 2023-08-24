#pragma once

#include "profile_sender/ProfileSender.h"


namespace logtail {

class MetricExportor {
public:
    static MetricExportor* GetInstance() {
        static MetricExportor* ptr = new MetricExportor();
        return ptr;
    }
    void PushMetrics(bool forceSend);

private:
    MetricExportor();
    ProfileSender mProfileSender;
    int32_t mSendInterval;
    int32_t mLastSendTime;
};
}