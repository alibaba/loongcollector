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
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/mitchellh/mapstructure"
)

const lokiName = "loki"

type LokiSubscriber struct {
	Address         string            `mapstructure:"address" comment:"the loki address"`
	QueryIntervalMs int               `mapstructure:"query_interval_ms" comment:"interval between queries select upserts records from loki"`
	TargetLabels    map[string]string `mapstructure:"target_labels" comment:"interval between queries select upserts records from loki"`
	TenantID        string            `mapstructure:"tenant_id" comment:"tenant id of loki"`

	url           string
	query         string
	lastTimestamp int64
	channel       chan *protocol.LogGroup
	stopCh        chan struct{}
	client        http.Client
}

type logContent struct {
	Contents struct {
		Index string `json:"Index"`
		Value string `json:"value"`
	} `json:"contents"`
	Tags struct {
		HostIP   string `json:"host.ip"`
		HostName string `json:"host.name"`
		Name     string `json:"name"`
	} `json:"tags"`
	Time int `json:"time"`
}

type QueryResponse struct {
	Status string
	Data   QueryData
}

type QueryData struct {
	Result []QueryResult
}

type QueryResult struct {
	Stream map[string]string
	Values [][]string
}

func (l *LokiSubscriber) Name() string {
	return lokiName
}

func (l *LokiSubscriber) Description() string {
	return "this a loki subscriber, which is the default mock backend for Ilogtail."
}

func (l *LokiSubscriber) Start() error {
	host, err := TryReplacePhysicalAddress(l.Address)
	if err != nil {
		return err
	}
	if len(l.TargetLabels) == 0 {
		return errors.New("Loki label cannot be empty")
	}
	queries := make([]string, len(l.TargetLabels))
	i := 0
	for key, value := range l.TargetLabels {
		queries[i] = fmt.Sprintf("%s=\"%s\"", key, value)
		i++
	}
	l.url = fmt.Sprintf("%s/loki/api/v1/query_range", host)
	l.query = fmt.Sprintf("{%s}", strings.Join(queries, ", "))
	go l.runQueryRecordsTask()
	return nil
}

func (l *LokiSubscriber) Stop() {
	close(l.stopCh)
}

func (l *LokiSubscriber) SubscribeChan() <-chan *protocol.LogGroup {
	return l.channel
}

func (l *LokiSubscriber) FlusherConfig() string {
	return ""
}

func (l *LokiSubscriber) runQueryRecordsTask() {
	for {
		select {
		case <-l.stopCh:
			close(l.channel)
			return
		case <-time.After(time.Duration(l.QueryIntervalMs) * time.Millisecond):
			logGroup, maxTimestamp, err := l.queryRecords()
			if err != nil {
				logger.Warning(context.Background(), "LOKI_SUBSCRIBER_ALARM", "err", err)
				continue
			}
			logger.Debugf(context.Background(), "subscriber get new logs maxTimestamp: %d, count: %v", maxTimestamp, len(logGroup.Logs))
			l.channel <- logGroup
			if maxTimestamp > l.lastTimestamp {
				l.lastTimestamp = maxTimestamp
			}
		}
	}
}

func (l *LokiSubscriber) queryRecords() (logGroup *protocol.LogGroup, maxTimestamp int64, err error) {
	logGroup = &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}
	req, _ := http.NewRequest("GET", l.url, nil)
	if l.TenantID != "" {
		req.Header.Set("X-Scope-OrgID", l.TenantID)
	}
	q := req.URL.Query()
	q.Add("query", l.query)
	// Empty to fetch all the logs of the last past hour
	if l.lastTimestamp > 0 {
		q.Add("start", strconv.FormatInt(l.lastTimestamp, 10))
	}
	req.URL.RawQuery = q.Encode()
	resp, err := l.client.Do(req)
	if err != nil {
		return
	}
	defer resp.Body.Close()
	var queryResponse QueryResponse
	err = json.NewDecoder(resp.Body).Decode(&queryResponse)
	if err != nil {
		return
	}
	for _, result := range queryResponse.Data.Result {
		for _, value := range result.Values {
			log := &protocol.Log{}
			lc := logContent{}
			logTimestamp, _ := strconv.ParseInt(value[0], 10, 64)
			if logTimestamp > maxTimestamp {
				maxTimestamp = logTimestamp + 1
			}
			if err = json.Unmarshal([]byte(value[1]), &lc); err != nil {
				return
			}
			log.Contents = append(log.Contents, &protocol.Log_Content{
				Key:   "name",
				Value: lc.Tags.Name,
			})
			log.Contents = append(log.Contents, &protocol.Log_Content{
				Key:   "value",
				Value: lc.Contents.Value,
			})
			logGroup.Logs = append(logGroup.Logs, log)
		}
	}
	return
}

func init() {
	RegisterCreator(lokiName, func(spec map[string]interface{}) (Subscriber, error) {
		l := &LokiSubscriber{}
		if err := mapstructure.Decode(spec, l); err != nil {
			return nil, err
		}
		if l.Address == "" {
			return nil, errors.New("addr must not be empty")
		}
		if l.QueryIntervalMs <= 0 {
			l.QueryIntervalMs = 1000
		}
		l.channel = make(chan *protocol.LogGroup, 10)
		l.stopCh = make(chan struct{})
		return l, nil
	})
	doc.Register("subscriber", lokiName, new(LokiSubscriber))
}
