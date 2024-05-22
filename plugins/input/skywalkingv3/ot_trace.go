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

	"github.com/alibaba/ilogtail/pkg/protocol"

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

func (ot *OtSpan) ToLog() (*protocol.Log, error) {
	log := &protocol.Log{}
	if ot.End != 0 {
		protocol.SetLogTimeWithNano(log, uint32(ot.End/int64(1000000)), uint32((ot.End*1000)%1e9))
	} else {
		nowTime := time.Now()
		protocol.SetLogTime(log, uint32(nowTime.Unix()))
	}
	log.Contents = make([]*protocol.Log_Content, 0)
	linksJSON, err := json.Marshal(ot.Links)
	if err != nil {
		return nil, errors.Wrap(err, "links cannot marshal to json")
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "links",
		Value: string(linksJSON),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "host",
		Value: ot.Host,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "service",
		Value: ot.Service,
	})
	resourceJSON, err := json.Marshal(ot.Resource)
	if err != nil {
		return nil, errors.Wrap(err, "resource cannot marshal to json")
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "resource",
		Value: string(resourceJSON),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "name",
		Value: ot.Name,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "kind",
		Value: ot.Kind,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "traceID",
		Value: ot.TraceID,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "spanID",
		Value: ot.SpanID,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "parentSpanID",
		Value: ot.ParentSpanID,
	})
	logsJSON, err := json.Marshal(ot.Logs)
	if err != nil {
		return nil, errors.Wrap(err, "logs cannot marshal to json")
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "logs",
		Value: string(logsJSON),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "traceState",
		Value: ot.TraceState,
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "start",
		Value: strconv.FormatInt(ot.Start, 10),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "end",
		Value: strconv.FormatInt(ot.End, 10),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "duration",
		Value: strconv.FormatInt(ot.Duration, 10),
	})
	attrJSON, err := json.Marshal(ot.Attribute)
	if err != nil {
		return nil, errors.Wrap(err, "attribute cannot marshal to json")
	}
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "attribute",
		Value: string(attrJSON),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "statusCode",
		Value: string(ot.StatusCode),
	})
	log.Contents = append(log.Contents, &protocol.Log_Content{
		Key:   "statusMessage",
		Value: ot.StatusMessage,
	})
	return log, nil
}
