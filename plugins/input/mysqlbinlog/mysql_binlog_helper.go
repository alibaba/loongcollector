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

package mysqlbinlog

import (
	"encoding/hex"
	"fmt"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/pipeline"

	"github.com/go-mysql-org/go-mysql/replication"
)

func binLogEventToSlsLog(event *replication.BinlogEvent, logData map[string]string, collector pipeline.Collector) error {
	switch event.Header.EventType {
	case replication.ROTATE_EVENT:
		if rotateEvent, cvt := event.Event.(*replication.RotateEvent); cvt {
			logData["rotate_file"] = string(rotateEvent.NextLogName)
			logData["start_position"] = strconv.Itoa(int(rotateEvent.Position))
		}
	case replication.FORMAT_DESCRIPTION_EVENT:
		if descEvent, ok := event.Event.(*replication.FormatDescriptionEvent); ok {
			logData["version"] = strconv.Itoa(int(descEvent.Version))
			logData["server"] = string(descEvent.ServerVersion)
			logData["check_sum_algorithm"] = strconv.Itoa(int(descEvent.ChecksumAlgorithm))
		}
	case replication.QUERY_EVENT:
		if qEvent, ok := event.Event.(*replication.QueryEvent); ok {
			logData["slave_proxy"] = strconv.Itoa(int(qEvent.SlaveProxyID))
			logData["execution_time"] = strconv.Itoa(int(qEvent.ExecutionTime))
			logData["error_code"] = strconv.Itoa(int(qEvent.ErrorCode))
			logData["schema"] = string(qEvent.Schema)
			logData["query"] = string(qEvent.Query)
			if qEvent.GSet != nil {
				logData["gtid_set"] = qEvent.GSet.String()
			}
		}
	case replication.XID_EVENT:
		if e, ok := event.Event.(*replication.XIDEvent); ok {
			logData["xid"] = strconv.Itoa(int(e.XID))
			if e.GSet != nil {
				logData["gtid_set"] = e.GSet.String()
			}
		}
	case replication.TABLE_MAP_EVENT:
		if e, ok := event.Event.(*replication.TableMapEvent); ok {
			logData["table_id"] = strconv.Itoa(int(e.TableID))
			logData["table"] = string(e.Table)
			logData["flags"] = strconv.Itoa(int(e.Flags))
			logData["schema"] = string(e.Schema)
			logData["column_count"] = strconv.Itoa(int(e.ColumnCount))
			logData["column_type_hex"] = hex.EncodeToString(e.ColumnType)
		}
	case replication.WRITE_ROWS_EVENTv0,
		replication.UPDATE_ROWS_EVENTv0,
		replication.DELETE_ROWS_EVENTv0,
		replication.WRITE_ROWS_EVENTv1,
		replication.DELETE_ROWS_EVENTv1,
		replication.UPDATE_ROWS_EVENTv1,
		replication.WRITE_ROWS_EVENTv2,
		replication.UPDATE_ROWS_EVENTv2,
		replication.DELETE_ROWS_EVENTv2:
		if e, ok := event.Event.(*replication.RowsEvent); ok {
			if e.Table != nil {
				logData["table"] = string(e.Table.Table)
			}
			logData["table_id"] = strconv.Itoa(int(e.TableID))
			logData["flags"] = strconv.Itoa(int(e.Flags))
			logData["column_count"] = strconv.Itoa(int(e.ColumnCount))
			for i, rows := range e.Rows {
				if i > 10 {
					break
				}
				iStr := strconv.Itoa(i) + "_"
				for j, d := range rows {
					if _, ok := d.([]byte); ok {
						logData[iStr+strconv.Itoa(j)] = fmt.Sprintf("%q", d)
					} else {
						logData[iStr+strconv.Itoa(j)] = fmt.Sprintf("%#v", d)
					}
				}
			}
		}
	case replication.ROWS_QUERY_EVENT:
		if e, ok := event.Event.(*replication.RowsQueryEvent); ok {
			logData["query"] = string(e.Query)
		}
	case replication.GTID_EVENT:
		if e, ok := event.Event.(*replication.GTIDEvent); ok {
			logData["sid"] = string(e.SID)
			logData["gno"] = strconv.Itoa(int(e.GNO))
			logData["commit_flag"] = strconv.Itoa(int(e.CommitFlag))
		}
	case replication.BEGIN_LOAD_QUERY_EVENT:
		if e, ok := event.Event.(*replication.BeginLoadQueryEvent); ok {
			logData["file_id"] = strconv.Itoa(int(e.FileID))
			logData["block_data"] = string(e.BlockData)
		}
	case replication.EXECUTE_LOAD_QUERY_EVENT:
		if e, ok := event.Event.(*replication.ExecuteLoadQueryEvent); ok {
			logData["dum_handling_flags"] = strconv.Itoa(int(e.DupHandlingFlags))
			logData["end_pos"] = strconv.Itoa(int(e.EndPos))
			logData["error_code"] = strconv.Itoa(int(e.ErrorCode))
			logData["execution_time"] = strconv.Itoa(int(e.ExecutionTime))
			logData["file_id"] = strconv.Itoa(int(e.FileID))
			logData["schema_length"] = strconv.Itoa(int(e.SchemaLength))
			logData["slave_proxy_id"] = strconv.Itoa(int(e.SlaveProxyID))
			logData["start_pos"] = strconv.Itoa(int(e.StartPos))
			logData["status_vars"] = strconv.Itoa(int(e.StatusVars))
		}
	case replication.MARIADB_ANNOTATE_ROWS_EVENT:
		if e, ok := event.Event.(*replication.MariadbAnnotateRowsEvent); ok {
			logData["query"] = string(e.Query)
		}
	case replication.MARIADB_BINLOG_CHECKPOINT_EVENT:
		if e, ok := event.Event.(*replication.MariadbBinlogCheckPointEvent); ok {
			logData["info"] = string(e.Info)
		}
	case replication.MARIADB_GTID_LIST_EVENT:
		if e, ok := event.Event.(*replication.MariadbGTIDListEvent); ok {
			for i, gtids := range e.GTIDs {
				logData["gtid_"+strconv.Itoa(i)] = gtids.String()
			}
		}
	case replication.MARIADB_GTID_EVENT:
		if e, ok := event.Event.(*replication.MariadbGTIDEvent); ok {
			logData["gtid"] = e.GTID.String()
		}
	default:
		if e, ok := event.Event.(*replication.GenericEvent); ok {
			logData["data"] = hex.EncodeToString(e.Data)
		}
	}

	collector.AddData(nil, logData, time.Unix(int64(event.Header.Timestamp), 0))

	return nil
}
