// Copyright 2023 iLogtail Authors
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
	"regexp"
	"sort"
	"strconv"
	"strings"

	client "github.com/influxdata/influxdb1-client/v2"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const influxdbName = "influxdb"
const querySQL = "select time,* from %s where time > %v group by * order by time"

type InfluxdbSubscriber struct {
	DbHost      string `mapstructure:"db_host" comment:"the influxdb host address"`
	DbUsername  string `mapstructure:"db_username" comment:"the influxdb username"`
	DbPassword  string `mapstructure:"db_password" comment:"the influxdb password"`
	DbName      string `mapstructure:"db_name" comment:"the influxdb database name to query from"`
	Measurement string `mapstructure:"measurement" comment:"the measurement to query from"`
	CreateDb    bool   `mapstructure:"create_db" comment:"if create the database, default is true"`

	client        client.Client
	lastTimestamp int64
}

func (i *InfluxdbSubscriber) Name() string {
	return influxdbName
}

func (i *InfluxdbSubscriber) Description() string {
	return "this's a influxdb subscriber, which will query inserted records from influxdb periodically."
}

func (i *InfluxdbSubscriber) GetData(_ string, startTime int32) ([]*protocol.LogGroup, error) {
	host, err := TryReplacePhysicalAddress(i.DbHost)
	if err != nil {
		return nil, err
	}

	c, err := client.NewHTTPClient(client.HTTPConfig{
		Addr:     host,
		Username: i.DbUsername,
		Password: i.DbPassword,
	})
	if err != nil {
		return nil, fmt.Errorf("failed to create influxdb client: %s", err)
	}
	i.client = c
	i.lastTimestamp = int64(startTime)

	if i.CreateDb {
		err = i.createDatabase()
		if err != nil {
			logger.Warningf(context.Background(), "INFLUXDB_SUBSCRIBER_ALARM", "failed to create database %s, err: %s", i.DbName, err)
		}
	}

	sql := fmt.Sprintf(querySQL, i.Measurement, i.lastTimestamp)
	resp, err := i.client.Query(client.NewQuery(sql, i.DbName, "1"))
	if err != nil {
		logger.Warning(context.Background(), "INFLUXDB_SUBSCRIBER_ALARM", "err", err)
		return nil, err
	}
	if err = resp.Error(); err != nil {
		logger.Warning(context.Background(), "INFLUXDB_SUBSCRIBER_ALARM", "err", err)
		return nil, err
	}
	logGroup := i.queryRecords2LogGroup(resp.Results)
	logger.Debugf(context.Background(), "subscriber get new logs count: %v", len(logGroup.Logs))
	return []*protocol.LogGroup{logGroup}, nil
}

func (i *InfluxdbSubscriber) FlusherConfig() string {
	return ""
}

func (i *InfluxdbSubscriber) Stop() error {
	if i.client != nil {
		return i.client.Close()
	}
	return nil
}

func (i *InfluxdbSubscriber) createDatabase() error {
	sql := fmt.Sprintf("create database %s", i.DbName)
	resp, err := i.client.Query(client.NewQuery(sql, "", ""))
	if err != nil {
		return err
	}
	if err := resp.Error(); err != nil {
		return err
	}

	return nil
}

func (i *InfluxdbSubscriber) queryRecords2LogGroup(results []client.Result) (logGroup *protocol.LogGroup) {
	logGroup = &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}

	for _, result := range results {
		if len(result.Err) > 0 {
			continue
		}

		for _, serie := range result.Series {
			labels := i.buildLabels(serie.Tags)
			for _, values := range serie.Values {
				for _, field := range serie.Columns[1:] {
					for _, v := range values[1:] {
						if field == "time" {
							continue
						}

						var timestamp string
						switch t := values[0].(type) {
						case json.Number:
							timestamp = t.String()
						case float64:
							timestamp = strconv.FormatFloat(t, 'f', -1, 64)
						}

						log := &protocol.Log{
							Contents: []*protocol.Log_Content{
								{Key: "__name__", Value: i.buildName(serie.Name, field)},
								{Key: "__labels__", Value: labels},
								{Key: "__time_nano__", Value: timestamp},
							},
						}

						typ, val := i.buildValue(v)
						log.Contents = append(log.Contents, &protocol.Log_Content{
							Key:   "__value__",
							Value: val,
						}, &protocol.Log_Content{
							Key:   "__type__",
							Value: typ,
						})

						logGroup.Logs = append(logGroup.Logs, log)
					}
				}
			}
		}
	}
	return
}

func (i *InfluxdbSubscriber) buildName(measurement string, field string) string {
	if field == "value" {
		return measurement
	}
	return fmt.Sprintf("%s.%s", measurement, field)
}

func (i *InfluxdbSubscriber) buildLabels(tags map[string]string) string {
	labels := make([]string, 0, len(tags))
	for k, v := range tags {
		labels = append(labels, fmt.Sprintf("%s#$#%s", k, v))
	}
	sort.Strings(labels)
	return strings.Join(labels, "|")
}

func (i *InfluxdbSubscriber) buildValue(value interface{}) (typeStr, valueStr string) {
	switch v := value.(type) {
	case string:
		return "string", v
	case float64:
		return "float", strconv.FormatFloat(v, 'g', -1, 64)
	case json.Number:
		return "float", v.String()
	default:
		return "unknown", fmt.Sprintf("%v", value)
	}
}

func init() {
	RegisterCreator(influxdbName, func(spec map[string]interface{}) (Subscriber, error) {
		i := &InfluxdbSubscriber{
			CreateDb: true,
		}
		if err := mapstructure.Decode(spec, i); err != nil {
			return nil, err
		}

		if i.DbHost == "" {
			return nil, errors.New("db_host must not be empty")
		}
		if i.DbName == "" {
			return nil, errors.New("db_name must not be empty")
		}
		if i.Measurement == "" {
			return nil, errors.New("measurement must no be empty")
		}

		match, _ := regexp.Match("^http(s)?://", []byte(i.DbHost))
		if !match {
			i.DbHost = "http://" + i.DbHost
		}

		return i, nil
	})
	doc.Register("subscriber", influxdbName, new(InfluxdbSubscriber))
}
