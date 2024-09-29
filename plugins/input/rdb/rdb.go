// Copyright 2022 iLogtail Authors
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

package rdb

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

type RdbFunc func() error //nolint:revive

type ColumnResolverFunc func(string) (string, error)

type CheckPoint struct {
	CheckPointColumn     string
	CheckPointColumnType string
	Value                string
	LastUpdateTime       time.Time
}

type Rdb struct {
	ColumnsHash           map[string]string
	ConnectionRetryTime   int
	ConnectionRetryWaitMs int
	Driver                string
	Address               string
	Port                  int
	DataBase              string
	User                  string
	Password              string
	PasswordPath          string
	DialTimeOutMs         int
	ReadTimeOutMs         int
	Limit                 bool
	PageSize              int
	MaxSyncSize           int
	StateMent             string
	StateMentPath         string
	CheckPoint            bool
	CheckPointColumn      string
	// int or time
	CheckPointColumnType  string
	CheckPointStart       string
	CheckPointSavePerPage bool
	IntervalMs            int

	// inner params
	checkpointColumnIndex int
	columnsKeyBuffer      []string
	checkpointValue       string
	dbInstance            *sql.DB
	dbStatment            *sql.Stmt
	columnValues          []sql.NullString
	columnTypes           []*sql.ColumnType
	columnStringValues    []string
	columnValuePointers   []interface{}
	Shutdown              chan struct{}
	waitGroup             sync.WaitGroup
	Context               pipeline.Context
	collectLatency        pipeline.LatencyMetric
	collectTotal          pipeline.CounterMetric
	checkpointMetric      pipeline.StringMetric
}

func (m *Rdb) Init(context pipeline.Context, rdbFunc RdbFunc) (int, error) {
	initAlarmName := fmt.Sprintf("%s_INIT_ALARM", strings.ToUpper(m.Driver))
	m.Context = context
	if len(m.StateMent) == 0 && len(m.StateMentPath) != 0 {
		data, err := os.ReadFile(m.StateMentPath)
		if err != nil && len(data) > 0 {
			m.StateMent = string(data)
		} else {
			logger.Warning(m.Context.GetRuntimeContext(), initAlarmName, "init sql statement error", err)
		}
	}
	if len(m.StateMent) == 0 {
		return 0, fmt.Errorf("no sql statement")
	}
	err := rdbFunc()
	if err != nil {
		logger.Warning(m.Context.GetRuntimeContext(), initAlarmName, "init rdbFunc error", err)
	}

	metricsRecord := m.Context.GetMetricRecord()

	m.collectLatency = helper.NewLatencyMetricAndRegister(metricsRecord, helper.MetricPluginCollectAvgCostTimeMs)
	m.collectTotal = helper.NewCounterMetricAndRegister(metricsRecord, helper.MetricPluginCollectTotal)
	if m.CheckPoint {
		m.checkpointMetric = helper.NewStringMetricAndRegister(metricsRecord, fmt.Sprintf("%s_checkpoint", m.Driver))
	}
	return 10000, nil
}

func (m *Rdb) initRdbsql(connStr string, rdbFunc RdbFunc) error {
	initAlarmName := fmt.Sprintf("%s_INIT_ALARM", strings.ToUpper(m.Driver))
	err := rdbFunc()
	if err != nil {
		logger.Error(m.Context.GetRuntimeContext(), initAlarmName, "rdb func ", err)
		return err
	}
	for tryTime := 0; tryTime < m.ConnectionRetryTime; tryTime++ {
		logger.Debug(m.Context.GetRuntimeContext(), "start connect ", connStr)
		m.dbInstance, err = sql.Open(m.Driver, connStr)
		if err == nil {
			if len(m.StateMent) > 0 {
				m.dbStatment, err = m.dbInstance.Prepare(m.StateMent) // ignore_security_alert
				if err == nil {
					logger.Debug(m.Context.GetRuntimeContext(), "sql connect success, ping error", m.dbInstance.Ping())
					break
				} else {
					logger.Warning(m.Context.GetRuntimeContext(), initAlarmName, "init db statement error, err", err)
					break
				}
			}
		}
		time.Sleep(time.Millisecond * time.Duration(m.ConnectionRetryWaitMs))
	}
	return err
}

func (m *Rdb) InitCheckPointFromString(val string) {
	m.checkpointValue = val
}

func (m *Rdb) CheckPointToString() string {
	return m.checkpointValue
}

