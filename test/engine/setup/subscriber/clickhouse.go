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
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"strings"
	"time"

	"github.com/ClickHouse/clickhouse-go/v2"
	"github.com/ClickHouse/clickhouse-go/v2/lib/driver"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const clickHouseName = "clickhouse"
const clickhouseQuerySQL = "select _timestamp,_log from `%s`.`ilogtail_%s_buffer` where _timestamp > %v order by _timestamp"

type ClickHouseSubscriber struct {
	Address     string `mapstructure:"address" comment:"the clickhouse address"`
	Username    string `mapstructure:"username" comment:"the clickhouse username"`
	Password    string `mapstructure:"password" comment:"the clickhouse password"`
	Database    string `mapstructure:"database" comment:"the clickhouse database name to query from"`
	Table       string `mapstructure:"table" comment:"the clickhouse table name to query from"`
	CreateTable bool   `mapstructure:"create_db" comment:"if create the database, default is true"`

	client        driver.Conn
	lastTimestamp int64
}

func (i *ClickHouseSubscriber) Name() string {
	return clickHouseName
}

func (i *ClickHouseSubscriber) Description() string {
	return "this's a clickhouse subscriber, which will query inserted records from clickhouse periodically."
}

func (i *ClickHouseSubscriber) GetData(sql string, startTime int32) ([]*protocol.LogGroup, error) {
	host, err := TryReplacePhysicalAddress(i.Address)
	if err != nil {
		return nil, err
	}
	host = strings.ReplaceAll(host, "http://", "")
	conn, err := clickhouse.Open(&clickhouse.Options{
		Addr: []string{host},
		Auth: clickhouse.Auth{
			Database: i.Database,
		},
		DialContext: func(ctx context.Context, addr string) (net.Conn, error) {
			var d net.Dialer
			return d.DialContext(ctx, "tcp", addr)
		},
		Settings: clickhouse.Settings{
			"max_execution_time": 60,
		},
		Compression: &clickhouse.Compression{
			Method: clickhouse.CompressionLZ4,
		},
		DialTimeout:      time.Duration(10) * time.Second,
		MaxOpenConns:     5,
		MaxIdleConns:     5,
		ConnMaxLifetime:  time.Duration(10) * time.Minute,
		ConnOpenStrategy: clickhouse.ConnOpenInOrder,
		BlockBufferSize:  10,
	})
	if err != nil {
		logger.Warningf(context.Background(), "CLICKHOUSE_SUBSCRIBER_ALARM", "failed to create clickhouse conn, host %, err: %s", host, err)
		return nil, err
	}
	i.client = conn
	i.lastTimestamp = int64(startTime)
	logGroup, err := i.queryRecords()
	if err != nil {
		logger.Warning(context.Background(), "CLICKHOUSE_SUBSCRIBER_ALARM", "err", err)
		return nil, err
	}
	return []*protocol.LogGroup{logGroup}, nil
}

func (i *ClickHouseSubscriber) FlusherConfig() string {
	return ""
}

func (i *ClickHouseSubscriber) Stop() error {
	if i.client != nil {
		return i.client.Close()
	}
	return nil
}

func (i *ClickHouseSubscriber) queryRecords() (logGroup *protocol.LogGroup, err error) {
	logGroup = &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}
	s := fmt.Sprintf(clickhouseQuerySQL, i.Database, i.Table, i.lastTimestamp)
	rows, err := i.client.Query(context.Background(), s)
	if err != nil {
		logger.Warning(context.Background(), "CLICKHOUSE_SUBSCRIBER_ALARM", "err", err)
		return
	}
	logger.Debug(context.Background(), "sql", s)

	type logContent struct {
		Contents struct {
			Index string `json:"Index"`
			Name  string `json:"_name"`
			Value string `json:"_value"`
		} `json:"contents"`
		Tags struct {
			HostIP   string `json:"host.ip"`
			HostName string `json:"host.name"`
		} `json:"tags"`
		Time int `json:"time"`
	}

	defer func() { _ = rows.Close() }()
	for rows.Next() {
		var (
			t int64
			l string
		)
		if err = rows.Scan(&t, &l); err != nil {
			logger.Warning(context.Background(), "CLICKHOUSE_SUBSCRIBER_ALARM", "failed to read clickhouse data, err", err)
			return
		}
		log := &protocol.Log{}
		lc := logContent{}
		if err = json.Unmarshal([]byte(l), &lc); err != nil {
			logger.Warning(context.Background(), "CLICKHOUSE_SUBSCRIBER_ALARM", "failed to unmarshal data, err", err)
			return
		}
		log.Contents = append(log.Contents, &protocol.Log_Content{
			Key:   "_name",
			Value: lc.Contents.Name,
		})
		log.Contents = append(log.Contents, &protocol.Log_Content{
			Key:   "_value",
			Value: lc.Contents.Value,
		})
		logGroup.Logs = append(logGroup.Logs, log)
	}
	return
}

func init() {
	RegisterCreator(clickHouseName, func(spec map[string]interface{}) (Subscriber, error) {
		i := &ClickHouseSubscriber{
			CreateTable: true,
		}
		if err := mapstructure.Decode(spec, i); err != nil {
			return nil, err
		}

		if i.Address == "" {
			return nil, errors.New("addr must not be empty")
		}
		if i.Database == "" {
			return nil, errors.New("database must not be empty")
		}
		if i.Table == "" {
			return nil, errors.New("table must no be empty")
		}
		return i, nil
	})
	doc.Register("subscriber", clickHouseName, new(ClickHouseSubscriber))
}
