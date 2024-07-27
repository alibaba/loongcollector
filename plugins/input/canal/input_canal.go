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

package canal

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"

	"github.com/go-mysql-org/go-mysql/canal"
	"github.com/go-mysql-org/go-mysql/mysql"
	"github.com/go-mysql-org/go-mysql/replication"
	"github.com/go-mysql-org/go-mysql/schema"
	canalLog "github.com/sirupsen/logrus"
)

func mysqlValueToString(value mysql.FieldValue) (string, error) {
	if value.Type == mysql.FieldValueTypeString {
		return string(value.AsString()), nil
	}
	return "", fmt.Errorf("invalid value")
}

type LogCanal struct {
}

// for canal log system
func (p *LogCanal) Levels() []canalLog.Level {
	return canalLog.AllLevels
}

// for canal log system
func (p *LogCanal) Fire(e *canalLog.Entry) error {
	switch {
	case e.Level == canalLog.WarnLevel:
		logger.Warning(context.Background(), "INPUT_CANAL_ALARM", "canal log, level", e.Level.String(), "message", e.Message)
	case e.Level == canalLog.ErrorLevel || e.Level == canalLog.FatalLevel || e.Level == canalLog.PanicLevel:
		logger.Error(context.Background(), "INPUT_CANAL_ALARM", "canal log, level", e.Level.String(), "message", e.Message)
	default:
		logger.Info(context.Background(), "canal log, level", e.Level.String(), "message", e.Message)
	}
	return nil
}

type CheckPoint struct {
	GTID     string
	FileName string
	Offset   uint32
	ID       int
}

// ServiceCanal is a service input plugin to collect binlog from MySQL.
// It works as a slave node to collect binlog from master (source) continually.
// It supports two kinds of replication mode: GTID or binlog-file.
//
// GTID mode uses GTID as checkpoint, which needs the server to enable
// this feature (by setting gtid_mode=ON). At the beginning, plugin will
// check this necessary condition, it will fall down to binlog-file mode
// if gtid_mode is OFF.
// One special case: the server has turned on gtid_mode, but the latest GTID
// is empty. In such case, plugin will fall down to binlog-file mode at beginning.
//
// Binlog-file mode uses the sequence number of file as checkpoint, which might
// leads to data repetition in master-slave arch. Because for same binlog content,
// master and slave nodes can use different sequence number to store it, once
// the sequence number on slave is higher and HA switch happens, data repetition
// happened. That's why we prefer GTID mode.
type ServiceCanal struct {
	Host     string
	Port     int
	User     string
	Password string
	Flavor   string
	ServerID int
	// Array of regex string, only when the name of table (included schema, such as db.test)
	// matches any regex in this array, its binlog will be collected.
	// Example: "IncludeTables": [".*\\..*"], collect all tables, notes the backslash for escaping.
	IncludeTables []string
	// Array of regex string, if the name of table (included schema, too) matches any regex
	// in this array, its binlog will not be collected.
	// Example: "ExcludeTables": ["^mysql\\..*$"], exclude all tables in schema named 'mysql'.
	ExcludeTables []string
	// Start* are used to specify start synchronization point. StartGTID is preferred if
	// both of them are specified. **Only works when no checkpoint is exiting.**
	// Start* will fail if any error is encountered during synchronization because plugin will
	// recovery from latest checkpoint (got from server).
	// StartGTID will not synchrnoize event specified by it, for example, set StartGTID to
	// 'uuid:1-9', then plugin will skip 1 to 9, and synchrnoize from interval 10.
	// However, StartBinName and StartBinLogPos will start synchrnoize from the event identified
	// by them.
	// For StartGTID:
	// - If its format is invalid (parse error), plugin starts synchronization from latest,
	//   because in such case, the first synchronization will fail, and plugin will get the
	//   latest checkpoint from server to failover.
	// - If its format is valid but value is invalid, plugin's behavior depends on server. It
	//   might start synchrnoize from beginning or ERROR to start from latest.
	// For StartBinName and StartBinLogPos:
	// - If any of them are invalid, plugins starts synchronization from latest (failover).
	StartGTID       string
	StartBinName    string
	StartBinLogPos  uint32
	HeartBeatPeriod int
	ReadTimeout     int
	EnableDDL       bool
	EnableXID       bool
	EnableGTID      bool
	EnableInsert    bool
	EnableUpdate    bool
	EnableDelete    bool
	TextToString    bool
	EnableEventMeta bool
	// True by default, convert field with SET type to string in format [elem1, elem2, ...].
	SetToString bool
	// True by default, convert field in Go []byte type to string, such as JSON type.
	ByteValueToString bool
	// TODO: This parameter is not exposed in document.
	// StartFromBegining only works when no checkpoint and all Start* parameters are missing.
	// If StartFromBegining is set, plugin will not try to get latest position from server.
	StartFromBegining bool
	Charset           string
	// Pack values into two fields: new_data and old_data. False by default.
	PackValues bool
	// True时 binlog解析DECIMAL输出原格式而非科学计数法, like:https://github.com/go-mysql-org/go-mysql/blob/6c99b4bff931a5aced0978b78aadb5867afcdcd3/canal/dump.go#L85
	UseDecimal bool

	shutdown  chan struct{}
	waitGroup sync.WaitGroup

	isGTIDEnabled      bool
	nextRowEventGTID   string // update before data is consumed
	config             *canal.Config
	canal              *canal.Canal
	checkpoint         CheckPoint // update checkpoint after data is consumed
	context            pipeline.Context
	collector          pipeline.Collector
	lastCheckPointTime time.Time
	lastErrorCount     int
	lastErrorChan      chan error

	rotateCounter     pipeline.CounterMetric
	syncCounter       pipeline.CounterMetric
	ddlCounter        pipeline.CounterMetric
	rowCounter        pipeline.CounterMetric
	xgidCounter       pipeline.CounterMetric
	checkpointCounter pipeline.CounterMetric
	lastBinLogMetric  pipeline.StringMetric
	lastGTIDMetric    pipeline.StringMetric
}

