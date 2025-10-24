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

package subscriber

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"strings"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const dorisName = "doris"
const dorisQuerySQL = "select time, content, value from `%s`.`%s` where time > %v order by time limit 100"

type DorisSubscriber struct {
	Address     string `mapstructure:"address" comment:"the doris FE address (format: http://host:port)"`
	Username    string `mapstructure:"username" comment:"the doris username"`
	Password    string `mapstructure:"password" comment:"the doris password"`
	Database    string `mapstructure:"database" comment:"the doris database name to query from"`
	Table       string `mapstructure:"table" comment:"the doris table name to query from"`
	CreateTable bool   `mapstructure:"create_table" comment:"if create the table, default is true"`

	client        *sql.DB
	lastTimestamp int64
}

func (d *DorisSubscriber) Name() string {
	return dorisName
}

func (d *DorisSubscriber) Description() string {
	return "this's a doris subscriber, which will query inserted records from doris periodically."
}

func (d *DorisSubscriber) GetData(sqlStr string, startTime int32) ([]*protocol.LogGroup, error) {
	host, err := TryReplacePhysicalAddress(d.Address)
	if err != nil {
		return nil, err
	}

	// Parse address to get host and port
	// Format: http://host:port or https://host:port
	host = strings.TrimPrefix(host, "http://")
	host = strings.TrimPrefix(host, "https://")

	// Doris uses MySQL protocol on port 9030 for query
	// But the address provided is typically the HTTP port (8030)
	// We need to replace the port with 9030 for MySQL protocol connection
	parts := strings.Split(host, ":")
	mysqlHost := parts[0]
	mysqlPort := "9030" // Default Doris MySQL protocol port

	// Create DSN (Data Source Name) for MySQL connection
	// Format: username:password@tcp(host:port)/database
	// Use default test password if not specified
	password := d.Password
	if password == "" {
		password = "test_password"
	}
	dsn := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?parseTime=true&timeout=10s",
		d.Username, password, mysqlHost, mysqlPort, d.Database)

	db, err := sql.Open("mysql", dsn)
	if err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"failed to connect to doris, host: %s, err: %s", host, err)
		return nil, err
	}

	// Set connection pool parameters
	db.SetMaxOpenConns(5)
	db.SetMaxIdleConns(2)
	db.SetConnMaxLifetime(time.Duration(10) * time.Minute)

	// Test connection
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"failed to ping doris, err: %s", err)
		db.Close()
		return nil, err
	}

	d.client = db
	d.lastTimestamp = int64(startTime)

	// Create table if needed
	if d.CreateTable {
		if err := d.createTable(); err != nil {
			logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
				"failed to create table, err: %s", err)
			// Don't return error, table might already exist
		}
	}

	logGroup, err := d.queryRecords()
	if err != nil {
		logger.Warning(context.Background(), "DORIS_SUBSCRIBER_ALARM", "err", err)
		return nil, err
	}
	return []*protocol.LogGroup{logGroup}, nil
}

func (d *DorisSubscriber) FlusherConfig() string {
	return ""
}

func (d *DorisSubscriber) Stop() error {
	if d.client != nil {
		return d.client.Close()
	}
	return nil
}

func (d *DorisSubscriber) createTable() error {
	// Create database if not exists
	createDB := fmt.Sprintf("CREATE DATABASE IF NOT EXISTS `%s`", d.Database)
	if _, err := d.client.Exec(createDB); err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"failed to create database, sql: %s, err: %s", createDB, err)
		return err
	}

	// Create table for testing with custom_single_flatten protocol
	// The table will have columns for time and common test fields
	tableName := fmt.Sprintf("`%s`.`%s`", d.Database, d.Table)
	createTableSQL := fmt.Sprintf(`
		CREATE TABLE IF NOT EXISTS %s (
			time BIGINT,
			content STRING,
			value STRING,
			__tag__hostip STRING,
			__tag__hostname STRING
		) DUPLICATE KEY(time)
		DISTRIBUTED BY HASH(time) BUCKETS 1
		PROPERTIES (
			"replication_num" = "1"
		)`, tableName)

	if _, err := d.client.Exec(createTableSQL); err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"failed to create table, sql: %s, err: %s", createTableSQL, err)
		return err
	}

	logger.Infof(context.Background(), "created doris table: %s", tableName)
	return nil
}

func (d *DorisSubscriber) queryRecords() (logGroup *protocol.LogGroup, err error) {
	logGroup = &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}

	query := fmt.Sprintf(dorisQuerySQL, d.Database, d.Table, d.lastTimestamp)
	logger.Debugf(context.Background(), "doris subscriber query: %s", query)

	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()

	rows, err := d.client.QueryContext(ctx, query)
	if err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"failed to query doris, query: %s, err: %s", query, err)
		return
	}
	defer rows.Close()

	for rows.Next() {
		var (
			timestamp int64
			content   sql.NullString
			value     sql.NullString
		)
		if err = rows.Scan(&timestamp, &content, &value); err != nil {
			logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
				"failed to scan row, err: %s", err)
			return
		}

		log := &protocol.Log{
			Time: uint32(timestamp),
		}

		// Add content field
		if content.Valid {
			log.Contents = append(log.Contents, &protocol.Log_Content{
				Key:   "content",
				Value: content.String,
			})
		}

		// Add value field
		if value.Valid {
			log.Contents = append(log.Contents, &protocol.Log_Content{
				Key:   "value",
				Value: value.String,
			})
		}

		// Update last timestamp
		if timestamp > d.lastTimestamp {
			d.lastTimestamp = timestamp
		}

		logGroup.Logs = append(logGroup.Logs, log)
	}

	if err = rows.Err(); err != nil {
		logger.Warningf(context.Background(), "DORIS_SUBSCRIBER_ALARM",
			"rows iteration error: %s", err)
		return
	}

	logger.Infof(context.Background(), "doris subscriber got %d logs", len(logGroup.Logs))
	return
}

func init() {
	RegisterCreator(dorisName, func(spec map[string]interface{}) (Subscriber, error) {
		d := &DorisSubscriber{
			CreateTable: true,
		}
		if err := mapstructure.Decode(spec, d); err != nil {
			return nil, err
		}

		if d.Address == "" {
			return nil, errors.New("address must not be empty")
		}
		if d.Database == "" {
			return nil, errors.New("database must not be empty")
		}
		if d.Table == "" {
			return nil, errors.New("table must not be empty")
		}
		// Username and password can be empty for some configurations
		return d, nil
	})
	doc.Register("subscriber", dorisName, new(DorisSubscriber))
}
