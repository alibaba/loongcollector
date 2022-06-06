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

package mysql

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/go-sql-driver/mysql"

	"github.com/alibaba/ilogtail"
	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

type CheckPoint struct {
	CheckPointColumn     string
	CheckPointColumnType string
	Value                string
	LastUpdateTime       time.Time
}

type Mysql struct {
	DefaultCharset        string
	ColumnsCharset        map[string]string
	ColumnsHash           map[string]string
	ConnectionRetryTime   int
	ConnectionRetryWaitMs int
	Driver                string
	Net                   string
	Address               string
	DataBase              string
	User                  string
	Password              string
	PasswordPath          string
	Location              string
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
	SSLCA                 string
	SSLCert               string
	SSLKey                string

	// inner params
	checkpointColumnIndex int
	columnsKeyBuffer      []string
	checkpointValue       string
	dbInstance            *sql.DB
	dbStatment            *sql.Stmt
	columnValues          []sql.NullString
	columnStringValues    []string
	columnValuePointers   []interface{}
	shutdown              chan struct{}
	waitGroup             sync.WaitGroup
	context               ilogtail.Context
	collectLatency        ilogtail.LatencyMetric
	collectTotal          ilogtail.CounterMetric
	checkpointMetric      ilogtail.StringMetric
}

func (m *Mysql) Init(context ilogtail.Context) (int, error) {
	m.context = context
	if len(m.StateMent) == 0 && len(m.StateMentPath) != 0 {
		data, err := ioutil.ReadFile(m.StateMentPath)
		if err != nil && len(data) > 0 {
			m.StateMent = string(data)
		} else {
			logger.Warning(m.context.GetRuntimeContext(), "MYSQL_INIT_ALARM", "init sql statement error", err)
		}
	}
	if len(m.StateMent) == 0 {
		return 0, fmt.Errorf("no sql statement")
	}
	if m.Limit && m.PageSize > 0 {
		m.StateMent += " limit ?, " + strconv.Itoa(m.PageSize)
	}

	m.collectLatency = helper.NewLatencyMetric("mysql_collect_avg_cost")
	m.collectTotal = helper.NewCounterMetric("mysql_collect_total")
	m.context.RegisterCounterMetric(m.collectTotal)
	m.context.RegisterLatencyMetric(m.collectLatency)
	if m.CheckPoint {
		m.checkpointMetric = helper.NewStringMetric("mysql_checkpoint")
		m.context.RegisterStringMetric(m.checkpointMetric)
	}
	return 10000, nil
}

func (m *Mysql) Description() string {
	return "mysql input plugin for logtail"
}

func (m *Mysql) initMysql() error {
	tlsConfig, err := util.GetTLSConfig(m.SSLCert, m.SSLKey, m.SSLCA, false)
	if err != nil {
		logger.Error(m.context.GetRuntimeContext(), "MYSQL_INIT_ALARM", "MySQL Error registering TLS config", err)
	}

	if tlsConfig != nil {
		_ = mysql.RegisterTLSConfig("custom", tlsConfig)
	}

	serv, err := m.dsnConfig()
	if err != nil {
		return err
	}
	for tryTime := 0; tryTime < m.ConnectionRetryTime; tryTime++ {
		logger.Debug(m.context.GetRuntimeContext(), "mysql start connect ", serv)
		m.dbInstance, err = sql.Open("mysql", serv)
		if err == nil {
			if len(m.StateMent) > 0 {
				m.dbStatment, err = m.dbInstance.Prepare(m.StateMent)
				if err == nil {
					logger.Debug(m.context.GetRuntimeContext(), "sql connect success, ping error", m.dbInstance.Ping())
					break
				} else {
					logger.Warning(m.context.GetRuntimeContext(), "MYSQL_INIT_ALARM", "init db statement error, err", err)
					break
				}
			}
		}
		time.Sleep(time.Millisecond * time.Duration(m.ConnectionRetryWaitMs))
	}
	return err
}

