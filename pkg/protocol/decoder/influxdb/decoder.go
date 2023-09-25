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

package influxdb

import (
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/influxdata/influxdb/models"

	"github.com/alibaba/ilogtail/pkg/helper"
	imodels "github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	typeKey      = "__type__"
	fieldNameKey = "__field__"
)

const (
	valueTypeFloat  = "float"
	valueTypeInt    = "int"
	valueTypeBool   = "bool"
	valueTypeString = "string"
)

const tagDB = "__tag__:db"

const metadataKeyDB = "db"

const (
	formDataKeyDB        = "db"
	formDataKeyPrecision = "precision"
)

// Decoder impl
type Decoder struct {
	FieldsExtend bool
}

func (d *Decoder) Decode(data []byte, req *http.Request, tags map[string]string) (logs []*protocol.Log, decodeErr error) {
	points, err := d.decodeToInfluxdbPoints(data, req)
	if err != nil {
		return nil, err
	}
	logs = d.parsePointsToLogs(points, req)
	return logs, nil
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

func (d *Decoder) DecodeV2(data []byte, req *http.Request) ([]*imodels.PipelineGroupEvents, error) {
	points, err := d.decodeToInfluxdbPoints(data, req)
	if err != nil {
		return nil, err
	}
	return d.parsePointsToEvents(points, req)
}

func (d *Decoder) decodeToInfluxdbPoints(data []byte, req *http.Request) ([]models.Point, error) {
	precision := req.FormValue(formDataKeyPrecision)
	var points []models.Point
	var err error
	if precision != "" {
		points, err = models.ParsePointsWithPrecision(data, time.Now().UTC(), precision)
	} else {
		points, err = models.ParsePoints(data)
	}
	if err != nil {
		return nil, err
	}
	return points, nil
}

func (d *Decoder) parsePointsToEvents(points []models.Point, req *http.Request) ([]*imodels.PipelineGroupEvents, error) {
	group := &imodels.PipelineGroupEvents{
		Group:  imodels.NewGroup(imodels.NewMetadata(), imodels.NewTags()),
		Events: make([]imodels.PipelineEvent, 0, len(points)),
	}
	if db := req.FormValue(formDataKeyDB); len(db) > 0 {
		group.Group.Metadata.Add(metadataKeyDB, db)
	}
	for _, point := range points {
		tags := imodels.NewTags()
		point.ForEachTag(func(k, v []byte) bool {
			tags.Add(string(k), string(v)) // deep copy here
			return true
		})
		multiValue, typedValue, err := d.parsePointFieldsToMetricValues(point)
		if err != nil {
			return nil, err
		}
		metric := imodels.NewMetric(string(point.Name()), imodels.MetricTypeUntyped, tags, point.UnixNano(), multiValue, typedValue)
		group.Events = append(group.Events, metric)
	}
	return []*imodels.PipelineGroupEvents{group}, nil
}

func (d *Decoder) parsePointFieldsToMetricValues(p models.Point) (imodels.MetricValue, imodels.MetricTypedValues, error) {
	multiValue := imodels.NewMetricMultiValue()
	typedValue := imodels.NewMetricTypedValues()

	iter := p.FieldIterator()
	for iter.Next() {
		if len(iter.FieldKey()) == 0 {
			continue
		}
		switch iter.Type() {
		case models.Float:
			v, err := iter.FloatValue()
			if err != nil {
				return nil, nil, fmt.Errorf("unable to unmarshal field %s: %s", string(iter.FieldKey()), err)
			}
			multiValue.Add(string(iter.FieldKey()), v)
		case models.Integer:
			v, err := iter.IntegerValue()
			if err != nil {
				return nil, nil, fmt.Errorf("unable to unmarshal field %s: %s", string(iter.FieldKey()), err)
			}
			multiValue.Add(string(iter.FieldKey()), float64(v))
		case models.Unsigned:
			v, err := iter.UnsignedValue()
			if err != nil {
				return nil, nil, fmt.Errorf("unable to unmarshal field %s: %s", string(iter.FieldKey()), err)
			}
			multiValue.Add(string(iter.FieldKey()), float64(v))
		case models.String:
			typedValue.Add(string(iter.FieldKey()), &imodels.TypedValue{Type: imodels.ValueTypeString, Value: iter.StringValue()})
		case models.Boolean:
			v, err := iter.BooleanValue()
			if err != nil {
				return nil, nil, fmt.Errorf("unable to unmarshal field %s: %s", string(iter.FieldKey()), err)
			}
			typedValue.Add(string(iter.FieldKey()), &imodels.TypedValue{Type: imodels.ValueTypeBoolean, Value: v})
		}
	}
	return multiValue, typedValue, nil
}

func (d *Decoder) parsePointsToLogs(points []models.Point, req *http.Request) []*protocol.Log {
	db := req.FormValue("db")
	logs := make([]*protocol.Log, 0, len(points))
	for _, s := range points {
		fields, err := s.Fields()
		if err != nil {
			continue
		}
		var valueType string
		var value string
		for field, v := range fields {
			switch v := v.(type) {
			case float64:
				value = strconv.FormatFloat(v, 'g', -1, 64)
				valueType = valueTypeFloat
			case int64:
				value = strconv.FormatInt(v, 10)
				valueType = valueTypeInt
			case bool:
				if v {
					value = "1"
				} else {
					value = "0"
				}
				valueType = valueTypeBool
			case string:
				if !d.FieldsExtend {
					continue
				}
				value = v
				valueType = valueTypeString
			default:
				continue
			}

			var name string
			if field == "value" {
				name = string(s.Name())
			} else {
				name = string(s.Name()) + ":" + field
			}
			var labels helper.MetricLabels
			for _, v := range s.Tags() {
				labels.Append(util.ZeroCopyBytesToString(v.Key), util.ZeroCopyBytesToString(v.Value))
			}

			metricLog := helper.NewMetricLogStringVal(name, s.UnixNano(), value, &labels)
			if d.FieldsExtend {
				metricLog.Contents = append(metricLog.Contents,
					&protocol.Log_Content{Key: typeKey, Value: valueType},
					&protocol.Log_Content{Key: fieldNameKey, Value: field})
				if len(db) > 0 {
					metricLog.Contents = append(metricLog.Contents, &protocol.Log_Content{
						Key:   tagDB,
						Value: db,
					})

				}
			}
			logs = append(logs, metricLog)
		}
	}
	return logs
}