// Start starts the ServiceInput's service, whatever that may be
func (m *Rdb) Start(collector pipeline.Collector, connStr string, rdbFunc RdbFunc, columnResolverFuncMap map[string]ColumnResolverFunc) error {
	checkpointAlarmName := fmt.Sprintf("%s_CHECKPOINT_ALARM", strings.ToUpper(m.Driver))
	timeoutAlarmName := fmt.Sprintf("%s_TIMEOUT_ALARM", strings.ToUpper(m.Driver))
	queryAlarmName := fmt.Sprintf("%s_QUERY_ALARM", strings.ToUpper(m.Driver))
	m.waitGroup.Add(1)
	defer m.waitGroup.Done()
	// initialize additional query intervals
	err := m.initRdbsql(connStr, rdbFunc)
	if err != nil {
		return err
	}

	// init checkpoint
	if m.CheckPoint {
		val, exist := m.Context.GetCheckPoint(m.CheckPointColumn)
		if exist && len(val) > 0 {
			cp := CheckPoint{}
			err = json.Unmarshal(val, &cp)

			switch {
			case err != nil:
				logger.Error(m.Context.GetRuntimeContext(), checkpointAlarmName, "init checkpoint error, key", m.CheckPointColumn, "value", string(val), "error", err)
			case cp.CheckPointColumn == m.CheckPointColumn && m.CheckPointColumnType == cp.CheckPointColumnType:
				m.checkpointValue = cp.Value
			default:
				logger.Warning(m.Context.GetRuntimeContext(), checkpointAlarmName, "not matched checkpoint, may be config update, last column",
					cp.CheckPointColumn, "now column", m.CheckPointColumn, "last type", cp.CheckPointColumnType, "now type", m.CheckPointColumnType)
			}
		}
		if len(m.checkpointValue) == 0 {
			m.InitCheckPointFromString(m.CheckPointStart)
		}
		logger.Info(m.Context.GetRuntimeContext(), "use checkpoint", m.checkpointValue)
	}

	if len(m.StateMent) == 0 {
		return fmt.Errorf("no query statement")
	}

	// first collect after 10 ms
	timer := time.NewTimer(10 * time.Millisecond)

	for {
		select {
		case <-timer.C:
			startTime := time.Now()
			err = m.Collect(collector, columnResolverFuncMap)
			if err != nil {
				logger.Error(m.Context.GetRuntimeContext(), queryAlarmName, "collect err", err)
			}
			m.collectLatency.Observe(float64(time.Since(startTime)))
			endTime := time.Now()
			if endTime.Sub(startTime) > time.Duration(m.IntervalMs)*time.Millisecond/2 {
				logger.Warning(m.Context.GetRuntimeContext(), timeoutAlarmName, "sql collect cost very long time, start", startTime, "end", endTime, "intervalMs", m.IntervalMs)
				timer.Reset(time.Duration(m.IntervalMs) * time.Millisecond)
			} else {
				timer.Reset(time.Duration(m.IntervalMs)*time.Millisecond - endTime.Sub(startTime))
			}
			logger.Debug(m.Context.GetRuntimeContext(), "sql collect done, start", startTime, "end", endTime, "intervalMs", m.IntervalMs)
		case <-m.Shutdown:
			m.SaveCheckPoint(collector)
			logger.Info(m.Context.GetRuntimeContext(), "recv shutdown signal", "start to exit")
			return nil
		}
	}
}

func (m *Rdb) Collect(collector pipeline.Collector, columnResolverFuncMap map[string]ColumnResolverFunc) error {
	if m.dbStatment == nil {
		return fmt.Errorf("unknow error, instance not init")
	}

	params := make([]interface{}, 0, 2)
	if m.CheckPoint {
		params = append(params, m.checkpointValue)
	}
	if m.Limit && m.PageSize > 0 {
		offsetIndex := len(params)
		params = append(params, struct{}{})
		totalRowCount := 0
	ForBlock:
		for startOffset := 0; true; startOffset += m.PageSize {
			params[offsetIndex] = startOffset
			rows, err := m.dbStatment.Query(params...)
			if err != nil {
				return fmt.Errorf("execute query error, query : %s, err : %s", m.StateMent, err)
			}
			rowCount := m.ParseRows(rows, columnResolverFuncMap, collector)
			totalRowCount += rowCount
			if rowCount > 0 {
				logger.Info(m.Context.GetRuntimeContext(), "syn sql success, data count", rowCount, "offset", startOffset, "pagesize", m.PageSize)
				if m.CheckPointSavePerPage {
					m.SaveCheckPoint(collector)
				}
			}
			if m.MaxSyncSize > 0 && (startOffset+rowCount) >= m.MaxSyncSize {
				break
			}
			if rowCount < m.PageSize {
				break
			}
			select {
			case <-m.Shutdown:
				// repush an object and break
				m.Shutdown <- struct{}{}
				logger.Info(m.Context.GetRuntimeContext(), "syn sql success, data count", rowCount, "offset", startOffset, "pagesize", m.PageSize)
				break ForBlock
			default:
			}
		}
		if !m.CheckPointSavePerPage && totalRowCount > 0 {
			m.SaveCheckPoint(collector)
		}
		m.collectTotal.Add(int64(totalRowCount))
	} else {
		rows, err := m.dbStatment.Query(params...)
		if err != nil {
			return fmt.Errorf("execute query error, query : %s, err : %s", m.StateMent, err)
		}
		rowCount := m.ParseRows(rows, columnResolverFuncMap, collector)
		if rowCount > 0 {
			logger.Debug(m.Context.GetRuntimeContext(), "syn sql success, data count", rowCount)
			m.SaveCheckPoint(collector)
		}
		m.collectTotal.Add(int64(rowCount))
	}

	return nil
}

