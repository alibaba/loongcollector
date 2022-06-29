/*
 * Copyright 2022 iLogtail Authors
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

#ifndef __OAS_SHENNONG_BASE_FILTER_NODE_H__
#define __OAS_SHENNONG_BASE_FILTER_NODE_H__

#include <memory>
#include "log_pb/sls_logs.pb.h"
#include "common/LogGroupContext.h"

namespace logtail {

enum FilterOperator { NOT_OPERATOR, AND_OPERATOR, OR_OPERATOR };

enum FilterNodeType { VALUE_NODE, OPERATOR_NODE };

enum FilterNodeFunctionType { REGEX_FUNCTION };

class BaseFilterNode {
public:
    explicit BaseFilterNode(FilterNodeType nodeType) : nodeType(nodeType) {}
    virtual ~BaseFilterNode() {}

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context) { return true; }

public:
    FilterNodeType GetNodeType() const { return nodeType; }

private:
    FilterNodeType nodeType;
};

typedef std::shared_ptr<BaseFilterNode> BaseFilterNodePtr;

} // namespace logtail

#endif //__OAS_SHENNONG_BASE_FILTER_NODE_H__