// Net                   string
// Address               string
// DataBase              string
// User                  string
// Password              string
// PasswordPath          string
// Location              string
// DialTimeOutMs         int
// ReadTimeOutMs         int
func (m *Mysql) dsnConfig() (string, error) {
	conf, err := mysql.ParseDSN("")
	if err != nil {
		return "", err
	}

	if len(m.Address) > 0 {
		conf.Addr = m.Address
	}
	if len(m.DataBase) > 0 {
		conf.DBName = m.DataBase
	}
	if len(conf.Net) > 0 {
		conf.Net = m.Net
	}
	if len(m.User) > 0 {
		conf.User = m.User
	}
	if len(m.Password) == 0 {
		if len(m.PasswordPath) != 0 {
			lines, err := util.ReadLines(m.PasswordPath)
			if err != nil && len(lines) > 0 {
				m.Password = strings.TrimSpace(lines[0])
			}
		}
	}
	if len(m.Password) > 0 {
		conf.Passwd = m.Password
	}
	if len(m.Location) > 0 {
		loc, err := time.LoadLocation(m.Location)
		if err != nil {
			conf.Loc = loc
		} else {
			logger.Error(m.context.GetRuntimeContext(), "MYSQL_INIT_ALARM", "Set Mysql location error, loc", m.Location, "error", err)
		}
	}

	conf.Timeout = time.Duration(m.DialTimeOutMs) * time.Millisecond
	conf.ReadTimeout = time.Duration(m.ReadTimeOutMs) * time.Millisecond

	if conf.Timeout == 0 {
		conf.Timeout = time.Second * 5
	}

	return conf.FormatDSN(), nil
}

func (m *Mysql) InitCheckPointFromString(val string) {
	// if m.CheckPointColumnType == "time" {
	// 	t, err := time.Parse("2006-01-02 15:04:05", val)
	// 	if err != nil {
	// 		logger.Warning("MYSQL_INIT_ALARM", "init time checkpoint error, value", val, "error", err)
	// 		m.checkpointValue = time.Unix(0, 0)
	// 	} else {
	// 		m.checkpointValue = t
	// 	}
	// } else {
	// 	m.checkpointValue, _ = strconv.Atoi(val)
	// }
	m.checkpointValue = val
}

func (m *Mysql) CheckPointToString() string {
	return m.checkpointValue
}

// Start starts the ServiceInput's service, whatever that may be
func (m *Mysql) Start(collector ilogtail.Collector) error {
	m.waitGroup.Add(1)
	defer m.waitGroup.Done()
	// initialize additional query intervals
	err := m.initMysql()
	if err != nil {
		return err
	}

	// init checkpoint
	if m.CheckPoint {
		val, exist := m.context.GetCheckPoint(m.CheckPointColumn)
		if exist && len(val) > 0 {
			cp := CheckPoint{}
			err = json.Unmarshal(val, &cp)

			switch {
			case err != nil:
				logger.Error(m.context.GetRuntimeContext(), "MYSQL_CHECKPOING_ALARM", "init checkpoint error, key", m.CheckPointColumn, "value", string(val), "error", err)
			case cp.CheckPointColumn == m.CheckPointColumn && m.CheckPointColumnType == cp.CheckPointColumnType:
				m.checkpointValue = cp.Value
			default:
				logger.Warning(m.context.GetRuntimeContext(), "MYSQL_CHECKPOING_ALARM", "not matched checkpoint, may be config update, last column",
					cp.CheckPointColumn, "now column", m.CheckPointColumn, "last type", cp.CheckPointColumnType, "now type", m.CheckPointColumnType)
			}
		}
		if len(m.checkpointValue) == 0 {
			m.InitCheckPointFromString(m.CheckPointStart)
		}
		logger.Debug(m.context.GetRuntimeContext(), "use checkpoint", m.checkpointValue)
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
			m.collectLatency.Begin()
			err = m.Collect(collector)
			if err != nil {
				logger.Error(m.context.GetRuntimeContext(), "MYSQL_QUERY_ALARM", "sql query error", err)
			}
			m.collectLatency.End()
			endTime := time.Now()
			if endTime.Sub(startTime) > time.Duration(m.IntervalMs)*time.Millisecond/2 {
				logger.Warning(m.context.GetRuntimeContext(), "MYSQL_TIMEOUT_ALARM", "sql collect cost very long time, start", startTime, "end", endTime, "intervalMs", m.IntervalMs)
				timer.Reset(time.Duration(m.IntervalMs) * time.Millisecond)
			} else {
				timer.Reset(time.Duration(m.IntervalMs)*time.Millisecond - endTime.Sub(startTime))
			}
			logger.Debug(m.context.GetRuntimeContext(), "sql collect done, start", startTime, "end", endTime, "intervalMs", m.IntervalMs)
		case <-m.shutdown:
			m.SaveCheckPoint(collector)
			logger.Debug(m.context.GetRuntimeContext(), "recv shutdown signal", "start to exit")
			return nil
		}
	}
}