func (m *Rdb) SaveCheckPoint(collector pipeline.Collector) {
	checkpointAlarmName := fmt.Sprintf("%s_CHECKPOINT_ALARM", strings.ToUpper(m.Driver))
	cp := CheckPoint{
		CheckPointColumn:     m.CheckPointColumn,
		CheckPointColumnType: m.CheckPointColumnType,
		Value:                m.CheckPointToString(),
		LastUpdateTime:       time.Now(),
	}
	buf, err := json.Marshal(&cp)
	logger.Info(m.Context.GetRuntimeContext(), checkpointAlarmName, string(buf))
	if err != nil {
		logger.Warning(m.Context.GetRuntimeContext(), checkpointAlarmName, "save checkpoint marshal error, checkpoint", cp, "error", err)
		return
	}
	err = m.Context.SaveCheckPoint(m.CheckPointColumn, buf)
	if m.checkpointMetric != nil {
		m.checkpointMetric.Set(m.CheckPointColumn)
	}
	if err != nil {
		logger.Warning(m.Context.GetRuntimeContext(), checkpointAlarmName, "save checkpoint dump error, checkpoint", cp, "error", err)
	}
}

func (m *Rdb) ParseRows(rows *sql.Rows, columnResolverFuncMap map[string]ColumnResolverFunc, collector pipeline.Collector) int {
	parseAlarmName := fmt.Sprintf("%s_PARSE_ALARM", strings.ToUpper(m.Driver))
	defer rows.Close()
	rowCount := 0
	if m.columnsKeyBuffer == nil {
		columns, err := rows.Columns()
		if err != nil {
			logger.Warning(m.Context.GetRuntimeContext(), parseAlarmName, "no columns info, use default columns info, error", err)
		}
		m.columnTypes, err = rows.ColumnTypes()
		if err != nil {
			logger.Warning(m.Context.GetRuntimeContext(), parseAlarmName, "no columns type info, use default columns info, error", err)
		}

		m.columnsKeyBuffer = make([]string, len(columns))
		m.checkpointColumnIndex = 0
		foundCheckpointColumn := false
		for index, val := range columns {
			hashVal, exist := m.ColumnsHash[val]
			if exist {
				m.columnsKeyBuffer[index] = hashVal
			} else {
				m.columnsKeyBuffer[index] = val
			}
			if m.columnsKeyBuffer[index] == m.CheckPointColumn {
				m.checkpointColumnIndex = index
				foundCheckpointColumn = true
			}
		}
		if m.CheckPoint && len(m.CheckPointColumn) != 0 && !foundCheckpointColumn {
			logger.Warning(m.Context.GetRuntimeContext(), parseAlarmName, "no checkpoint column", m.CheckPointColumn)
		}

		m.columnValues = make([]sql.NullString, len(m.columnsKeyBuffer))
		m.columnStringValues = make([]string, len(m.columnsKeyBuffer))
		m.columnValuePointers = make([]interface{}, len(m.columnsKeyBuffer))
		for index := range m.columnValues {
			m.columnValuePointers[index] = &m.columnValues[index]
		}
	}
	if m.columnsKeyBuffer == nil {
		return 0
	}
	logger.Debug(m.Context.GetRuntimeContext(), "start", "parse")
	for rows.Next() {
		err := rows.Scan(m.columnValuePointers...)
		if err != nil {
			logger.Warning(m.Context.GetRuntimeContext(), parseAlarmName, "scan error, row", rowCount, "error", err)
			return rowCount
		}
		for index, val := range m.columnValues {
			if val.Valid {
				m.columnStringValues[index] = val.String

				if columnResolverFuncMap != nil {
					if columnResolver, ok := columnResolverFuncMap[m.columnTypes[index].DatabaseTypeName()]; ok {
						if parseVal, parseErr := columnResolver(val.String); parseErr == nil {
							m.columnStringValues[index] = parseVal
						}
					}
				}

			} else {
				m.columnStringValues[index] = "null"
			}
		}
		collector.AddDataArray(nil, m.columnsKeyBuffer, m.columnStringValues)
		rowCount++
	}
	if rowCount != 0 {
		logger.Warning(m.Context.GetRuntimeContext(), parseAlarmName, m.columnStringValues[m.checkpointColumnIndex], "rowCount", rowCount)
		m.checkpointValue = m.columnStringValues[m.checkpointColumnIndex]
	}
	return rowCount
}

// Stop stops the services and closes any necessary channels and connections
func (m *Rdb) Stop() error {
	m.Shutdown <- struct{}{}
	m.waitGroup.Wait()
	if m.dbStatment != nil {
		_ = m.dbStatment.Close()
	}
	if m.dbInstance != nil {
		_ = m.dbInstance.Close()
	}
	return nil
}