func (sc *ServiceCanal) Init(context pipeline.Context) (int, error) {
	sc.context = context
	if sc.ReadTimeout < 1 {
		sc.ReadTimeout = 120
	}
	if sc.HeartBeatPeriod >= sc.ReadTimeout {
		sc.HeartBeatPeriod = sc.ReadTimeout / 2
	}
	sc.config = canal.NewDefaultConfig()
	sc.config.Addr = fmt.Sprintf("%s:%d", sc.Host, sc.Port)
	sc.config.User = sc.User
	sc.config.Password = sc.Password
	sc.config.Flavor = sc.Flavor
	sc.config.ReadTimeout = time.Duration(sc.ReadTimeout) * time.Second
	sc.config.HeartbeatPeriod = time.Duration(sc.HeartBeatPeriod) * time.Second
	sc.config.Dump.ExecutionPath = ""
	sc.config.Dump.DiscardErr = false
	sc.config.Charset = sc.Charset
	sc.config.ServerID = uint32(sc.ServerID)
	sc.config.DiscardNoMetaRowEvent = true
	sc.config.IncludeTableRegex = sc.IncludeTables
	sc.config.ExcludeTableRegex = sc.ExcludeTables
	sc.config.UseDecimal = sc.UseDecimal

	sc.lastErrorChan = make(chan error, 1)

	metricsRecord := context.GetMetricRecord()
	sc.rotateCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_rotate")
	sc.syncCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_sync")
	sc.ddlCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_ddl")
	sc.rowCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_row")
	sc.xgidCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_xgid")
	sc.checkpointCounter = helper.NewCounterMetricAndRegister(metricsRecord, "binlog_checkpoint")
	sc.lastBinLogMetric = helper.NewStringMetricAndRegister(metricsRecord, "binlog_filename")
	sc.lastGTIDMetric = helper.NewStringMetricAndRegister(metricsRecord, "binlog_gtid")

	return 0, nil
}

var canalMetaFields = []string{
	"_db_", "_event_", "_gtid_", "_host_", "_id_",
	"_table_", "_filename_", "_offset_", "_event_time_", "_event_log_postion_", "_event_size_", "_event_server_id_", //nolint:misspell
}

var canalOldFieldPrefix = "_old_"

