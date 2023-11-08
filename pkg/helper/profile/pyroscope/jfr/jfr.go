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

package jfr

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"mime/multipart"
	"strconv"
	"strings"

	"github.com/pyroscope-io/pyroscope/pkg/util/form"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/protocol"
)

type RawProfile struct {
	RawData             []byte
	FormDataContentType string

	logs []*protocol.Log // v1 result
}

func NewRawProfile(data []byte, format string) *RawProfile {
	return &RawProfile{
		RawData:             data,
		FormDataContentType: format,
	}
}

func (r *RawProfile) Parse(ctx context.Context, meta *profile.Meta, tags map[string]string) (logs []*protocol.Log, err error) {
	reader, labels, err := r.extractProfileRaw()
	if err != nil {
		return nil, err
	}
	if err := r.ParseJFR(ctx, meta, reader, labels, r.extractProfileV1(meta, tags)); err != nil {
		return nil, err
	}
	logs = r.logs
	r.logs = nil
	return
}

func (r *RawProfile) extractProfileV1(meta *profile.Meta, tags map[string]string) profile.CallbackFunc {
	profileID := profile.GetProfileID(meta)
	return func(id uint64, stack *profile.Stack, vals []uint64, types, units, aggs []string, startTime, endTime int64, labels map[string]string) {
		var content []*protocol.Log_Content
		for k, v := range tags {
			labels[k] = v
		}
		b, _ := json.Marshal(labels)
		content = append(content,
			&protocol.Log_Content{
				Key:   "name",
				Value: stack.Name,
			},
			&protocol.Log_Content{
				Key:   "stack",
				Value: strings.Join(stack.Stack, "\n"),
			},
			&protocol.Log_Content{
				Key:   "stackID",
				Value: strconv.FormatUint(id, 16),
			},
			&protocol.Log_Content{
				Key:   "language",
				Value: meta.SpyName,
			},
			&protocol.Log_Content{
				Key:   "dataType",
				Value: "CallStack",
			},
			&protocol.Log_Content{
				Key:   "durationNs",
				Value: strconv.FormatInt(endTime-startTime, 10),
			},

			&protocol.Log_Content{
				Key:   "profileID",
				Value: profileID,
			},
			&protocol.Log_Content{
				Key:   "labels",
				Value: string(b),
			},
		)
		for i, v := range vals {
			var res []*protocol.Log_Content
			if i != len(vals)-1 {
				res = make([]*protocol.Log_Content, len(content))
				copy(res, content)
			} else {
				res = content
			}
			res = append(res,
				&protocol.Log_Content{
					Key:   "units",
					Value: units[i],
				},
				&protocol.Log_Content{
					Key:   "type",
					Value: profile.DetectProfileType(types[i]).Kind,
				},
				&protocol.Log_Content{
					Key:   "valueTypes",
					Value: types[i],
				},
				&protocol.Log_Content{
					Key:   "aggTypes",
					Value: aggs[i],
				},
				&protocol.Log_Content{
					Key:   "val",
					Value: strconv.FormatFloat(float64(v), 'f', 2, 64),
				},
			)
			log := &protocol.Log{
				Contents: res,
			}
			protocol.SetLogTimeWithNano(log, uint32(startTime/1e9), uint32(startTime%1e9))
			r.logs = append(r.logs, log)
		}
	}
}

func (r *RawProfile) extractProfileRaw() (io.Reader, *LabelsSnapshot, error) {
	var reader io.Reader = bytes.NewReader(r.RawData)
	var err error
	labels := new(LabelsSnapshot)
	if strings.Contains(r.FormDataContentType, "multipart/form-data") {
		if reader, labels, err = loadJFRFromForm(reader, r.FormDataContentType); err != nil {
			return nil, nil, err
		}
	}
	return reader, labels, err
}

func loadJFRFromForm(r io.Reader, contentType string) (io.Reader, *LabelsSnapshot, error) {
	boundary, err := form.ParseBoundary(contentType)
	if err != nil {
		return nil, nil, err
	}

	f, err := multipart.NewReader(r, boundary).ReadForm(32 << 20)
	if err != nil {
		return nil, nil, err
	}
	defer func() {
		_ = f.RemoveAll()
	}()

	jfrField, err := form.ReadField(f, "jfr")
	if err != nil {
		return nil, nil, err
	}
	if jfrField == nil {
		return nil, nil, fmt.Errorf("jfr field is required")
	}

	labelsField, err := form.ReadField(f, "labels")
	if err != nil {
		return nil, nil, err
	}
	var labels LabelsSnapshot
	if len(labelsField) > 0 {
		if err = proto.Unmarshal(labelsField, &labels); err != nil {
			return nil, nil, err
		}
	}

	return bytes.NewReader(jfrField), &labels, nil
}
