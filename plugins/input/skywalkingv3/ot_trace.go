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

package skywalkingv3

import (
	"encoding/json"
	"strconv"
	"time"

	"github.com/alibaba/ilogtail/pkg/models"

	"github.com/pkg/errors"
)

type StatusCode string

const (
	StatusCodeOk    StatusCode = "OK"
	StatusCodeError StatusCode = "ERROR"
)

type OtSpan struct {
	Host          string              `json:"host"`
	Service       string              `json:"service"`
	Resource      map[string]string   `json:"resource,omitempty"`
	Name          string              `json:"name"`
	Kind          string              `json:"kind"`
	TraceID       string              `json:"traceID"`
	SpanID        string              `json:"spanID"`
	ParentSpanID  string              `json:"parentSpanID"`
	Links         []*OtSpanRef        `json:"links,omitempty"`
	Logs          []map[string]string `json:"logs,omitempty"`
	TraceState    string              `json:"traceState"`
	Start         int64               `json:"start"`
	End           int64               `json:"end"`
	Duration      int64               `json:"duration"`
	Attribute     map[string]string   `json:"attribute,omitempty"`
	StatusCode    StatusCode          `json:"statusCode"`
	StatusMessage string              `json:"statusMessage,omitempty"`
	Raw           string              `json:"raw,omitempty"`
}

func NewOtSpan() *OtSpan {
	return &OtSpan{
		Resource:  make(map[string]string),
		Attribute: make(map[string]string),
	}
}

type OtSpanRef struct {
	TraceID    string              `json:"traceID"`
	SpanID     string              `json:"spanID"`
	TraceState string              `json:"traceState"`
	Attributes []map[string]string `json:"attributes"`
}

func (ot *OtSpan) ToLog() (*models.Log, error) {
	timestamp := time.UnixMicro(ot.End)
	if ot.End == 0 {
		timestamp = time.Now().Truncate(time.Second)
	} else if timestamp.Unix() == 0 {
		timestamp = time.Now()
	}
	log := &models.Log{
		Contents:  models.NewOrderedLogContents(17),
		Tags:      models.NewTags(),
		Timestamp: uint64(timestamp.UnixNano()),
	}
	linksJSON, err := json.Marshal(ot.Links)
	if err != nil {
		return nil, errors.Wrap(err, "links cannot marshal to json")
	}
	log.Contents.Add("links", string(linksJSON))
	log.Contents.Add("host", ot.Host)
	log.Contents.Add("service", ot.Service)
	resourceJSON, err := json.Marshal(ot.Resource)
	if err != nil {
		return nil, errors.Wrap(err, "resource cannot marshal to json")
	}
	log.Contents.Add("resource", string(resourceJSON))
	log.Contents.Add("name", ot.Name)
	log.Contents.Add("kind", ot.Kind)
	log.Contents.Add("traceID", ot.TraceID)
	log.Contents.Add("spanID", ot.SpanID)
	log.Contents.Add("parentSpanID", ot.ParentSpanID)
	logsJSON, err := json.Marshal(ot.Logs)
	if err != nil {
		return nil, errors.Wrap(err, "logs cannot marshal to json")
	}
	log.Contents.Add("logs", string(logsJSON))
	log.Contents.Add("traceState", ot.TraceState)
	log.Contents.Add("start", strconv.FormatInt(ot.Start, 10))
	log.Contents.Add("end", strconv.FormatInt(ot.End, 10))
	log.Contents.Add("duration", strconv.FormatInt(ot.Duration, 10))
	attrJSON, err := json.Marshal(ot.Attribute)
	if err != nil {
		return nil, errors.Wrap(err, "attribute cannot marshal to json")
	}
	log.Contents.Add("attribute", string(attrJSON))
	log.Contents.Add("statusCode", string(ot.StatusCode))
	log.Contents.Add("statusMessage", ot.StatusMessage)
	return log, nil
}
