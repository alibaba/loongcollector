// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package fieldswithcondition

import (
	"fmt"
	"regexp"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

const (
	PluginName = "processor_fields_with_condition"

	RelationOperatorEquals    = "equals"
	RelationOperatorRegexp    = "regexp"
	RelationOperatorContains  = "contains"
	RelationOperatorStartwith = "startwith"

	LogicalOperatorAnd = "and"
	LogicalOperatorOr  = "or"

	ActionAddFieldsType = "processor_add_fields"
	ActionDropType      = "processor_drop"
)

type ProcessorFieldsWithCondition struct {
	DropIfNotMatchCondition bool        `comment:"Optional. When the case condition is not met, whether the log is discarded (true) or retained (false)"`
	Switch                  []Condition `comment:"The switch-case conditions"`

	filterMetric selfmonitor.CounterMetric
	context      pipeline.Context
}

type Condition struct {
	Case    ConditionCase            `comment:"The condition that log data satisfies"`
	Actions []map[string]interface{} `comment:"The action that executes when the case condition is met"`

	actions []ConditionAction
}

type FieldApply func(reg *regexp.Regexp, conditionContent string, logContent string) bool

type Field struct {
	apply            FieldApply
	reg              *regexp.Regexp
	conditionContent string
}

type ConditionCase struct {
	LogicalOperator  string            `comment:"Optional. The Logical operators between multiple conditional fields, alternate values are and/or"`
	RelationOperator string            `comment:"Optional. The Relational operators for conditional fields, alternate values are equals/regexp/contains/startwith"`
	FieldConditions  map[string]string `comment:"The key-value pair of field names and expressions"`

	fieldConditionFields map[string]Field
}

type ConditionAction struct {
	Type          string                 `comment:"action type. alternate values are processor_add_fields/processor_drop"`
	IgnoreIfExist bool                   `comment:"Optional. Whether to ignore when the same key exists"`
	Fields        map[string]interface{} `comment:"The appending fields"`
	DropKeys      []interface{}          `comment:"The dropping fields"`

	dropkeyDictionary map[string]interface{}
}

// Init called for init some system resources, like socket, mutex...
func (p *ProcessorFieldsWithCondition) Init(context pipeline.Context) error {
	p.context = context
	for i := range p.Switch {
		// set default values
		relationOpertor := p.Switch[i].Case.RelationOperator
		switch relationOpertor {
		case RelationOperatorEquals:
		case RelationOperatorRegexp:
		case RelationOperatorContains:
		case RelationOperatorStartwith:
		default:
			if len(relationOpertor) > 0 {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ConditionInitAlarm, "init relationOpertor error, relationOpertor", relationOpertor)
			}
			relationOpertor = RelationOperatorEquals
			p.Switch[i].Case.RelationOperator = relationOpertor
		}
		logicalOperator := p.Switch[i].Case.LogicalOperator
		switch logicalOperator {
		case LogicalOperatorAnd:
		case LogicalOperatorOr:
		default:
			if len(logicalOperator) > 0 {
				logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ConditionInitAlarm, "init logicalOperator error, logicalOperator", logicalOperator)
			}
			p.Switch[i].Case.LogicalOperator = LogicalOperatorAnd
		}

		if p.Switch[i].Case.FieldConditions != nil {
			p.Switch[i].Case.fieldConditionFields = make(map[string]Field)
			for key, val := range p.Switch[i].Case.FieldConditions {
				switch relationOpertor {
				case RelationOperatorRegexp:
					reg, err := regexp.Compile(val)
					if err != nil {
						logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ConditionInitAlarm, "init condition regex error, key", key, "regex", val, "error", err)
						return err
					}
					p.Switch[i].Case.fieldConditionFields[key] = Field{
						reg:              reg,
						conditionContent: val,
						apply: func(reg *regexp.Regexp, conditionContent string, logContent string) bool {
							return reg.MatchString(logContent)
						},
					}
				case RelationOperatorContains:
					p.Switch[i].Case.fieldConditionFields[key] = Field{
						reg:              nil,
						conditionContent: val,
						apply: func(reg *regexp.Regexp, conditionContent string, logContent string) bool {
							return strings.Contains(logContent, conditionContent)
						},
					}
				case RelationOperatorStartwith:
					p.Switch[i].Case.fieldConditionFields[key] = Field{
						reg:              nil,
						conditionContent: val,
						apply: func(reg *regexp.Regexp, conditionContent string, logContent string) bool {
							return strings.HasPrefix(logContent, conditionContent)
						},
					}
				default:
					p.Switch[i].Case.fieldConditionFields[key] = Field{
						reg:              nil,
						conditionContent: val,
						apply: func(reg *regexp.Regexp, conditionContent string, logContent string) bool {
							return logContent == conditionContent
						},
					}
				}
			}
		}

		if p.Switch[i].Actions != nil {
			p.Switch[i].actions = make([]ConditionAction, len(p.Switch[i].Actions))
			for j := range p.Switch[i].Actions {
				action := p.Switch[i].Actions[j]
				if typeName, ok := action["type"]; ok {
					switch typeName {
					case ActionAddFieldsType:
						IgnoreIfExist := false
						if ignoreIfExist, ok := action["IgnoreIfExist"]; ok {
							if dropKeysArrInner, ok := ignoreIfExist.(bool); ok {
								IgnoreIfExist = dropKeysArrInner
							}
						}
						AddFields := make(map[string]interface{})
						if addFields, ok := action["Fields"]; ok {
							if addFieldsInner, ok := addFields.(map[string]interface{}); ok {
								AddFields = addFieldsInner
							}
						}
						p.Switch[i].actions[j] = ConditionAction{
							Type:          ActionAddFieldsType,
							IgnoreIfExist: IgnoreIfExist,
							Fields:        AddFields,
						}
					case ActionDropType:
						if dropKeys, ok := action["DropKeys"]; ok {
							if dropKeysArr, ok := dropKeys.([]interface{}); ok {
								dropkeyDictionary := make(map[string]interface{})
								for _, dropKey := range dropKeysArr {
									dropkeyDictionary[dropKey.(string)] = true
								}
								p.Switch[i].actions[j] = ConditionAction{
									Type:              ActionDropType,
									DropKeys:          dropKeysArr,
									dropkeyDictionary: dropkeyDictionary,
								}
							}
						}
					default:
						logger.Warning(p.context.GetRuntimeContext(), selfmonitor.ConditionInitAlarm, "init condition action type error, type", typeName)
						return fmt.Errorf("init condition action type error,type is %v", typeName)
					}
				}

			}
		}
	}
	metricsRecord := p.context.GetMetricRecord()
	p.filterMetric = selfmonitor.NewCounterMetricAndRegister(metricsRecord, selfmonitor.MetricPluginDiscardedEventsTotal)
	return nil
}

