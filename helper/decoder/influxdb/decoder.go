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
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/helper"
	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/influxdata/influxdb/models"
)

const (
	metricNameKey = "__name__"
	labelsKey     = "__labels__"
	timeNanoKey   = "__time_nano__"
	valueKey      = "__value__"
	typeKey       = "__type__"
	fieldNameKey  = "__field__"
)

const (
	valueTypeFloat  = "float"
	valueTypeInt    = "int"
	valueTypeBool   = "bool"
	valueTypeString = "string"
)

const tagDB = "__tag__:db"

// Decoder impl
type Decoder struct {
	FieldsExtend bool
}

func (d *Decoder) Decode(data []byte, req *http.Request) (logs []*protocol.Log, decodeErr error) {
	precision := req.FormValue("precision")
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

	logs = d.parsePointsToLogs(points, req)

	return logs, err
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

func (d *Decoder) parsePointsToLogs(points []models.Point, req *http.Request) []*protocol.Log {
	db := req.FormValue("db")
	contentLen := 4
	if d.FieldsExtend && len(db) > 0 {
		contentLen++
	}
	if d.FieldsExtend {
		contentLen += 2
	}

	logs := make([]*protocol.Log, 0, len(points))
	for _, s := range points {
		fields, err := s.Fields()
		if err != nil {
			continue
		}
		var valueType = valueTypeFloat
		var value string
		for field, v := range fields {
			switch v := v.(type) {
			case float64:
				value = strconv.FormatFloat(v, 'g', -1, 64)
			case int64:
				value = strconv.FormatInt(v, 10)
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

			if !d.FieldsExtend {
				helper.ReplaceInvalidChars(&name)
			}
			var builder strings.Builder
			for index, v := range s.Tags() {
				if index != 0 {
					builder.WriteByte('|')
				}
				if !d.FieldsExtend {
					key := string(v.Key)
					helper.ReplaceInvalidChars(&key)
					builder.WriteString(key)
				} else {
					builder.Write(v.Key)
				}
				builder.WriteString("#$#")
				builder.WriteString(string(v.Value))
			}

			contents := make([]*protocol.Log_Content, 0, contentLen)
			contents = append(contents, &protocol.Log_Content{
				Key:   metricNameKey,
				Value: name,
			}, &protocol.Log_Content{
				Key:   labelsKey,
				Value: builder.String(),
			}, &protocol.Log_Content{
				Key:   timeNanoKey,
				Value: strconv.FormatInt(s.UnixNano(), 10),
			}, &protocol.Log_Content{
				Key:   valueKey,
				Value: value,
			})
			if d.FieldsExtend {
				contents = append(contents, &protocol.Log_Content{
					Key:   typeKey,
					Value: valueType,
				}, &protocol.Log_Content{
					Key:   fieldNameKey,
					Value: field,
				})
			}
			if d.FieldsExtend && len(db) > 0 {
				contents = append(contents, &protocol.Log_Content{
					Key:   tagDB,
					Value: db,
				})
			}

			log := &protocol.Log{
				Time:     uint32(s.Time().Unix()),
				Contents: contents,
			}
			logs = append(logs, log)

		}
	}
	return logs
}
