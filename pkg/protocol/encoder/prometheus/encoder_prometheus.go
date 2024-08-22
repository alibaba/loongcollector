// Copyright 2024 iLogtail Authors
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

package prometheus

import (
	"context"
	"errors"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

const defaultSeriesLimit = 1000

var errNilOrZeroGroupEvents = errors.New("nil or zero group events")

type Option struct {
	SeriesLimit int // config for prometheus encoder
}

func NewPromEncoder(seriesLimit int) extensions.Encoder {
	return newPromEncoder(seriesLimit)
}

type Encoder struct {
	SeriesLimit int
}

func newPromEncoder(seriesLimit int) *Encoder {
	if seriesLimit <= 0 {
		seriesLimit = defaultSeriesLimit
	}

	return &Encoder{
		SeriesLimit: seriesLimit,
	}
}

func (p *Encoder) EncodeV1(logGroups *protocol.LogGroup) ([][]byte, error) {
	// TODO implement me
	return nil, nil
}

func (p *Encoder) EncodeBatchV1(logGroups []*protocol.LogGroup) ([][]byte, error) {
	// TODO implement me
	return nil, nil
}

func (p *Encoder) EncodeV2(groupEvents *models.PipelineGroupEvents) ([][]byte, error) {
	if groupEvents == nil || len(groupEvents.Events) == 0 {
		return nil, errNilOrZeroGroupEvents
	}

	var res [][]byte

	wr := getWriteRequest(p.SeriesLimit)
	defer putWriteRequest(wr)

	for _, event := range groupEvents.Events {
		if event == nil {
			logger.Debugf(context.Background(), "nil event")
			continue
		}

		if event.GetType() != models.EventTypeMetric {
			logger.Debugf(context.Background(), "event type (%s) not metric", event.GetName())
			continue
		}

		metricEvent, ok := event.(*models.Metric)
		if !ok {
			logger.Debugf(context.Background(), "assert metric event type (%s) failed", event.GetName())
			continue
		}

		wr.Timeseries = append(wr.Timeseries, genPromRemoteWriteTimeseries(metricEvent))
		if len(wr.Timeseries) >= p.SeriesLimit {
			res = append(res, marshalBatchTimeseriesData(wr))
			wr.Timeseries = wr.Timeseries[:0]
		}
	}

	if len(wr.Timeseries) > 0 {
		res = append(res, marshalBatchTimeseriesData(wr))
		wr.Timeseries = wr.Timeseries[:0]
	}

	return res, nil
}

func (p *Encoder) EncodeBatchV2(groupEventsSlice []*models.PipelineGroupEvents) ([][]byte, error) {
	if len(groupEventsSlice) == 0 {
		return nil, errNilOrZeroGroupEvents
	}

	var res [][]byte

	for _, groupEvents := range groupEventsSlice {
		bytes, err := p.EncodeV2(groupEvents)
		if err != nil {
			continue
		}

		res = append(res, bytes...)
	}

	if res == nil {
		return nil, errNilOrZeroGroupEvents
	}

	return res, nil
}
