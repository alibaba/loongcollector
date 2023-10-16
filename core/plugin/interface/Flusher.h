#pragma once

#include "plugin/interface/Plugin.h"
// #include "table/Table.h"
#include "json/json.h"

namespace logtail {
class Flusher : public Plugin {
public:
    virtual ~Flusher() = default;

    // virtual bool Init(const Table& config) = 0;
    virtual bool Init(const Json::Value& config) = 0;
    virtual bool Start() = 0;
    // 单例flusher: 只修改元信息
    // 独立flusher：停止线程
    virtual bool Stop(bool isPipelineRemoving) = 0;
};
} // namespace logtail
