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

package statsd

import (
	"bytes"
	"context"
	"fmt"
	"net/http"
	"sync"
	"time"

	dogstatsd "github.com/narqo/go-dogstatsd-parser"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/selfmonitor"
)

type Decoder struct {
	Time time.Time
	mu   sync.Mutex
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) ([]byte, int, error) {
	return common.CollectBody(res, req, maxBodySize)
}

func (d *Decoder) Decode(data []byte, _ *http.Request) ([]*models.PipelineGroupEvents, error) {
	timestamp := time.Now().UnixNano()
	var events []models.PipelineEvent
	d.parseMetrics(data, func(metric *dogstatsd.Metric, value float64) {
		if metric.Tags == nil {
			metric.Tags = make(map[string]string)
		}
		event := models.NewSingleValueMetric(metric.Name, models.MetricTypeUntyped, models.NewTagsWithMap(metric.Tags), timestamp, value)
		events = append(events, event)
	})
	if len(events) == 0 {
		return nil, nil
	}
	return []*models.PipelineGroupEvents{{
		Group:  models.NewGroup(models.NewMetadata(), models.NewTags()),
		Events: events,
	}}, nil
}

func (d *Decoder) parseMetrics(data []byte, emit func(*dogstatsd.Metric, float64)) {
	for _, packet := range bytes.Split(data, []byte("\n")) {
		line := bytes.TrimSpace(packet)
		if len(line) == 0 {
			continue
		}
		// The parser expects a first byte on every option delimited by '|'.
		if line[len(line)-1] == '|' || bytes.Contains(line, []byte("||")) {
			d.reportParseError(fmt.Errorf("empty statsd metric option"))
			continue
		}
		metric, err := dogstatsd.Parse(string(line))
		if err != nil {
			d.reportParseError(err)
			continue
		}
		switch value := metric.Value.(type) {
		case float64:
			emit(metric, value)
		case int64:
			emit(metric, float64(value))
		default:
			d.reportParseError(fmt.Errorf("unsupported statsd metric value type %T", value))
		}
	}
}

func (d *Decoder) reportParseError(err error) {
	logger.Debug(context.Background(), "parse statsd error", err)
	d.mu.Lock()
	defer d.mu.Unlock()
	if time.Since(d.Time) > 10*time.Second {
		logger.Warning(context.Background(), selfmonitor.StatsdParseAlarm, "parse err", err)
		d.Time = time.Now()
	}
}
