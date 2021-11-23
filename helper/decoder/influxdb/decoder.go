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

	"github.com/alibaba/ilogtail/helper/decoder/common"
	"github.com/alibaba/ilogtail/pkg/protocol"

	"github.com/influxdata/influxdb/models"
)

const (
	metricNameKey = "__name__"
	labelsKey     = "__labels__"
	timeNanoKey   = "__time_nano__"
	valueKey      = "__value__"
)

// Decoder impl
type Decoder struct {
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

	logs = d.parsePointsToLogs(points)
	return logs, err
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

// ReplaceInvalidChars analog of invalidChars = regexp.MustCompile("[^a-zA-Z0-9_]")
func ReplaceInvalidChars(in *string) {

	for charIndex, char := range *in {
		charInt := int(char)
		if !((charInt >= 97 && charInt <= 122) || // a-z
			(charInt >= 65 && charInt <= 90) || // A-Z
			(charInt >= 48 && charInt <= 57) || // 0-9
			charInt == 95 || charInt == ':') { // _

			*in = (*in)[:charIndex] + "_" + (*in)[charIndex+1:]
		}
	}
}

func (d *Decoder) parsePointsToLogs(points []models.Point) []*protocol.Log {
	logs := make([]*protocol.Log, 0, len(points))
	for _, s := range points {
		fields, err := s.Fields()
		if err != nil {
			continue
		}
		for field, v := range fields {
			var value float64
			switch v := v.(type) {
			case float64:
				value = v
			case int64:
				value = float64(v)
			case bool:
				if v {
					value = 1
				} else {
					value = 0
				}
			default:
				continue
			}

			var name string
			if field == "value" {
				name = string(s.Name())
			} else {
				name = string(s.Name()) + ":" + field
			}

			ReplaceInvalidChars(&name)
			var builder strings.Builder
			for index, v := range s.Tags() {
				if index != 0 {
					builder.WriteByte('|')
				}
				key := string(v.Key)
				ReplaceInvalidChars(&key)
				builder.WriteString(key)
				builder.WriteString("#$#")
				builder.WriteString(string(v.Value))
			}

			log := &protocol.Log{
				Time: uint32(s.Time().Unix()),
				Contents: []*protocol.Log_Content{
					{
						Key:   metricNameKey,
						Value: name,
					},
					{
						Key:   labelsKey,
						Value: builder.String(),
					},
					{
						Key:   timeNanoKey,
						Value: strconv.FormatInt(s.UnixNano(), 10),
					},
					{
						Key:   valueKey,
						Value: strconv.FormatFloat(value, 'g', -1, 64),
					},
				},
			}
			logs = append(logs, log)

		}
	}
	return logs
}
