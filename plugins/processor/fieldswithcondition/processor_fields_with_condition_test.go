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
	"encoding/json"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func init() {
	logger.InitTestLogger(logger.OptionOpenMemoryReceiver)
}

func newProcessor() (*ProcessorFieldsWithCondition, error) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"contains",
					"FieldConditions":{
						"content":"Out of memory"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":false,
						"Fields":{
							"eventcode":"c1",
							"cid":"c-1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					}
				]
			},
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"contains",
					"FieldConditions":{
						"content":"BIOS-provided"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":false,
						"Fields":{
							"eventcode":"c2",
							"cid":"c-2",
							"test2.1":"test2.11",
							"test2.2":"test2.22"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test2.1",
							"test2.2"
						]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	return &processor, err
}

// Success Case
func TestSuccessCase(t *testing.T) {
	processor, err := newProcessor()
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	value1 := "time:2017.09.12 20:55:36" +
		"\\tOut of memory: Kill process "
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})

	log2 := &protocol.Log{Time: 0}
	value2 := "time:2017.09.12 20:55:36" +
		"\\tBIOS-provided physical RAM map "
	log2.Contents = append(log2.Contents, &protocol.Log_Content{Key: "content", Value: value2})
	processor.ProcessLogs([]*protocol.Log{log1, log2})

	assert.Equal(t, 3, len(log1.Contents))
	assert.Equal(t, 3, len(log2.Contents))
	for idx := range log1.Contents {
		key := log1.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c1", log1.Contents[idx].Value)
		} else if key == "cid" {
			assert.Equal(t, "c-1", log1.Contents[idx].Value)
		}
	}
	for idx := range log2.Contents {
		key := log2.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c2", log2.Contents[idx].Value)
		} else if key == "cid" {
			assert.Equal(t, "c-2", log2.Contents[idx].Value)
		}
	}
}

// Relation value error，change value to equals
func TestRelationOperatorFail(t *testing.T) {
	logger.ClearMemoryLog()
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"test123",
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":false,
						"Fields":{
							"eventcode":"c1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	memoryLog, ok := logger.ReadMemoryLog(1)
	assert.True(t, ok)
	assert.Equal(t, 1, logger.GetMemoryLogCount())
	assert.True(t, strings.Contains(memoryLog, "AlarmType:CONDITION_INIT_ALARM\tinit relationOpertor error"))
	log1 := &protocol.Log{Time: 0}
	value1 := "dummy"
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})
	processor.ProcessLogs([]*protocol.Log{log1})
	assert.Equal(t, 2, len(log1.Contents))
	for idx := range log1.Contents {
		key := log1.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c1", log1.Contents[idx].Value)
		}
	}
}

// Test all relations (equals/startwith/regexp/contains)
func TestAllRelationsCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"RelationOperator":"regexp",
					"FieldConditions":{
						"content": "^dummy test1"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{
							"eventcode":"c1"
						}
					}
				]
			},
			{
				"Case":{
					"RelationOperator":"contains",
					"FieldConditions":{
						"content": "dummy test2"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{
							"eventcode":"c2"
						}
					}
				]
			},
			{
				"Case":{
					"RelationOperator":"startwith",
					"FieldConditions":{
						"content": "dummy test3"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{
							"eventcode":"c3"
						}
					}
				]
			},
			{
				"Case":{
					"RelationOperator":"equals",
					"FieldConditions":{
						"content": "dummy test4"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{
							"eventcode":"c4"
						}
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	value1 := "dummy test1 regexp end"
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})

	log2 := &protocol.Log{Time: 0}
	value2 := "contains start dummy test2 contains end"
	log2.Contents = append(log2.Contents, &protocol.Log_Content{Key: "content", Value: value2})

	log3 := &protocol.Log{Time: 0}
	value3 := "dummy test3 startwith end"
	log3.Contents = append(log3.Contents, &protocol.Log_Content{Key: "content", Value: value3})

	log4 := &protocol.Log{Time: 0}
	value4 := "dummy test4"
	log4.Contents = append(log4.Contents, &protocol.Log_Content{Key: "content", Value: value4})
	processor.ProcessLogs([]*protocol.Log{log1, log2, log3, log4})

	assert.Equal(t, 2, len(log1.Contents))
	assert.Equal(t, 2, len(log2.Contents))
	assert.Equal(t, 2, len(log3.Contents))
	assert.Equal(t, 2, len(log4.Contents))

	for idx := range log1.Contents {
		key := log1.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c1", log1.Contents[idx].Value)
		}
	}
	for idx := range log2.Contents {
		key := log2.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c2", log2.Contents[idx].Value)
		}
	}
	for idx := range log3.Contents {
		key := log3.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c3", log3.Contents[idx].Value)
		}
	}
	for idx := range log4.Contents {
		key := log4.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "c4", log4.Contents[idx].Value)
		}
	}
}