// addData will pack values into new_data and old_data two fields if this feature is enabled.
// Three kinds of fields:
// - Meta fields: stay unchanged, such as _db_, _event_, _gtid_, _host_, _id_. _table_.
// - Old fields (old_data): any fields start with '_old_'.
// - New fields (data): remaining fields.
func (sc *ServiceCanal) addData(values map[string]string) {
	if !sc.PackValues {
		sc.collector.AddData(nil, values)
		return
	}

	oldData := make(map[string]string)
	newData := make(map[string]string)
	packedData := make(map[string]string)
ValueLoop:
	for key, value := range values {
		for _, mf := range canalMetaFields {
			if key == mf {
				packedData[key] = value
				continue ValueLoop
			}
		}
		if strings.HasPrefix(key, canalOldFieldPrefix) {
			oldData[key] = value
			continue
		}
		newData[key] = value
	}

	marshal := func(fieldName string, value map[string]string) {
		if len(value) == 0 {
			packedData[fieldName] = ""
			return
		}

		b, err := json.Marshal(value)
		if err == nil {
			packedData[fieldName] = string(b)
			return
		}
		logger.Warningf(sc.context.GetRuntimeContext(), "CANAL_RUNTIME_ALARM",
			"json.Marshal on %v failed: %v, %v", fieldName, value, err)
		packedData[fieldName] = ""
	}
	marshal("old_data", oldData)
	marshal("data", newData)
	sc.collector.AddData(nil, packedData)
}

func (sc *ServiceCanal) OnRotate(_ *replication.EventHeader, r *replication.RotateEvent) error {
	logger.Info(sc.context.GetRuntimeContext(), "bin log file rotate", string(r.NextLogName), "pos", r.Position)
	sc.lastBinLogMetric.Set(string(r.NextLogName))
	sc.rotateCounter.Add(1)
	return nil
}

// OnDDL...
func (sc *ServiceCanal) OnDDL(_ *replication.EventHeader, pos mysql.Position, e *replication.QueryEvent) error {
	logger.Debugf(sc.context.GetRuntimeContext(), "[CANAL_DEBUG] OnDDL:%v GTID:%v Pos:%v", string(e.Query), sc.nextRowEventGTID, pos.Pos)
	sc.ddlCounter.Add(1)
	if !sc.EnableDDL {
		return nil
	}
	values := map[string]string{
		"_host_":        sc.Host,
		"ErrorCode":     strconv.Itoa(int(e.ErrorCode)),
		"ExecutionTime": strconv.Itoa(int(e.ExecutionTime)),
		"_db_":          string(e.Schema),
		"Query":         string(e.Query),
		"StatusVars":    string(e.StatusVars),
		"_event_":       "ddl",
	}
	nextOffsetString := strconv.FormatUint(uint64(pos.Pos), 10)
	if sc.EnableGTID {
		values["_gtid_"] = sc.nextRowEventGTID
		values["_filename_"] = sc.checkpoint.FileName
		values["_offset_"] = nextOffsetString
	}
	sc.addData(values)
	return nil
}

func (sc *ServiceCanal) columnValueToString(column *schema.TableColumn, rowVal interface{}) string {
	switch column.Type {
	case schema.TYPE_ENUM:
		if val, ok := rowVal.(int64); ok {
			if int(val) > 0 && int(val) <= len(column.EnumValues) {
				return column.EnumValues[int(val-1)]
			}
		}
	case schema.TYPE_STRING:
		if sc.TextToString && strings.HasSuffix(column.RawType, "text") {
			if val, ok := rowVal.([]byte); ok {
				return string(val)
			}
		}
	case schema.TYPE_JSON:
		if val, ok := rowVal.([]byte); ok {
			return string(val)
		}
	case schema.TYPE_SET:
		if !sc.SetToString {
			break
		}
		if val, ok := rowVal.(int64); ok {
			subSetCount := 1 << uint(len(column.SetValues))
			if val >= 0 && val < int64(subSetCount) {
				bits := strconv.FormatInt(val, 2)
				var setValue []string
				for i := 0; i < len(bits); i++ {
					if '0' == bits[i] {
						continue
					}
					setValue = append([]string{column.SetValues[len(bits)-1-i]}, setValue...)
				}
				return fmt.Sprint(setValue)
			}
		}
	default:
		if sc.ByteValueToString {
			if val, ok := rowVal.([]byte); ok {
				return string(val)
			}
		}
	}
	return fmt.Sprint(rowVal)
}

