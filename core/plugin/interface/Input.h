#pragma once

#include "plugin/interface/Plugin.h"
// #include "table/Table.h"
#include "json/json.h"

namespace logtail {
class Input : public Plugin {
public:
    virtual ~Input() = default;

    // virtual bool Init(const Table& config) = 0;
    virtual bool Init(const Json::Value& config) = 0;
    virtual bool Start() = 0;
    virtual bool Stop(bool isPipelineRemoving) = 0;
};
} // namespace logtail