// Test DropIfNotMatchCondition functionality
func TestNoMatchCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"equals",
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":true,
						"Fields":{
							"eventcode":"c1",
							"cid":"c-1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	value1 := "time:2017.09.12 20:55:36" +
		"\\tOut of memory: Kill process "
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})
	logArray := processor.ProcessLogs([]*protocol.Log{log1})
	// Drop
	assert.Equal(t, 0, len(logArray))

	jsonStr = `{
		"DropIfNotMatchCondition":false,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"equals",
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":true,
						"Fields":{
							"eventcode":"c1",
							"cid":"c-1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					}
				]
			}
		]
	}
	`
	json.Unmarshal([]byte(jsonStr), &processor)
	err = processor.Init(ctx)
	require.NoError(t, err)
	logArray = processor.ProcessLogs([]*protocol.Log{log1})
	// Retain
	assert.Equal(t, 1, len(logArray))
}

// Test default value
func TestOptinalDefaultCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"Switch":[
			{
				"Case":{
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{
							"eventcode":"c1"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	assert.False(t, processor.DropIfNotMatchCondition)
	for i := range processor.Switch {
		// Set default values
		relationOpertor := processor.Switch[i].Case.RelationOperator
		logicalOpertor := processor.Switch[i].Case.LogicalOperator
		fieldConditions := processor.Switch[i].Case.FieldConditions
		assert.Equal(t, RelationOperatorEquals, relationOpertor)
		assert.Equal(t, LogicalOperatorAnd, logicalOpertor)
		assert.Equal(t, 1, len(fieldConditions))

		actionAdd := processor.Switch[i].actions[0]
		actionDrop := processor.Switch[i].actions[1]
		assert.False(t, actionAdd.IgnoreIfExist)
		assert.Equal(t, 1, len(actionAdd.Fields))
		assert.Equal(t, 0, len(actionDrop.dropkeyDictionary))
	}
}

// Type input error, Init error
func TestActionTypeErrorCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"Switch":[
			{
				"Case":{
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"test_error",
						"Fields":{
							"eventcode":"c1"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)

	assert.Condition(t, func() (success bool) {
		return len(err.Error()) > 0
	})
}

// No field in action, case run with nothing
func TestActionNoFieldsCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"Switch":[
			{
				"Case":{
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"Fields":{}
					},
					{
						"type":"processor_drop",
						"DropKeys":[]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	value1 := "dummy"
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})
	processor.ProcessLogs([]*protocol.Log{log1})
	assert.Equal(t, 1, len(log1.Contents))
}

// Test actions process order
func TestActionProcessOrderCase(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"equals",
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":true,
						"Fields":{
							"eventcode":"c1",
							"cid":"c-1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	value1 := "dummy"
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: value1})
	processor.ProcessLogs([]*protocol.Log{log1})
	assert.Equal(t, 3, len(log1.Contents))

	jsonStr = `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Case":{
					"LogicalOperator":"and",
					"RelationOperator":"equals",
					"FieldConditions":{
						"content": "dummy"
					}
				},
				"Actions":[
					{
						"type":"processor_drop",
						"DropKeys":[
							"test1.1",
							"test1.2"
						]
					},
					{
						"type":"processor_add_fields",
						"IgnoreIfExist":true,
						"Fields":{
							"eventcode":"c1",
							"cid":"c-1",
							"test1.1":"test1.1",
							"test1.2":"test1.2"
						}
					}
				]
			}
		]
	}
	`
	json.Unmarshal([]byte(jsonStr), &processor)
	err = processor.Init(ctx)
	require.NoError(t, err)
	log2 := &protocol.Log{Time: 0}
	value2 := "dummy"
	log2.Contents = append(log2.Contents, &protocol.Log_Content{Key: "content", Value: value2})
	processor.ProcessLogs([]*protocol.Log{log2})
	assert.Equal(t, 5, len(log2.Contents))
}

// Test multiple case1
func TestMulti1Case(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition": true,
		"Switch":
		[
			{
				"Case":
				{
					"FieldConditions":
					{
						"seq": "10",
						"d": "10"
					}
				},
				"Actions":
				[
					{
						"type": "processor_add_fields",
						"IgnoreIfExist": false,
						"Fields":
						{
							"eventCode": "event_00001",
							"name": "error_oom"
						}
					},
					{
						"type": "processor_drop",
						"DropKeys":
						[
							"c"
						]
					}
				]
			},
			{
				"Case":
				{
					"FieldConditions":
					{
						"seq": "20",
						"d": "10"
					}
				},
				"Actions":
				[
					{
						"type": "processor_add_fields",
						"IgnoreIfExist": false,
						"Fields":
						{
							"eventCode": "event_00002",
							"name": "error_oom2"
						}
					}
				]
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: `{"t1": "2022-05-06 12:50:08.571218122", "t2": "2022-05-06 12:50:08", "a":"b","c":2,"d":10, "seq": 20}`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "t1", Value: `2022-05-06 12:50:08.571218122`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "t2", Value: `2022-05-06 12:50:08`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "a", Value: `b`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "c", Value: `2`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "d", Value: `10`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "seq", Value: `20`})
	processor.ProcessLogs([]*protocol.Log{log1})
	assert.Equal(t, 9, len(log1.Contents))
	for idx := range log1.Contents {
		key := log1.Contents[idx].Key
		if key == "eventcode" {
			assert.Equal(t, "event_00002", log1.Contents[idx].Value)
		} else if key == "name" {
			assert.Equal(t, "error_oom2", log1.Contents[idx].Value)
		}
	}
}

// Test multiple case2
func TestMulti2Case(t *testing.T) {
	ctx := mock.NewEmptyContext("p", "l", "c")
	jsonStr := `{
		"DropIfNotMatchCondition":true,
		"Switch":[
			{
				"Actions":[
					{
						"Fields":{
							"eventCode":"event_00001",
							"name":"error_oom"
						},
						"IgnoreIfExist":false,
						"type":"processor_add_fields"
					},
					{
						"type":"processor_drop",
						"DropKeys":[
							"c"
						]
					}
				],
				"Case":{
					"RelationOperator":"regexp",
					"FieldConditions":{
						"d":"^10.*",
						"seq":"10"
					},
					"LogicalOperator":"and"
				}
			},
			{
				"Actions":[
					{
						"Fields":{
							"eventCode":"event_00002",
							"name":"error_oom2"
						},
						"IgnoreIfExist":false,
						"type":"processor_add_fields"
					}
				],
				"Case":{
					"RelationOperator":"regexp",
					"LogicalOperator":"or",
					"FieldConditions":{
						"d":"^2.*",
						"seq":"20"
					}
				}
			}
		]
	}
	`
	var processor ProcessorFieldsWithCondition
	json.Unmarshal([]byte(jsonStr), &processor)
	err := processor.Init(ctx)
	require.NoError(t, err)
	log1 := &protocol.Log{Time: 0}
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "content", Value: `{"t1": "2022-05-06 12:50:08.571218122", "t2": "2022-05-06 12:50:08", "a":"b","c":2,"d":1, "seq": 21}`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "t1", Value: `2022-05-06 12:50:08.571218122`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "t2", Value: `2022-05-06 12:50:08`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "a", Value: `b`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "c", Value: `2`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "d", Value: `1`})
	log1.Contents = append(log1.Contents, &protocol.Log_Content{Key: "seq", Value: `21`})
	processor.ProcessLogs([]*protocol.Log{log1})
	assert.Equal(t, 7, len(log1.Contents))
}