// OnRow processes the row event, according user's config, constructs data to send.
func (sc *ServiceCanal) OnRow(e *canal.RowsEvent) error {
	logger.Debug(sc.context.GetRuntimeContext(), "[CANAL_DEBUG] host", sc.Host, "db", e.Table.Schema, "table", e.Table.Name, "action", e.Action, "GTID", sc.nextRowEventGTID)
	sc.rowCounter.Add(1)
	values := map[string]string{"_host_": sc.Host, "_db_": e.Table.Schema, "_table_": e.Table.Name, "_event_": "row_" + e.Action, "_id_": strconv.Itoa(sc.checkpoint.ID)}
	nextOffsetString := strconv.FormatUint(uint64(e.Header.LogPos), 10)
	if sc.EnableGTID {
		values["_gtid_"] = sc.nextRowEventGTID
		values["_filename_"] = sc.checkpoint.FileName
		values["_offset_"] = nextOffsetString
	}
	if sc.EnableEventMeta && e.Header != nil {
		values["_event_time_"] = strconv.Itoa(int(e.Header.Timestamp))
		values["_event_log_postion_"] = strconv.Itoa(int(e.Header.LogPos)) //nolint:misspell
		values["_event_size_"] = strconv.Itoa(int(e.Header.EventSize))
		values["_event_server_id_"] = strconv.Itoa(int(e.Header.ServerID))
	}
	if e.Action == canal.UpdateAction {
		if !sc.EnableUpdate {
			return nil
		}
		if len(e.Rows)%2 != 0 {
			logger.Error(sc.context.GetRuntimeContext(), "CANAL_INVALID_ALARM", "invalid update value count", len(e.Rows))
			return nil
		}
		for i := 0; i < len(e.Rows); i += 2 {
			if len(e.Rows[i]) != len(e.Table.Columns) || len(e.Rows[i+1]) != len(e.Table.Columns) {
				// clear cache, force update meta
				// @bug fix : if table with no PK, len(e.Rows[i])-len(e.Table.Columns) always be 1, and we
				//            should not flush table schema, it will cause high load for mysql
				if i == 0 && !(len(e.Rows[i])-len(e.Table.Columns) == 1 && len(e.Table.PKColumns) == 0) {
					logger.Info(sc.context.GetRuntimeContext(), "table column size", len(e.Rows[i]), " != event column size", len(e.Table.Columns))
					sc.canal.ClearTableCache([]byte(e.Table.Schema), []byte(e.Table.Name))
					tableMeta, err := sc.canal.GetTable(e.Table.Schema, e.Table.Name)
					if err != nil || tableMeta == nil {
						logger.Error(sc.context.GetRuntimeContext(), "CANAL_INVALID_ALARM", "invalid row values", e.Table.Name,
							"old columns", len(e.Rows[i]),
							"new columns", len(e.Rows[i+1]),
							"table meta columns", len(e.Table.Columns),
							"error", err)
					} else {
						e.Table = tableMeta
					}
				}
				for index, rowVal := range e.Rows[i] {
					if index < len(e.Table.Columns) {
						values["_old_"+e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
					} else {
						values[fmt.Sprintf("_old_unknow_col_%d", index)] = fmt.Sprint(rowVal)
					}
				}

				for index, rowVal := range e.Rows[i+1] {
					if index < len(e.Table.Columns) {
						values[e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
					} else {
						values[fmt.Sprintf("unknow_col_%d", index)] = fmt.Sprint(rowVal)
					}
				}
			} else {
				for index, rowVal := range e.Rows[i] {
					values["_old_"+e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
				}

				for index, rowVal := range e.Rows[i+1] {
					values[e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
				}
			}
			sc.addData(values)
		}
	} else {
		if e.Action == canal.DeleteAction {
			if !sc.EnableDelete {
				return nil
			}
		} else {
			if !sc.EnableInsert {
				return nil
			}
		}
		for i, rowValues := range e.Rows {
			if len(rowValues) != len(e.Table.Columns) {
				if i == 0 && !(len(e.Rows[i])-len(e.Table.Columns) == 1 && len(e.Table.PKColumns) == 0) {
					sc.canal.ClearTableCache([]byte(e.Table.Schema), []byte(e.Table.Name))
					tableMeta, err := sc.canal.GetTable(e.Table.Schema, e.Table.Name)
					if err != nil || tableMeta == nil {
						logger.Error(sc.context.GetRuntimeContext(), "CANAL_INVALID_ALARM", "invalid row values", e.Table.Name,
							"columns", len(rowValues),
							"table meta columns", len(e.Table.Columns),
							"error", err)
					} else {
						e.Table = tableMeta
					}
				}
				for index, rowVal := range rowValues {
					if index < len(e.Table.Columns) {
						values[e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
					} else {
						values[fmt.Sprintf("unknow_col_%d", index)] = fmt.Sprint(rowVal)
					}
				}
			} else {
				for index, rowVal := range rowValues {
					values[e.Table.Columns[index].Name] = sc.columnValueToString(&e.Table.Columns[index], rowVal)
				}
			}
			sc.addData(values)
		}
	}

	// Update checkpoint.
	sc.checkpoint.ID++
	return nil
}

/*
	  What is an XID_EVENT?
		An XID_EVENT marks the end of a transaction that has been committed. It signifies that the transaction has been fully completed and can be safely replicated to a slave server. When a transaction is committed in a MySQL server, an XID_EVENT is written to the binlog to signify that the commit operation has been successfully completed.

		When will an XID_EVENT be triggered?
		An XID_EVENT is triggered and written to the binlog in the following scenarios:

		Commit of a Transaction: When a transaction is committed in InnoDB or another transactional storage engine, an XID_EVENT is written to the binlog. This happens at the end of the transaction, indicating that all changes within that transaction are now complete and consistent.

		Replication: During replication, the slave server reads the XID_EVENT to understand that a transaction has been committed on the master. This helps in maintaining transactional consistency between the master and the slave.
*/
func (sc *ServiceCanal) OnXID(_ *replication.EventHeader, p mysql.Position) error {
	logger.Debugf(sc.context.GetRuntimeContext(), "[CANAL_DEBUG] OnXID Pos:%v", p.String())
	sc.xgidCounter.Add(1)
	return nil
}

// OnGTID reports the GTID of the following event (OnRow, OnDDL).
// So we can not update checkpoint here, just record GTID and update in OnRow.
//
// This strategy brings a potential problem, checkpoint will only be updated
// when OnRow is called, however, because of IncludeTables and ExcludeTables,
// calls to OnRow will be filtered. So, if plugin restarts before the next
// OnRow call comes, it will rerun from a old checkpoint.
// But this should be trivial for cases that valid data comes continuously.
func (sc *ServiceCanal) OnGTID(_ *replication.EventHeader, e mysql.BinlogGTIDEvent) error {
	sc.xgidCounter.Add(1)
	s, err := e.GTIDNext()
	if err != nil {
		return err
	}
	logger.Debugf(sc.context.GetRuntimeContext(), "[CANAL_DEBUG] OnGTID: %v", s.String())
	sc.nextRowEventGTID = s.String()
	return nil
}

// OnPosSynced is called right after RotateEvent, XIDEvent and DDLEvent
// If our handle do not return err, we do not need to save checkpoint in those handlers
func (sc *ServiceCanal) OnPosSynced(_ *replication.EventHeader, pos mysql.Position, gset mysql.GTIDSet, force bool) error {
	logger.Debugf(sc.context.GetRuntimeContext(), "[CANAL_DEBUG] OnPosSynced: %v", gset.String())
	sc.syncCounter.Add(1)
	sc.checkpoint.FileName = pos.Name
	sc.checkpoint.Offset = pos.Pos
	if sc.checkpoint.GTID != "" {
		sc.checkpoint.GTID = gset.String()
	}
	nowTime := time.Now()
	// save checkpoint 3 second per time
	if nowTime.Sub(sc.lastCheckPointTime) > time.Duration(3)*time.Second ||
		force {
		sc.saveCheckPoint()
	}
	return nil
}

func (sc *ServiceCanal) OnRowsQueryEvent(_ *replication.RowsQueryEvent) error {
	return nil
}

func (sc *ServiceCanal) OnTableChanged(_ *replication.EventHeader, _ string, _ string) error {
	return nil
}

func (sc *ServiceCanal) String() string { return "logtail mysql canal" }

func (sc *ServiceCanal) Description() string {
	return "mysql canal service input plugin for logtail"
}

// initCheckPoint tries to get last checkpoint with key 'msyql_canal'.
// If no such checkpoint is found, use Start* values from user config.
func (sc *ServiceCanal) initCheckPoint() {
	ok := sc.context.GetCheckPointObject("mysql_canal", &sc.checkpoint)
	if ok {
		return
	}
	sc.checkpoint.FileName = sc.StartBinName
	sc.checkpoint.Offset = sc.StartBinLogPos
	sc.checkpoint.GTID = sc.StartGTID
}

func (sc *ServiceCanal) saveCheckPoint() {
	sc.checkpointCounter.Add(1)
	sc.lastCheckPointTime = time.Now()
	_ = sc.context.SaveCheckPointObject("mysql_canal", &sc.checkpoint)
}

// Collect takes in an accumulator and adds the metrics that the Input
// gathers. This is called every "interval"
func (sc *ServiceCanal) Collect(pipeline.Collector) error {
	return nil
}

func (sc *ServiceCanal) runCanal(pos mysql.Position) {
	logger.Infof(sc.context.GetRuntimeContext(), "start canal from %v with binlog-file mode", pos)
	sc.canal.SetEventHandler(sc)
	sc.lastBinLogMetric.Set(pos.String())
	sc.checkpoint.FileName = pos.Name
	sc.checkpoint.Offset = pos.Pos
	sc.checkpoint.GTID = ""
	sc.saveCheckPoint()
	sc.lastErrorChan <- sc.canal.RunFrom(pos)
}

func (sc *ServiceCanal) runCanalByGTID(gtid mysql.GTIDSet) {
	logger.Infof(sc.context.GetRuntimeContext(), "start canal from %v with GTID mode", gtid)
	sc.canal.SetEventHandler(sc)
	sc.lastGTIDMetric.Set(gtid.String())
	var pos mysql.Position
	sc.checkpoint.FileName = pos.Name
	sc.checkpoint.Offset = pos.Pos
	sc.checkpoint.GTID = gtid.String()
	sc.saveCheckPoint()
	sc.lastErrorChan <- sc.canal.StartFromGTID(gtid)
}

// GetBinlogLatestPos gets the latest binlog position from server.
func (sc *ServiceCanal) GetBinlogLatestPos() mysql.Position {
	latestPos := mysql.Position{}
	result, err := sc.canal.Execute("show binary logs")
	if err == nil && result != nil {
		if len(result.Values) > 0 {
			value := result.Values[len(result.Values)-1]
			if len(value) == 2 {
				valueStr, errConv := mysqlValueToString(value[0])
				if errConv != nil {
					latestPos.Name = valueStr
				} else {
					logger.Error(sc.context.GetRuntimeContext(), "CANAL_INVALID_ALARM", "show binary logs error")
				}
				offset, conErr := strconv.ParseUint(fmt.Sprint(value[1]), 10, 64)
				if conErr == nil {
					latestPos.Pos = uint32(offset)
				}
			}
		}
	} else {
		logger.Error(sc.context.GetRuntimeContext(), "CANAL_INVALID_ALARM", "show binary logs error", err)
	}
	logger.Info(sc.context.GetRuntimeContext(), "start from latest binlog position", latestPos)
	return latestPos
}

// getGTIDMode checks if the server supports or enables gtid_mode.
// @return true if gtid_mode is ON, otherwise false. The second return value is
// used to indicate if the error is caused by network, so that caller can know
// when to retry (for temporary network problem).
func (sc *ServiceCanal) getGTIDMode() (bool, bool, error) {
	statement := "select @@global.gtid_mode"
	result, err := sc.canal.Execute(statement)
	if err != nil {
		return false, true, fmt.Errorf(
			"Execute statement (%v) failed during getGTIDMode, error: %v",
			statement, err)
	}
	if 0 == len(result.Values) {
		return false, false, nil
	}
	value := result.Values[len(result.Values)-1]
	if len(value) != 1 {
		return false, false, fmt.Errorf("The number of columns (%v) is not 1 for %v",
			len(value), statement)
	}
	gtidModeVal, err := mysqlValueToString(value[0])
	if err != nil {
		return false, false, fmt.Errorf("Invaild GTID mode value, error: %v", err)
	}
	if "on" == strings.ToLower(gtidModeVal) {
		return true, false, nil
	}
	return false, false, nil
}

// getLatestGTID gets the latest GTID from server.
func (sc *ServiceCanal) getLatestGTID() (mysql.GTIDSet, error) {
	// do not use "show global variables like 'gtid_executed'" as it truncates gtid to 1024 bytes
	statement := "select @@global.gtid_executed"
	result, err := sc.canal.Execute(statement)
	if err != nil {
		return nil, fmt.Errorf("Execute statement %v failed during getLatestGTID, error: %v",
			statement, err)
	}
	if 0 == len(result.Values) {
		return nil, fmt.Errorf("Empty result: statement (%v)", statement)
	}
	value := result.Values[len(result.Values)-1]
	if len(value) != 1 {
		return nil, fmt.Errorf("The number of columns (%v) is not 1 for %v", len(value), statement)
	}

	gtidVal, err := mysqlValueToString(value[0])
	if err != nil {
		return nil, fmt.Errorf("Invaild GTID value, error: %v", err)
	}
	gtid, err := mysql.ParseGTIDSet(sc.Flavor, gtidVal)
	if err != nil {
		return nil, fmt.Errorf("ParseGTIDSet(%v) failed: %v", gtidVal, err)
	}
	return gtid, nil
}

// newCanal is a wrapper of canal.NewCanal with retry logic (every 5 seconds).
// @return bool to indicate if the shutdown has signaled. non-nil error if
// NewCanal failed because of non-ROW mode.
func (sc *ServiceCanal) newCanal() (bool, error) {
	var err error
	for {
		sc.canal, err = canal.NewCanal(sc.config)
		if sc.canal != nil {
			return false, nil
		}
		errStr := err.Error()
		// NOTE: The error string is a hardcode copied from Canal.checkBinlogRowFormat,
		// review this if the source code of canal has been updated.
		if strings.Contains(errStr, "binlog must ROW format") {
			return false, err
		}

		logger.Warning(sc.context.GetRuntimeContext(), "CANAL_START_ALARM", "newCanal failed, error", err, "retry it")
		if util.RandomSleep(time.Second*5, 0.1, sc.shutdown) {
			return true, nil
		}
	}
}

// Start starts the ServiceInput's service, whatever that may be
func (sc *ServiceCanal) Start(c pipeline.Collector) error {
	sc.lastErrorCount = 0
	sc.shutdown = make(chan struct{}, 1)
	sc.waitGroup.Add(1)
	defer sc.waitGroup.Done()
	sc.collector = c

	shouldShutdown, err := sc.newCanal()
	if err != nil {
		logger.Error(sc.context.GetRuntimeContext(),
			"CANAL_START_ALARM", "service_canal plugin only supports ROW mode", err)
		return err
	}
	if shouldShutdown {
		logger.Info(sc.context.GetRuntimeContext(), "service_canal quit because shutdown is signaled during newCanal")
		return nil
	}

	// Check if the GTID mode is available on server, if not, use binlog-file mode.
	// Although we have added retry logic here, we can not deal with such case:
	// sc.canal turns into invalid. In such case, this loop will run continuously and
	// user will receive alarms (rare case, ignore it now).
	for {
		shouldRetry := false
		sc.isGTIDEnabled, shouldRetry, err = sc.getGTIDMode()
		if nil == err {
			break
		}
		err = fmt.Errorf("Check GTID mode failed, error: %v", err)
		logger.Warning(sc.context.GetRuntimeContext(), "CANAL_START_ALARM", err.Error())
		if shouldRetry {
			if util.RandomSleep(time.Second*5, 0.1, sc.shutdown) {
				sc.canal.Close()
				logger.Info(sc.context.GetRuntimeContext(), "service_canal quit because shutdown during getGTIDMode")
				return nil
			}
		}
	}
	if sc.isGTIDEnabled {
		logger.Info(sc.context.GetRuntimeContext(), "GTID mode is enabled, use it as checkpoint")
	} else {
		logger.Info(sc.context.GetRuntimeContext(), "GTID mode is not supported or disabled, use binlog-file as checkpoint")
	}

	// Initialize checkpoint, if server does not support or disable GTID mode,
	// clear GTID in checkpoint.
	sc.initCheckPoint()
	if !sc.isGTIDEnabled && sc.checkpoint.GTID != "" {
		sc.checkpoint.GTID = ""
	}
	logger.Infof(sc.context.GetRuntimeContext(), "Checkpoint initialized: %v", sc.checkpoint)

	// Construct start synchronization position according to GTID or binlog-file.
	// Start with GTID if it exists and is valid, otherwise use binlog-file to
	// construct synchronization position.
	// If both of them are missing, get from server if StartFromBegining is not set.
	var gtid mysql.GTIDSet
	var startPos mysql.Position
	if sc.checkpoint.GTID != "" {
		gtid, err = mysql.ParseGTIDSet(sc.Flavor, sc.checkpoint.GTID)
		if err != nil {
			logger.Error(sc.context.GetRuntimeContext(), "CANAL_START_ALARM", "Parse GTID error, clear it",
				sc.checkpoint.GTID, err)
			gtid = nil
			sc.checkpoint.GTID = ""
		}
	}
	if nil == gtid && sc.checkpoint.FileName != "" {
		startPos.Name = sc.checkpoint.FileName
		startPos.Pos = sc.checkpoint.Offset
	}
	if nil == gtid && 0 == len(startPos.Name) && !sc.StartFromBegining {
		gtid, err = sc.getLatestGTID()
		if err != nil {
			logger.Warning(sc.context.GetRuntimeContext(), "CANAL_START_ALARM", "Call getLatestGTID failed, error", err)
			startPos = sc.GetBinlogLatestPos()
		}
		logger.Infof(sc.context.GetRuntimeContext(), "Get latest checkpoint GTID: %v Position: %v", gtid, startPos)
	}

	if gtid != nil {
		go sc.runCanalByGTID(gtid)
	} else {
		go sc.runCanal(startPos)
	}

ForBlock:
	for {
		select {
		case <-sc.shutdown:
			sc.canal.Close() // will trigger OnPosSynced with force=true
			logger.Info(sc.context.GetRuntimeContext(), "service_canal quit because of shutting down, checkpoint",
				sc.checkpoint)
			<-sc.lastErrorChan
			return nil
		case err = <-sc.lastErrorChan:
			sc.canal.Close() // will trigger OnPosSynced with force=true

			if nil == err {
				logger.Info(sc.context.GetRuntimeContext(), "Canal returns normally, break loop")
				break ForBlock
			}

			// Sleep a while and process error.
			if util.RandomSleep(time.Second*5, 0.1, sc.shutdown) {
				logger.Info(sc.context.GetRuntimeContext(), "Shutdown is signaled during sleep, break loop")
				break ForBlock
			}
			errStr := err.Error()
			logger.Error(sc.context.GetRuntimeContext(), "CANAL_RUNTIME_ALARM", "Restart canal because of error", err)

			// Get latest position from server and restart.
			//
			// Risk of losing data: before plugin reconnects to new master after HA switching,
			// more requests were processed and gtid_executed has been updated on new master.
			// In such case, there is a gap between local checkpoint and latest checkpoint got
			// from server (data lossing).
			//
			// TODO: instead of getting from server, use local checkpoint.
			// This solution has to deal with cases that local checkpoint is invalid to
			// avoid infinite looping.
			// In detail, if user's configuration is wrong or local checkpoint is corrupted,
			// canal will return error and enter this logic again.
			var gtid mysql.GTIDSet
			var startPos mysql.Position
			if strings.Contains(errStr, "ERROR 1236") {
				logger.Infof(sc.context.GetRuntimeContext(), "Reset binlog with config %v, GTID mode status: %v",
					sc.config, sc.isGTIDEnabled)
				if sc.isGTIDEnabled {
					gtid, err = sc.getLatestGTID()
					if err != nil {
						logger.Warning(sc.context.GetRuntimeContext(), "CANAL_RUNTIME_ALARM",
							"getLatestGTID failed duration restarting", err)
					}
				}
				if nil == gtid {
					startPos = sc.GetBinlogLatestPos()
				}
			}
			shouldShutdown, err = sc.newCanal()
			if err != nil {
				logger.Info(sc.context.GetRuntimeContext(), "newCanal returns error, break loop", err)
				break ForBlock
			}
			if shouldShutdown {
				logger.Info(sc.context.GetRuntimeContext(), "Shutdown is signaled during newCanal, break loop")
				break ForBlock
			}
			if gtid != nil {
				go sc.runCanalByGTID(gtid)
			} else {
				go sc.runCanal(startPos)
			}
			continue ForBlock
		}
	}
	logger.Info(sc.context.GetRuntimeContext(), "Break from loop, waiting for shutting down, checkpoint", sc.checkpoint)
	<-sc.shutdown
	return err
}

// Stop stops the services and closes any necessary channels and connections
func (sc *ServiceCanal) Stop() error {
	close(sc.shutdown)
	logger.Info(sc.context.GetRuntimeContext(), "canal stop", "wait")
	sc.waitGroup.Wait()
	logger.Info(sc.context.GetRuntimeContext(), "canal stop", "done")
	return nil
}

func NewServiceCanal() *ServiceCanal {
	return &ServiceCanal{Host: "127.0.0.1",
		Port:              3306,
		User:              "root",
		Flavor:            "mysql",
		ServerID:          125,
		HeartBeatPeriod:   60,
		ReadTimeout:       90,
		EnableGTID:        true,
		EnableInsert:      true,
		EnableUpdate:      true,
		EnableDelete:      true,
		EnableEventMeta:   false,
		Charset:           mysql.DEFAULT_CHARSET,
		ByteValueToString: true,
		SetToString:       true,
		PackValues:        false,
	}
}

func init() {
	pipeline.ServiceInputs["service_canal"] = func() pipeline.ServiceInput {
		return NewServiceCanal()
	}

	canalLog.AddHook(&LogCanal{})
}