func (*ProcessorFieldsWithCondition) Description() string {
	return "Processor to match multiple conditions, and if one of the conditions is met, the corresponding action is performed."
}

func init() {
	pipeline.Processors[PluginName] = func() pipeline.Processor {
		return &ProcessorFieldsWithCondition{}
	}
}

// Process applies the first matching case's add and drop actions to each log.
// When DropIfNotMatchCondition is set, logs without a match are dropped.
func (p *ProcessorFieldsWithCondition) Process(in *models.PipelineGroupEvents, context pipeline.PipelineContext) {
	if in == nil {
		return
	}
	events := make([]models.PipelineEvent, 0, len(in.Events))
	for _, event := range in.Events {
		if event.GetType() != models.EventTypeLogging {
			// Metric/Span events pass through unchanged.
			events = append(events, event)
			continue
		}
		// Keep unmatched logs only when configured.
		if p.processLogEvent(event.(*models.Log)) || !p.DropIfNotMatchCondition {
			events = append(events, event)
		} else {
			p.filterMetric.Add(1)
		}
	}
	context.Collector().Collect(in.Group, events...)
}

// processLogEvent applies the first matching case's actions and reports whether a case matched.
func (p *ProcessorFieldsWithCondition) processLogEvent(log *models.Log) bool {
	return p.matchAndProcessEvent(log.GetIndices())
}

// Match single case against log fields.
func (p *ProcessorFieldsWithCondition) isCaseMatchEvent(contents models.LogContents, conditionCase ConditionCase) bool {
	if conditionCase.fieldConditionFields != nil {
		matchedCount := 0
		for key, field := range conditionCase.fieldConditionFields {
			if !contents.Contains(key) {
				continue
			}
			logContent := pipeline.GetStringValue(contents.Get(key))
			if field.apply(field.reg, field.conditionContent, logContent) {
				matchedCount++
			}
		}
		if conditionCase.LogicalOperator == LogicalOperatorAnd && matchedCount == len(conditionCase.fieldConditionFields) {
			return true
		}
		if conditionCase.LogicalOperator == LogicalOperatorOr && matchedCount > 0 {
			return true
		}
	}
	return false
}

// Process single action against log fields.
func (p *ProcessorFieldsWithCondition) processActionEvent(contents models.LogContents, conditionAction ConditionAction) {
	if conditionAction.Type == ActionAddFieldsType {
		for k, v := range conditionAction.Fields {
			if conditionAction.IgnoreIfExist && contents.Contains(k) {
				continue
			}
			contents.Add(k, v.(string))
		}
	} else if conditionAction.Type == ActionDropType {
		for key := range conditionAction.dropkeyDictionary {
			contents.Delete(key)
		}
	}
}

// Match different cases against log fields.
func (p *ProcessorFieldsWithCondition) matchAndProcessEvent(contents models.LogContents) bool {
	if p.Switch != nil {
		for _, condition := range p.Switch {
			if p.isCaseMatchEvent(contents, condition.Case) {
				for i := range condition.actions {
					p.processActionEvent(contents, condition.actions[i])
				}
				return true
			}
		}
	}
	return false
}
