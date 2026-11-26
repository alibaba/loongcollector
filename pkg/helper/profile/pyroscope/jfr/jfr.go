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
	"time"

	"github.com/pyroscope-io/pyroscope/pkg/util/form"
	"google.golang.org/protobuf/proto"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/models"
)

type RawProfile struct {
	RawData             []byte
	FormDataContentType string
}

func NewRawProfile(data []byte, format string) *RawProfile {
	return &RawProfile{
		RawData:             data,
		FormDataContentType: format,
	}
}

func (r *RawProfile) Parse(ctx context.Context, meta *profile.Meta, tags map[string]string) ([]models.PipelineEvent, error) {
	reader, labels, err := r.extractProfileRaw()
	if err != nil {
		return nil, err
	}
	var events []models.PipelineEvent
	if err := r.ParseJFR(ctx, meta, reader, labels, profileLogCallback(meta, tags, &events)); err != nil {
		return nil, err
	}
	return events, nil
}

func profileLogCallback(meta *profile.Meta, tags map[string]string, events *[]models.PipelineEvent) profile.CallbackFunc {
	profileID := profile.GetProfileID(meta)
	return func(id uint64, stack *profile.Stack, vals []uint64, types, units, aggs []string, startTime, endTime int64, labels map[string]string) {
		for key, value := range tags {
			labels[key] = value
		}
		encodedLabels, _ := json.Marshal(labels)
		labelText := string(encodedLabels)
		timestamp := profile.LogTimestamp(startTime/int64(time.Second), startTime%int64(time.Second))
		stackTrace := strings.Join(stack.Stack, "\n")
		stackID := strconv.FormatUint(id, 16)
		duration := strconv.FormatInt(endTime-startTime, 10)
		for i, value := range vals {
			log := models.NewLogWithOrderedContents(13)
			log.Timestamp = timestamp
			log.Contents.Add("name", stack.Name)
			log.Contents.Add("stack", stackTrace)
			log.Contents.Add("stackID", stackID)
			log.Contents.Add("language", meta.SpyName)
			log.Contents.Add("dataType", "CallStack")
			log.Contents.Add("durationNs", duration)
			log.Contents.Add("profileID", profileID)
			log.Contents.Add("labels", labelText)
			log.Contents.Add("units", units[i])
			log.Contents.Add("type", profile.DetectProfileType(types[i]).Kind)
			log.Contents.Add("valueTypes", types[i])
			log.Contents.Add("aggTypes", aggs[i])
			log.Contents.Add("val", strconv.FormatFloat(float64(value), 'f', 2, 64))
			*events = append(*events, log)
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
