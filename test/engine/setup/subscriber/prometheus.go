// Copyright 2026 iLogtail Authors
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
	"fmt"
	"io"
	"net/http"
	"net/url"
	"time"

	"github.com/mitchellh/mapstructure"

	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const prometheusName = "prometheus"

type PrometheusSubscriber struct {
	PrometheusURL string `mapstructure:"prometheus_url" comment:"prometheus base url, e.g. http://prometheus:9090"`
	Query         string `mapstructure:"query" comment:"promql query, default agent_cpu"`
}

func (p *PrometheusSubscriber) Name() string {
	return prometheusName
}

func (p *PrometheusSubscriber) Description() string {
	return "prometheus subscriber queries instant vectors via HTTP API"
}

func (p *PrometheusSubscriber) GetData(_ string, _ int32) ([]*protocol.LogGroup, error) {
	if p.Query == "" {
		p.Query = "agent_cpu"
	}
	host, err := TryReplacePhysicalAddress(p.PrometheusURL)
	if err != nil {
		return nil, err
	}
	queryURL := fmt.Sprintf("%s/api/v1/query?query=%s", host, url.QueryEscape(p.Query))
	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Get(queryURL)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, err
	}
	var parsed struct {
		Status string `json:"status"`
		Data   struct {
			ResultType string `json:"resultType"`
			Result     []struct {
				Metric map[string]string `json:"metric"`
				Value  []interface{}     `json:"value"`
			} `json:"result"`
		} `json:"data"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return nil, err
	}
	if parsed.Status != "success" || len(parsed.Data.Result) == 0 {
		return []*protocol.LogGroup{{Logs: []*protocol.Log{}}}, nil
	}
	logGroup := &protocol.LogGroup{Logs: make([]*protocol.Log, 0, len(parsed.Data.Result))}
	for _, r := range parsed.Data.Result {
		log := &protocol.Log{Contents: make([]*protocol.Log_Content, 0, 8)}
		if name, ok := r.Metric["__name__"]; ok {
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: "__name__", Value: name})
		}
		for k, v := range r.Metric {
			if k == "__name__" {
				continue
			}
			log.Contents = append(log.Contents, &protocol.Log_Content{Key: k, Value: v})
		}
		if len(r.Value) >= 2 {
			if valStr, ok := r.Value[1].(string); ok {
				log.Contents = append(log.Contents, &protocol.Log_Content{Key: "__value__", Value: valStr})
			}
		}
		logGroup.Logs = append(logGroup.Logs, log)
	}
	logger.Debugf(context.Background(), "prometheus subscriber got %d samples for query %s", len(logGroup.Logs), p.Query)
	return []*protocol.LogGroup{logGroup}, nil
}

func (p *PrometheusSubscriber) FlusherConfig() string {
	return ""
}

func (p *PrometheusSubscriber) Stop() error {
	return nil
}

func init() {
	RegisterCreator(prometheusName, func(spec map[string]interface{}) (Subscriber, error) {
		p := &PrometheusSubscriber{}
		if err := mapstructure.Decode(spec, p); err != nil {
			return nil, err
		}
		if p.PrometheusURL == "" {
			return nil, fmt.Errorf("prometheus_url must not be empty")
		}
		return p, nil
	})
	doc.Register("subscriber", prometheusName, new(PrometheusSubscriber))
}
