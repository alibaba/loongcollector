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
	"log"

	"strings"

	"github.com/elastic/go-elasticsearch/v8"
	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const elasticSearchName = "elasticsearch"

var r map[string]interface{}

type ElasticSearchSubscriber struct {
	Address  string `mapstructure:"address" comment:"the elasticsearch address"`
	Username string `mapstructure:"username" comment:"the elasticsearch username"`
	Password string `mapstructure:"password" comment:"the elasticsearch password"`
	Index    string `mapstructure:"index" comment:"the elasticsearch index name to query from"`

	client        *elasticsearch.Client
	lastTimestamp int64
}

func (i *ElasticSearchSubscriber) Name() string {
	return elasticSearchName
}

func (i *ElasticSearchSubscriber) Description() string {
	return "this's a elasticsearch subscriber, which will query inserted records from elasticsearch periodically."
}

func (i *ElasticSearchSubscriber) GetData(startTime int32) ([]*protocol.LogGroup, error) {
	host, err := TryReplacePhysicalAddress(i.Address)
	if err != nil {
		return nil, err
	}
	host = strings.ReplaceAll(host, "http://", "")

	cfg := elasticsearch.Config{
		Addresses: []string{i.Address},
		Username:  i.Username,
		Password:  i.Password,
	}
	es, err := elasticsearch.NewClient(cfg)
	if err != nil {
		logger.Warningf(context.Background(), "ELASTICSEARCH_SUBSCRIBER_ALARM", "failed to create elasticsearch client, host %, err: %s", host, err)
		return nil, err
	}
	i.client = es
	i.lastTimestamp = int64(startTime)
	logGroup, err := i.queryRecords()
	return []*protocol.LogGroup{logGroup}, err
}

func (i *ElasticSearchSubscriber) FlusherConfig() string {
	return ""
}

func (i *ElasticSearchSubscriber) Stop() error {
	return nil
}

func (i *ElasticSearchSubscriber) queryRecords() (logGroup *protocol.LogGroup, err error) {
	logGroup = &protocol.LogGroup{
		Logs: []*protocol.Log{},
	}
	// s := fmt.Sprintf(clickhouseQuerySQL, i.Database, i.Table, i.lastTimestamp)
	// rows, err := i.client.Query(context.Background(), s)

	res, err := i.client.Search(
		i.client.Search.WithContext(context.Background()),
		i.client.Search.WithIndex(i.Index),
		i.client.Search.WithBody(strings.NewReader(`{"query" : { "match_all" : { } }}`)),
	)
	if err != nil {
		logger.Warning(context.Background(), "ELASTICSEARCH_SUBSCRIBER_ALARM", "err", err)
		return
	}
	if err = json.NewDecoder(res.Body).Decode(&r); err != nil {
		log.Fatalf("Error parsing the response body: %s", err)
	}

	type logContent struct {
		Contents struct {
			Index   string `json:"Index"`
			Content string `json:"Content"`
		} `json:"contents"`
		Tags struct {
			HostIP   string `json:"host.ip"`
			HostName string `json:"host.name"`
		} `json:"tags"`
		Time int `json:"time"`
	}
	if _, ok := r["hits"]; !ok {
		return nil, fmt.Errorf("no hits found in response")
	}
	if _, ok := r["hits"].(map[string]interface{})["hits"]; !ok {
		return nil, fmt.Errorf("no hits found in response")
	}
	for _, hit := range r["hits"].(map[string]interface{})["hits"].([]interface{}) {
		tmpJSON, _ := json.Marshal(hit.(map[string]interface{})["_source"])
		lc := logContent{}
		err = json.Unmarshal(tmpJSON, &lc)
		if err != nil {
			logger.Warning(context.Background(), "ELASTICSEARCH_SUBSCRIBER_ALARM", "err", err)
		}

		log := &protocol.Log{
			Contents: []*protocol.Log_Content{
				{Key: "index", Value: lc.Contents.Index},
				{Key: "content", Value: lc.Contents.Content},
			},
		}
		logGroup.Logs = append(logGroup.Logs, log)
	}
	return
}

func init() {
	RegisterCreator(elasticSearchName, func(spec map[string]interface{}) (Subscriber, error) {
		i := &ElasticSearchSubscriber{}
		if err := mapstructure.Decode(spec, i); err != nil {
			return nil, err
		}

		if i.Address == "" {
			return nil, errors.New("addr must not be empty")
		}
		if i.Username == "" {
			return nil, errors.New("username must not be empty")
		}
		if i.Password == "" {
			return nil, errors.New("password must not be empty")
		}
		return i, nil
	})
	doc.Register("subscriber", elasticSearchName, new(ElasticSearchSubscriber))
}
