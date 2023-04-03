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

package pyroscope

import (
	"context"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/pyroscope-io/pyroscope/pkg/storage/segment"
	"github.com/pyroscope-io/pyroscope/pkg/util/attime"

	"github.com/alibaba/ilogtail/pkg/helper/profile"
	"github.com/alibaba/ilogtail/pkg/helper/profile/pyroscope/jfr"
	"github.com/alibaba/ilogtail/pkg/helper/profile/pyroscope/pprof"
	"github.com/alibaba/ilogtail/pkg/helper/profile/pyroscope/raw"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/pkg/protocol/decoder/common"
)

const AlarmType = "PYROSCOPE_ALARM"

type Decoder struct {
}

func (d *Decoder) DecodeV2(data []byte, req *http.Request) (groups []*models.PipelineGroupEvents, err error) {
	// do nothing
	return nil, nil
}

func (d *Decoder) Decode(data []byte, req *http.Request, tags map[string]string) (logs []*protocol.Log, err error) {
	in, err := d.extractRawInput(data, req)
	if err != nil {
		return nil, err
	}
	return in.Profile.Parse(context.Background(), &in.Metadata, tags)
}

func (d *Decoder) extractRawInput(data []byte, req *http.Request) (*profile.Input, error) {
	in, ft, err := d.parseInputMeta(req)
	if err != nil {
		return nil, err
	}
	ct := req.Header.Get("Content-Type")
	var category string
	switch {
	case ft == profile.FormatPprof:
		in.Profile = pprof.NewRawProfile(data, "")
		category = "pprof"
	case ft == profile.FormatJFR:
		in.Profile = jfr.NewRawProfile(data, ct)
		category = "JFR"
	case strings.Contains(ct, "multipart/form-data"):
		in.Profile = pprof.NewRawProfile(data, ct)
		category = "pprof"
	case ft == profile.FormatTrie, ct == "binary/octet-stream+trie":
		in.Profile = raw.NewRawProfile(data, profile.FormatTrie)
		category = "tire"
	default:
		in.Profile = raw.NewRawProfile(data, profile.FormatGroups)
		category = "groups"
	}
	if logger.DebugFlag() {
		var h string
		for k, v := range req.Header {
			h += "key: " + k + " val: " + strings.Join(v, ",")
		}
		logger.Debug(context.Background(), "CATEGORY", category, "URL", req.URL.Query().Encode(), "Header", h)
	}
	return in, nil
}

func (d *Decoder) ParseRequest(res http.ResponseWriter, req *http.Request, maxBodySize int64) (data []byte, statusCode int, err error) {
	return common.CollectBody(res, req, maxBodySize)
}

func (d *Decoder) parseInputMeta(req *http.Request) (*profile.Input, profile.Format, error) {
	var input profile.Input
	q := req.URL.Query()

	n := q.Get("name")
	key, err := segment.ParseKey(n)
	if err != nil {
		logger.Error(context.Background(), AlarmType, "invalid name", n)
		return nil, "", fmt.Errorf("pyroscope protocol get name err: %w", err)
	}
	name := key.AppName()
	if strings.HasSuffix(name, ".cpu") {
		key.Add("__name__", name[:len(name)-4])
	}
	input.Metadata.Tags = key.Labels()

	if f := q.Get("from"); f != "" {
		input.Metadata.StartTime = attime.Parse(f)
	}
	if input.Metadata.StartTime.IsZero() {
		input.Metadata.StartTime = time.Now()
	}

	if f := q.Get("until"); f != "" {
		input.Metadata.EndTime = attime.Parse(f)
	}
	if input.Metadata.StartTime.IsZero() {
		input.Metadata.EndTime = time.Now()
	}

	input.Metadata.SampleRate = 100
	if sr := q.Get("sampleRate"); sr != "" {
		sampleRate, err := strconv.Atoi(sr)
		if err != nil {
			logger.Error(context.Background(), AlarmType, "invalid sampleRate", sr)
		} else {
			input.Metadata.SampleRate = uint32(sampleRate)
		}
	}

	if sn := q.Get("spyName"); sn != "" {
		sn = strings.TrimPrefix(sn, "pyroscope-")
		sn = strings.TrimSuffix(sn, "spy")
		input.Metadata.SpyName = sn
	} else {
		input.Metadata.SpyName = "unknown"
	}

	if u := q.Get("units"); u != "" {
		input.Metadata.Units = profile.Units(u)
	} else {
		input.Metadata.Units = profile.SamplesUnits
	}

	if at := q.Get("aggregationType"); at != "" {
		input.Metadata.AggregationType = profile.AggType(at)
	} else {
		input.Metadata.AggregationType = profile.SumAggType
	}

	format := q.Get("format")
	return &input, profile.Format(format), nil
}
