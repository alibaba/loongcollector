/*
 * Copyright 2023 iLogtail Authors
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

#include "app_config/AppConfig.h"
#include "common/LogGroupContext.h"
#include "models/LogEvent.h"
#include "plugin/interface/Processor.h"

namespace logtail {

// BaseFilterNode
enum FilterOperator { NOT_OPERATOR, AND_OPERATOR, OR_OPERATOR };

enum FilterNodeType { VALUE_NODE, OPERATOR_NODE };

enum FilterNodeFunctionType { REGEX_FUNCTION };

class BaseFilterNode {
public:
    explicit BaseFilterNode(FilterNodeType nodeType) : nodeType(nodeType) {}
    virtual ~BaseFilterNode() {}

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context) { return true; }
    virtual bool Match(const LogContents& contents, const PipelineContext& mContext) { return true; }

public:
    FilterNodeType GetNodeType() const { return nodeType; }

private:
    FilterNodeType nodeType;
};

typedef std::shared_ptr<BaseFilterNode> BaseFilterNodePtr;

// BinaryFilterOperatorNode
class BinaryFilterOperatorNode : public BaseFilterNode {
public:
    BinaryFilterOperatorNode(FilterOperator op, BaseFilterNodePtr left, BaseFilterNodePtr right)
        : BaseFilterNode(OPERATOR_NODE), op(op), left(left), right(right) {}
    virtual ~BinaryFilterOperatorNode(){};

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context);
    virtual bool Match(const LogContents& contents, const PipelineContext& mContext);

private:
    FilterOperator op;
    BaseFilterNodePtr left;
    BaseFilterNodePtr right;
};

// RegexFilterValueNode
class RegexFilterValueNode : public BaseFilterNode {
public:
    RegexFilterValueNode(const std::string& key, const std::string& exp)
        : BaseFilterNode(VALUE_NODE), key(key), reg(exp) {}

    virtual ~RegexFilterValueNode() {}

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context);

    virtual bool Match(const LogContents& contents, const PipelineContext& mContext);

private:
    std::string key;
    boost::regex reg;
};

// UnaryFilterOperatorNode
class UnaryFilterOperatorNode : public BaseFilterNode {
public:
    UnaryFilterOperatorNode(BaseFilterNodePtr child)
        : BaseFilterNode(OPERATOR_NODE), child(child) {}

    virtual ~UnaryFilterOperatorNode(){};

public:
    virtual bool Match(const sls_logs::Log& log, const LogGroupContext& context);

    virtual bool Match(const LogContents& contents, const PipelineContext& mContext);

private:
    BaseFilterNodePtr child;
};

BaseFilterNodePtr ParseExpressionFromJSON(const Json::Value& value);
bool GetOperatorType(const std::string& type, FilterOperator& op);
bool GetNodeFuncType(const std::string& type, FilterNodeFunctionType& func);

class ProcessorFilterNative : public Processor {
public:
    static const std::string sName;

    const std::string& Name() const override { return sName; }
    bool Init(const Json::Value& config) override;
    void Process(PipelineEventGroup& logGroup) override;

    // Log field whitelist. The relationship between multiple conditions is "and". Only when all conditions are met, the
    // log will be collected.
    std::unordered_map<std::string, std::string> mInclude;
    BaseFilterNodePtr mConditionExp = nullptr;
    bool mDiscardingNonUTF8 = false;

protected:
    bool IsSupportedEvent(const PipelineEventPtr& e) const override;

private:
    enum class Mode { BYPASS_MODE, EXPRESSION_MODE, RULE_MODE };

    struct LogFilterRule {
        std::vector<std::string> FilterKeys;
        std::vector<boost::regex> FilterRegs;
    };

    bool ProcessEvent(PipelineEventPtr& e);

    // Filter logs through ConditionExp
    bool FilterExpressionRoot(LogEvent& sourceEvent, const BaseFilterNodePtr& node);

    // Filter logs through FilterRule
    bool FilterFilterRule(LogEvent& sourceEvent, const LogFilterRule* filterRule);
    bool IsMatched(const LogContents& contents, const LogFilterRule& rule);

    bool noneUtf8(StringView& strSrc, bool modify);
    bool CheckNoneUtf8(const StringView& strSrc);
    void FilterNoneUtf8(std::string& strSrc);

    Mode mFilterMode = Mode::BYPASS_MODE;

    std::shared_ptr<LogFilterRule> mFilterRule;

    CounterPtr mProcFilterErrorTotal;
    CounterPtr mProcFilterRecordsTotal;

#ifdef APSARA_UNIT_TEST_MAIN
    friend class ProcessorFilterNativeUnittest;
#endif
};

} // namespace logtail