func (m *Mysql) Collect(collector ilogtail.Collector) error {
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
			rowCount := m.ParseRows(rows, collector)
			totalRowCount += rowCount
			if rowCount > 0 {
				logger.Debug(m.context.GetRuntimeContext(), "syn sql success, data count", rowCount, "offset", startOffset, "pagesize", m.PageSize)
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
			case <-m.shutdown:
				// repush a object and break
				m.shutdown <- struct{}{}
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
		rowCount := m.ParseRows(rows, collector)
		if rowCount > 0 {
			logger.Debug(m.context.GetRuntimeContext(), "syn sql success, data count", rowCount)
			m.SaveCheckPoint(collector)
		}
		m.collectTotal.Add(int64(rowCount))
	}

	return nil
}

func (m *Mysql) SaveCheckPoint(collector ilogtail.Collector) {
	cp := CheckPoint{CheckPointColumn: m.CheckPointColumn, CheckPointColumnType: m.CheckPointColumnType, Value: m.CheckPointToString(), LastUpdateTime: time.Now()}
	buf, err := json.Marshal(&cp)
	if err != nil {
		logger.Warning(m.context.GetRuntimeContext(), "MYSQL_CHECKPOINT_ALARM", "save checkpoint marshal error, checkpoint", cp, "error", err)
		return
	}
	err = m.context.SaveCheckPoint(m.CheckPointColumn, buf)
	if m.checkpointMetric != nil {
		m.checkpointMetric.Set(m.CheckPointColumn)
	}
	if err != nil {
		logger.Warning(m.context.GetRuntimeContext(), "MYSQL_CHECKPOINT_ALARM", "save checkpoint dump error, checkpoint", cp, "error", err)
	}
}

func (m *Mysql) ParseRows(rows *sql.Rows, collector ilogtail.Collector) int {
	// Must be closed manually, otherwise the connection will not be closed when the statement
	//   is a storage procedure call such as 'CALL SP(?)' or error happened.
	defer rows.Close()
	rowCount := 0
	if m.columnsKeyBuffer == nil {
		columns, err := rows.Columns()
		if err != nil {
			logger.Warning(m.context.GetRuntimeContext(), "MYSQL_PARSE_ALARM", "no columns info, use default columns info, error", err)
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
			logger.Warning(m.context.GetRuntimeContext(), "MYSQL_PARSE_ALARM", "no checkpoint column", m.CheckPointColumn)
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
	logger.Debug(m.context.GetRuntimeContext(), "start", "parse")
	for rows.Next() {
		err := rows.Scan(m.columnValuePointers...)
		if err != nil {
			logger.Warning(m.context.GetRuntimeContext(), "MYSQL_PARSE_ALARM", "scan error, row", rowCount, "error", err)
			return rowCount
		}
		for index, val := range m.columnValues {
			if val.Valid {
				m.columnStringValues[index] = val.String
			} else {
				m.columnStringValues[index] = "null"
			}
		}
		collector.AddDataArray(nil, m.columnsKeyBuffer, m.columnStringValues)
		rowCount++
	}
	if rowCount != 0 {
		m.checkpointValue = m.columnStringValues[m.checkpointColumnIndex]
	}
	return rowCount
}

// Stop stops the services and closes any necessary channels and connections
func (m *Mysql) Stop() error {
	m.shutdown <- struct{}{}
	m.waitGroup.Wait()
	if m.dbStatment != nil {
		_ = m.dbStatment.Close()
	}
	if m.dbInstance != nil {
		_ = m.dbInstance.Close()
	}
	return nil
}

func init() {
	ilogtail.ServiceInputs["service_mysql"] = func() ilogtail.ServiceInput {
		return &Mysql{
			ConnectionRetryTime:   3,
			ConnectionRetryWaitMs: 5000,
			Driver:                "mysql",
			Net:                   "tcp",
			Address:               "127.0.0.1:3306",
			User:                  "root",
			MaxSyncSize:           10000,
			DialTimeOutMs:         5000,
			ReadTimeOutMs:         5000,
			IntervalMs:            60000,
			shutdown:              make(chan struct{}, 2),
		}
	}
}
