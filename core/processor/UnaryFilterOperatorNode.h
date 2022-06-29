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

#ifndef __OAS_SHENNONG_UNARY_FILTER_OPERATOR_NODE_H__
#define __OAS_SHENNONG_UNARY_FILTER_OPERATOR_NODE_H__

#include <boost/config.hpp>
#include "BaseFilterNode.h"

namespace logtail {

class UnaryFilterOperatorNode : public BaseFilterNode {
public:
    UnaryFilterOperatorNode(FilterOperator op, BaseFilterNodePtr child)
        : BaseFilterNode(OPERATOR_NODE), op(op), child(child) {}

    virtual ~UnaryFilterOperatorNode(){};

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context) {
        if (BOOST_LIKELY(child.get() != NULL)) {
            return !child->Match(log, context);
        }
        return false;
    }

private:
    FilterOperator op;
    BaseFilterNodePtr child;
};

} // namespace logtail

#endif //__OAS_SHENNONG_UNARY_FILTER_OPERATOR_NODE_H__
